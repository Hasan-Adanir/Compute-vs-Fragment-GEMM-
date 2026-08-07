/* ===========================================================================
 * frag_naive -- C = A * B'yi FRAGMENT SHADER ile hesaplar.
 *
 * Fikir: matrisleri dokuya koy, N x N'lik bir framebuffer'a tam ekran ucgen
 * ciz. Rasterizer her hedef texel icin bir fragment uretir; her fragment kendi
 * (i, j) hucresinin ic carpimini hesaplar. Ekrana hicbir sey cizilmez, sonuc
 * framebuffer'dan geri okunur.
 *
 * Esleme:
 *   rasterizer -> is dagitici      fragment -> thread
 *   doku       -> read-only buffer FBO      -> cikti buffer'i
 * ===========================================================================*/

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Matrisi tek kanalli 32-bit float dokuya yukler.
 * texel(x, y) = m->data[y * cols + x] olur. */
static GLuint upload_matrix(const Mat *m)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    /* texelFetch filtreleme yapmaz ama eksik mipmap zinciri dokuyu "incomplete"
     * gosterebilir; NEAREST + CLAMP bunu bastan engeller. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m->cols, m->rows, 0,
                 GL_RED, GL_FLOAT, m->data);
    return tex;
}

int main(int argc, char **argv)
{
    int  N       = 512;
    bool verify  = true;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--noverify") == 0) verify = false;   /* buyuk N icin */
        else { int v = atoi(argv[i]); if (v >= 16) N = v; }
    }

    if (!gl_init()) return 1;

    printf("\n=== frag_naive (fragment shader GEMM) ===\n");
    gl_print_info();
    printf("N        : %d  (%d x %d matrisler)\n\n", N, N, N);

    /* ---------------------------------------------------------- Matrisleri doldurma ----*/
    Mat A = mat_alloc(N, N);
    Mat B = mat_alloc(N, N);
    Mat C_gpu = mat_alloc(N, N);
    mat_fill_random(&A, 12345u);
    mat_fill_random(&B, 67890u);

    /* ------------------------------------------------- CPU temel cizgi ----*/
    double t0 = mm_now_ms();
    mat_mul_cpu(&A, &B, &C_gpu);        /* tamponu gecici olarak kullaniyoruz */
    double cpu_ms = mm_now_ms() - t0;
    printf("CPU (blocked fp32) : %8.2f ms   %7.2f GFLOP/s\n",
           cpu_ms, mm_gflops(N, cpu_ms));

    /* ------------------------------------------------------- GL kaynak ----*/
    GLuint texA = upload_matrix(&A);
    GLuint texB = upload_matrix(&B);

    /* Cikti: N x N, tek kanalli float doku; FBO'ya renk eki olarak baglanir. */
    GLuint texC;
    glGenTextures(1, &texC);
    glBindTexture(GL_TEXTURE_2D, texC);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, N, N, 0, GL_RED, GL_FLOAT, NULL);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texC, 0);
    GLenum draw_buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &draw_buf);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO tamamlanamadi (R32F renk eki desteklenmiyor olabilir)\n");
        return 1;
    }

    /* Core profile'da VAO bagli olmadan cizim yapilamaz. Vertex verisi yok,
     * bos bir VAO yeterli. */
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    // array object - buffer object farkı 

    GLuint prog = gl_program_graphics("fullscreen.vert", "gemm.frag");
    if (!prog) return 1;

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uA"), 0);   /* doku birimi 0 */
    glUniform1i(glGetUniformLocation(prog, "uB"), 1);   /* doku birimi 1 */
    glUniform1i(glGetUniformLocation(prog, "uK"), N);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texA);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texB);

    /* Viewport hedef dokuyu birebir kaplar: N*N fragment, hucre basina bir tane. */
    glViewport(0, 0, N, N);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);          /* karistirma acik kalirsa sonuc bozulur */

    /* ---------------------------------------------------------- olcum ----*/
    double samples[MM_RUNS];
    for (int r = 0; r < MM_WARMUP + MM_RUNS; ++r) {
        gl_timer_begin();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        double ms = gl_timer_end_ms();
        if (r >= MM_WARMUP) samples[r - MM_WARMUP] = ms;
    }
    double gpu_ms = mm_median(samples, MM_RUNS);

    printf("GPU (fragment)     : %8.2f ms   %7.2f GFLOP/s   [%d kosunun medyani]\n",
           gpu_ms, mm_gflops(N, gpu_ms), MM_RUNS);
    printf("hizlanma           : %8.2fx  (CPU blocked'a gore)\n\n", cpu_ms / gpu_ms);

    /* ------------------------------------------------------ dogrulama ----*/
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, N, N, GL_RED, GL_FLOAT, C_gpu.data);

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

    mat_free(&A); mat_free(&B); mat_free(&C_gpu);
    gl_shutdown();
    return 0;
}
