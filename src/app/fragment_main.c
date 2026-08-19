/* 
  frag -- C = A * B'yi FRAGMENT SHADER ile hesaplar.
    comp.c                             frag.c
    ------                             ------
    glGenBuffers + glBufferData        My_glCreateBuffer
    glBindBufferBase                   My_glBindBufferBase
    gl_program_compute("gemm.comp")    My_glCreateComputeProgram("gemm.frag")
    layout(local_size_x = 16, ...)     My_glProgramLocalSize(16, 16)
    glDispatchCompute(gx, gy, 1)       My_glDispatchCompute(gx, gy, 1)
    glMemoryBarrier(...)               My_glMemoryBarrier(...)
    glGetBufferSubData(...)            My_glGetBufferSubData(...)
 
 
  SSBO yerine texture, dispatch
  yerine tam ekran ucgen, cikti yerine FBO.
 */

#include "fragment_compute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int  N      = 512;
    bool verify = true;
    bool show   = false;
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--noverify") == 0) verify = false;
        else if (strcmp(argv[i], "--print")    == 0) show   = true;   /* matrisleri bas */
        else { int v = atoi(argv[i]); if (v >= 2) N = v; }
    }

    if (!gl_init()) return 1;

    printf("\n=== frag (fragment shader GEMM) ===\n");
    gl_print_info();
    printf("N        : %d  (%d x %d matrisler)\n\n", N, N, N);

    /* ---------------------------------------------------------- veri ----*/
    Mat A = mat_alloc(N, N);
    Mat B = mat_alloc(N, N);
    Mat C_gpu = mat_alloc(N, N);
    mat_fill_random(&A, 12345u);
    mat_fill_random(&B, 67890u);

    /* ------------------------------------------------- CPU temel cizgi ----*/
    double t0 = mm_now_ms();
    mat_mul_cpu(&A, &B, &C_gpu);
    double cpu_ms = mm_now_ms() - t0;
    printf("CPU (blocked fp32) : %8.2f ms   %7.2f GFLOP/s\n",
           cpu_ms, mm_gflops(N, cpu_ms));

    /* ------------------------------------------------------- GL kaynak ----*/
    My_glInit();   /* tam ekran ucgenin VBO'su; compute yolunda karsiligi yok */

    MyBuffer bufA = My_glCreateBuffer(N, N, A.data);
    MyBuffer bufB = My_glCreateBuffer(N, N, B.data);
    MyBuffer bufC = My_glCreateBuffer(N, N, NULL);
    My_glBindBufferBase(0, &bufA);
    My_glBindBufferBase(1, &bufB);
    My_glBindBufferBase(2, &bufC);

    GLuint prog = My_glCreateComputeProgram("fragment/matrix_multiply.frag");
    if (!prog) return 1;

    glUseProgram(prog);
    /* GLES 2.0'da indisleri float tutuyoruz; comp.c'de bunlar glUniform1i. */
    glUniform1f(glGetUniformLocation(prog, "uM"), (float)N);
    glUniform1f(glGetUniformLocation(prog, "uN"), (float)N);
    glUniform1f(glGetUniformLocation(prog, "uK"), (float)N);

    /* Compute'ta bu bilgi shader'in icindeydi (layout(local_size_x = 16,
     * local_size_y = 16)); fragment yolunda viewport'u belirledigi icin C
     * tarafinda soyleniyor. N 16'nin kati degilse yukari yuvarlanir, tasan
     * fragment'lari shader'daki discard eler. */
    const int LOCAL = 16;
    My_glProgramLocalSize(LOCAL, LOCAL);
    GLuint groups = (GLuint)((N + LOCAL - 1) / LOCAL);

    /* ---------------------------------------------------------- olcum ----*/
    double samples[MM_RUNS];
    for (int r = 0; r < MM_WARMUP + MM_RUNS; ++r) {
        gl_timer_begin();
        My_glDispatchCompute(groups, groups, 1);
        double ms = gl_timer_end_ms();
        if (r >= MM_WARMUP) samples[r - MM_WARMUP] = ms;
    }
    double gpu_ms = mm_median(samples, MM_RUNS);

    printf("GPU (fragment)     : %8.2f ms   %7.2f GFLOP/s   [%d kosunun medyani]\n",
           gpu_ms, mm_gflops(N, gpu_ms), MM_RUNS);
    printf("hizlanma           : %8.2fx  (CPU blocked'a gore)\n\n", cpu_ms / gpu_ms);

    /* ------------------------------------------------------ dogrulama ----*/
    /* Yazmalar geri okumadan once gorunur olmali. */
    My_glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    My_glGetBufferSubData(&bufC, C_gpu.data);

    if (show) {
        printf("\n");
        mat_print("A", &A, 8);
        mat_print("B", &B, 8);
        mat_print("C = A * B  (GPU sonucu)", &C_gpu, 8);
    }

    if (verify) {
        Mat C_ref = mat_alloc(N, N);
        printf("fp64 referans hesaplaniyor (N=%d icin biraz surebilir)...\n", N);
        mat_mul_reference(&A, &B, &C_ref);
        mat_check(&C_ref, &C_gpu);
        mat_free(&C_ref);
    } else {
        printf("  dogrulama atlandi (--noverify)\n");
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) fprintf(stderr, "[GL hatasi] 0x%04X\n", err);

    My_glDeleteBuffer(&bufA);
    My_glDeleteBuffer(&bufB);
    My_glDeleteBuffer(&bufC);
    mat_free(&A); mat_free(&B); mat_free(&C_gpu);
    gl_shutdown();
    return 0;
}
