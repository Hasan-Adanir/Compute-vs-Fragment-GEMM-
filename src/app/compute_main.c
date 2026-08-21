/* Runs compute-shader matrix benchmarks. */

#include "platform_support.h"
#include "matrix_operation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MatrixOperation operation;
    double cpu_ms;
    double gpu_ms;
} OperationResult;

/* Uploads data to an SSBO. */
static GLuint upload_ssbo(GLuint binding, const float *data, size_t bytes)
{
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
    return buffer;
}

/* Benchmarks one compute operation. */
static bool run_operation(MatrixOperation operation, int n, bool verify,
                          bool show, const Mat *A, const Mat *B, Mat *C_gpu,
                          GLuint output_buffer, size_t bytes, GLuint groups,
                          OperationResult *result)
{
    double start = mm_now_ms();
    matrix_operation_cpu(operation, A, B, C_gpu);
    result->cpu_ms = mm_now_ms() - start;

    GLuint program = gl_program_compute(
        matrix_operation_compute_shader(operation));
    if (!program) return false;

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uM"), n);
    glUniform1i(glGetUniformLocation(program, "uN"), n);
    glUniform1i(glGetUniformLocation(program, "uK"), n);

    double samples[MM_RUNS];
    for (int run = 0; run < MM_WARMUP + MM_RUNS; ++run) {
        gl_timer_begin();
        glDispatchCompute(groups, groups, 1);
        double elapsed_ms = gl_timer_end_ms();
        if (run >= MM_WARMUP) samples[run - MM_WARMUP] = elapsed_ms;
    }
    result->operation = operation;
    result->gpu_ms = mm_median(samples, MM_RUNS);

    if (verify || show) {
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, output_buffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)bytes,
                           C_gpu->data);
    }

    printf("\n[%s]\n", matrix_operation_name(operation));
    printf("  CPU            : %10.4f ms\n", result->cpu_ms);
    printf("  GPU compute    : %10.4f ms  (%d kosunun medyani)\n",
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

    glDeleteProgram(program);
    return valid;
}

/* Prints benchmark results. */
static void print_summary(const OperationResult *results, int count)
{
    double total_cpu_ms = 0.0;
    double total_gpu_ms = 0.0;

    printf("\n%-12s %14s %17s\n", "islem", "CPU (ms)", "GPU compute (ms)");
    printf("-----------------------------------------------\n");
    for (int i = 0; i < count; ++i) {
        printf("%-12s %14.4f %17.4f\n",
               matrix_operation_name(results[i].operation),
               results[i].cpu_ms, results[i].gpu_ms);
        total_cpu_ms += results[i].cpu_ms;
        total_gpu_ms += results[i].gpu_ms;
    }
    printf("-----------------------------------------------\n");
    printf("%-12s %14.4f %17.4f\n", "TOPLAM", total_cpu_ms, total_gpu_ms);
    printf("\nNot: GPU toplam, cekirdek surelerinin toplamidir; shader derleme,\n"
           "buffer yukleme ve geri okuma bu sureye dahil degildir.\n");
}

/* Runs compute-shader benchmarks. */
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

    printf("\n=== compute shader matrix operations ===\n");
    gl_print_info();
    if (!gl_has_compute()) {
        fprintf(stderr, "OpenGL 4.3+ compute shader destegi gerekiyor.\n");
        gl_shutdown();
        return 2;
    }
    printf("N        : %d  (%d x %d matrisler)\n", n, n, n);
    printf("mod      : %s\n", run_all ? "tum islemler" : matrix_operation_name(selected));

    Mat A = mat_alloc(n, n);
    Mat B = mat_alloc(n, n);
    Mat C_gpu = mat_alloc(n, n);
    mat_fill_random(&A, 12345u);
    mat_fill_random(&B, 67890u);
    if (run_all || selected == MATRIX_OP_DIVIDE)
        matrix_operation_prepare_inputs(MATRIX_OP_DIVIDE, &B);

    if (show) {
        mat_print("A", &A, 8);
        mat_print("B", &B, 8);
    }

    size_t bytes = (size_t)n * (size_t)n * sizeof(float);
    GLuint input_a = upload_ssbo(0, A.data, bytes);
    GLuint input_b = upload_ssbo(1, B.data, bytes);
    GLuint output = upload_ssbo(2, NULL, bytes);

    const int local_size = 16;
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
                               &C_gpu, output, bytes, groups,
                               &results[result_count])) {
                success = false;
                break;
            }
            ++result_count;
        }
    } else {
        success = run_operation(selected, n, verify, show, &A, &B, &C_gpu,
                                output, bytes, groups, &results[0]);
        result_count = success ? 1 : 0;
    }

    if (result_count > 0) print_summary(results, result_count);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "[GL hatasi] 0x%04X\n", error);
        success = false;
    }

    glDeleteBuffers(1, &input_a);
    glDeleteBuffers(1, &input_b);
    glDeleteBuffers(1, &output);
    mat_free(&A);
    mat_free(&B);
    mat_free(&C_gpu);
    gl_shutdown();
    return success ? 0 : 1;
}
