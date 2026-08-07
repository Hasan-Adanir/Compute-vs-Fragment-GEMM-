/* ===========================================================================
 * comp_naive -- C = A * B'yi COMPUTE SHADER ile hesaplar.
 *
 * frag_naive.c ile ayni algoritma, farkli pipeline. Karsilastirma bu iki
 * dosya arasinda yapilir:
 *
 *   frag_naive                      comp_naive
 *   ----------                      ----------
 *   veri dokuda (texelFetch)        veri SSBO'da (duz dizi indisi)
 *   is dagitimi rasterizer'dan      is dagitimi glDispatchCompute'tan
 *   cikti FBO renk eki              cikti SSBO, istenen yere yazilabilir
 *   tam ekran ucgen ciziliyor       cizim yok
 *
 * ===========================================================================*/

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Matrisi shader storage buffer'a yukler ve verilen binding noktasina baglar. */
static GLuint upload_ssbo(GLuint binding, const float *data, size_t bytes)
{
    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buf);
    return buf;
}

int main(int argc, char **argv)
{
    int  N      = 512;
    bool verify = true;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--noverify") == 0) verify = false;
        else { int v = atoi(argv[i]); if (v >= 16) N = v; }
    }

    if (!gl_init()) return 1;

    printf("\n=== comp_naive (compute shader GEMM) ===\n");
    gl_print_info();

    if (!gl_has_compute()) {
        printf("\nBu makinede compute shader yok: OpenGL 4.3 gerekiyor.\n"
               "macOS 4.1'de tavan yapar. Bu olcumu GL 4.3+ destekleyen bir\n"
               "makinede (Windows / Linux) alin. frag_naive burada calisir.\n");
        gl_shutdown();
        return 2;
    }

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
    size_t bytes = (size_t)N * N * sizeof(float);
    GLuint bufA = upload_ssbo(0, A.data, bytes);
    GLuint bufB = upload_ssbo(1, B.data, bytes);
    GLuint bufC = upload_ssbo(2, NULL,   bytes);

    GLuint prog = gl_program_compute("gemm.comp");
    if (!prog) return 1;

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uM"), N);
    glUniform1i(glGetUniformLocation(prog, "uN"), N);
    glUniform1i(glGetUniformLocation(prog, "uK"), N);

    /* Shader'daki local_size ile ayni olmali. N 16'nin kati degilse yukari
     * yuvarlanir; tasan invocation'lari shader icindeki sinir kontrolu eler. */
    const int LOCAL = 16;
    GLuint groups = (GLuint)((N + LOCAL - 1) / LOCAL);

    /* ---------------------------------------------------------- olcum ----*/
    double samples[MM_RUNS];
    for (int r = 0; r < MM_WARMUP + MM_RUNS; ++r) {
        gl_timer_begin();
        glDispatchCompute(groups, groups, 1);
        double ms = gl_timer_end_ms();
        if (r >= MM_WARMUP) samples[r - MM_WARMUP] = ms;
    }
    double gpu_ms = mm_median(samples, MM_RUNS);

    printf("GPU (compute)      : %8.2f ms   %7.2f GFLOP/s   [%d kosunun medyani]\n",
           gpu_ms, mm_gflops(N, gpu_ms), MM_RUNS);
    printf("hizlanma           : %8.2fx  (CPU blocked'a gore)\n\n", cpu_ms / gpu_ms);

    /* ------------------------------------------------------ dogrulama ----*/
    /* Yazmalar geri okumadan once gorunur olmali. */
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufC);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)bytes, C_gpu.data);

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

    glDeleteBuffers(1, &bufA);
    glDeleteBuffers(1, &bufB);
    glDeleteBuffers(1, &bufC);
    mat_free(&A); mat_free(&B); mat_free(&C_gpu);
    gl_shutdown();
    return 0;
}
