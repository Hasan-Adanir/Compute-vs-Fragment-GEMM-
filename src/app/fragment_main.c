/* Fragment shader'lari genel amacli matris cekirdekleri gibi calistiran
 * uygulama. Varsayilan olarak tum islemleri olcer; islem adi verilirse
 * yalnizca secilen cekirdegi calistirir. */

#include "fragment_compute.h"
#include "matrix_operation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MatrixOperation operation;
    double cpu_ms;
    double gpu_ms;
} OperationResult;

static bool run_operation(MatrixOperation operation, int n, bool verify,
                          bool show, const Mat *A, const Mat *B, Mat *C_gpu,
                          MyBuffer *output, GLuint groups,
                          OperationResult *result)  
{
    double start = mm_now_ms();
    matrix_operation_cpu(operation, A, B, C_gpu);
    result->cpu_ms = mm_now_ms() - start;

    GLuint program = My_glCreateComputeProgram(
        matrix_operation_fragment_shader(operation));
    if (!program) return false;

    MyGLStateSnapshot state = {0};
    if (!My_glBeginStateScope(&state)) {
        fprintf(stderr, "Fragment OpenGL state'i kaydedilemedi.\n");
        My_glEndStateScope(&state);
        glDeleteProgram(program);
        return false;
    }

    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "uM"), (float)n);
    glUniform1f(glGetUniformLocation(program, "uN"), (float)n);
    glUniform1f(glGetUniformLocation(program, "uK"), (float)n);

    double samples[MM_RUNS];
    for (int run = 0; run < MM_WARMUP + MM_RUNS; ++run) {
        gl_timer_begin();
        My_glDispatchCompute(groups, groups, 1);
        double elapsed_ms = gl_timer_end_ms();
        if (run >= MM_WARMUP) samples[run - MM_WARMUP] = elapsed_ms;
    }
    result->operation = operation;
    result->gpu_ms = mm_median(samples, MM_RUNS);

    if (verify || show) {
        My_glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        My_glGetBufferSubData(output, C_gpu->data);
    }

    printf("\n[%s]\n", matrix_operation_name(operation));
    printf("  CPU            : %10.4f ms\n", result->cpu_ms);
    printf("  GPU fragment   : %10.4f ms  (%d kosunun medyani)\n",
           result->gpu_ms, MM_RUNS);

    if (show) mat_print("C (GPU sonucu)", C_gpu, 8);

    bool valid = true;
    if (verify) {
        Mat reference = mat_alloc(n, n);
        matrix_operation_reference(operation, A, B, &reference);
        valid = mat_check(&reference, C_gpu);
        mat_free(&reference);
    } else {
        printf("  dogrulama atlandi (--noverify)\n");
    }

    /* Program, framebuffer, viewport, texture unit'leri, vertex input ve
     * pipeline acik/kapali state'leri bu noktada eski haline getirilir. */
    My_glEndStateScope(&state);
    glDeleteProgram(program);
    return valid;
}

static void print_summary(const OperationResult *results, int count)
{
    double total_cpu_ms = 0.0;
    double total_gpu_ms = 0.0;

    printf("\n%-12s %14s %18s\n", "islem", "CPU (ms)", "GPU fragment (ms)");
    printf("------------------------------------------------\n");
    for (int i = 0; i < count; ++i) {
        printf("%-12s %14.4f %18.4f\n",
               matrix_operation_name(results[i].operation),
               results[i].cpu_ms, results[i].gpu_ms);
        total_cpu_ms += results[i].cpu_ms;
        total_gpu_ms += results[i].gpu_ms;
    }
    printf("------------------------------------------------\n");
    printf("%-12s %14.4f %18.4f\n", "TOPLAM", total_cpu_ms, total_gpu_ms);
    printf("\nNot: GPU toplam, cekirdek surelerinin toplamidir; shader derleme,\n"
           "texture yukleme ve geri okuma bu sureye dahil degildir.\n");
}

int main(int argc, char **argv)
{
    int n = 512;
    bool verify = true;
    bool show = false;
    bool run_all = true;
    MatrixOperation selected = MATRIX_OP_MULTIPLY;

    for (int i = 1; i < argc; ++i) {
        MatrixOperation parsed;
        if (strcmp(argv[i], "--noverify") == 0) verify = false;
        else if (strcmp(argv[i], "--print") == 0) show = true;
        else if (strcmp(argv[i], "all") == 0) run_all = true;
        else if (matrix_operation_parse(argv[i], &parsed)) {
            selected = parsed;
            run_all = false;
        } else {
            int value = atoi(argv[i]);
            if (value >= 2) n = value;
            else {
                fprintf(stderr, "Bilinmeyen arguman: %s\n", argv[i]);
                return 1;
            }
        }
    }

    if (!gl_init()) return 1;

    printf("\n=== fragment shader matrix operations ===\n");
    gl_print_info();
    printf("N        : %d  (%d x %d matrisler)\n", n, n, n);
    printf("mod      : %s\n", run_all ? "tum islemler" : matrix_operation_name(selected));

    Mat A = mat_alloc(n, n);
    Mat B = mat_alloc(n, n);
    Mat C_gpu = mat_alloc(n, n);
    mat_fill_random(&A, 12345u);
    mat_fill_random(&B, 67890u);
    /* Tum islemler ayni A ve B matrislerini kullanir. Bolmenin tanimli ve
     * sayisal olarak kararli kalmasi icin kucuk bolenler bir kez duzeltilir. */
    if (run_all || selected == MATRIX_OP_DIVIDE)
        matrix_operation_prepare_inputs(MATRIX_OP_DIVIDE, &B);

    if (show) {
        mat_print("A", &A, 8);
        mat_print("B", &B, 8);
    }

    My_glInit();
    MyBuffer input_a = My_glCreateBuffer(n, n, A.data);
    MyBuffer input_b = My_glCreateBuffer(n, n, B.data);
    MyBuffer output = My_glCreateBuffer(n, n, NULL);
    My_glBindBufferBase(0, &input_a);
    My_glBindBufferBase(1, &input_b);
    My_glBindBufferBase(2, &output);

    const int local_size = 16;
    My_glProgramLocalSize(local_size, local_size);
    GLuint groups = (GLuint)((n + local_size - 1) / local_size);

    static const MatrixOperation all_operations[] = {
        MATRIX_OP_MULTIPLY,
        MATRIX_OP_ADD,
        MATRIX_OP_SUBTRACT,
        MATRIX_OP_DIVIDE
    };
    OperationResult results[4];
    int result_count = 0;
    bool success = true;

    if (run_all) {
        for (int i = 0; i < 4; ++i) {
            if (!run_operation(all_operations[i], n, verify, show, &A, &B,
                               &C_gpu, &output, groups,
                               &results[result_count])) {
                success = false;
                break;
            }
            ++result_count;
        }
    } else {
        success = run_operation(selected, n, verify, show, &A, &B, &C_gpu,
                                &output, groups, &results[0]);
        result_count = success ? 1 : 0;
    }

    if (result_count > 0) print_summary(results, result_count);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "[GL hatasi] 0x%04X\n", error);
        success = false;
    }

    My_glDeleteBuffer(&input_a);
    My_glDeleteBuffer(&input_b);
    My_glDeleteBuffer(&output);
    mat_free(&A);
    mat_free(&B);
    mat_free(&C_gpu);
    gl_shutdown();
    return success ? 0 : 1;
}
