/* Runs interactive fragment-shader matrix operations. */

#include "fragment_compute.h"
#include "matrix_operation.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CLI_MAX_DIMENSION 4096

/* Reads a bounded integer. */
static bool read_int(const char *prompt, int minimum, int maximum, int *value)
{
    char line[128];

    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) return false;

        errno = 0;
        char *end = NULL;
        long parsed = strtol(line, &end, 10);
        while (end && isspace((unsigned char)*end)) ++end;

        if (errno == 0 && end != line && end && *end == '\0' &&
            parsed >= minimum && parsed <= maximum) {
            *value = (int)parsed;
            return true;
        }

        printf("Gecersiz giris. %d ile %d arasinda bir tam sayi girin.\n",
               minimum, maximum);
    }
}

/* Reads a valid float. */
static bool read_float(const char *prompt, bool reject_zero, float *value)
{
    char line[128];

    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) return false;

        errno = 0;
        char *end = NULL;
        float parsed = strtof(line, &end);
        while (end && isspace((unsigned char)*end)) ++end;

        if (errno == 0 && end != line && end && *end == '\0' &&
            isfinite(parsed) && (!reject_zero || parsed != 0.0f)) {
            *value = parsed;
            return true;
        }

        if (reject_zero && parsed == 0.0f)
            printf("Bolen sifir olamaz. Sifirdan farkli bir deger girin.\n");
        else
            printf("Gecersiz sayi. Ornek: 2.5 veya -3\n");
    }
}

/* Reads all matrix values. */
static bool read_matrix(const char *name, Mat *matrix, bool reject_zero)
{
    printf("%s matrisinin elemanlarini satir satir girin:\n", name);
    for (int row = 0; row < matrix->rows; ++row) {
        for (int column = 0; column < matrix->cols; ++column) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt), "%s[%d][%d] = ",
                     name, row, column);
            if (!read_float(prompt, reject_zero,
                            &matrix->data[(size_t)row * matrix->cols + column]))
                return false;
        }
    }
    return true;
}

/* Prints a matrix. */
static void print_matrix(const char *name, const Mat *matrix)
{
    printf("%s (%d x %d)\n", name, matrix->rows, matrix->cols);
    for (int row = 0; row < matrix->rows; ++row) {
        printf("  ");
        for (int column = 0; column < matrix->cols; ++column)
            printf("%12.5f", matrix->data[(size_t)row * matrix->cols + column]);
        printf("\n");
    }
}

/* Reads the operation choice. */
static bool choose_operation(MatrixOperation *operation)
{
    printf("Yapilacak islemi secin:\n"
           "  1 - Matris carpimi\n"
           "  2 - Eleman bazli toplama\n"
           "  3 - Eleman bazli cikarma\n"
           "  4 - Eleman bazli bolme\n");

    int choice = 0;
    if (!read_int("Secim: ", 1, 4, &choice)) return false;

    static const MatrixOperation operations[] = {
        MATRIX_OP_MULTIPLY,
        MATRIX_OP_ADD,
        MATRIX_OP_SUBTRACT,
        MATRIX_OP_DIVIDE
    };
    *operation = operations[choice - 1];
    return true;
}

/* Validates matrix dimensions. */
static bool validate_dimensions(MatrixOperation operation,
                                int a_rows, int a_cols,
                                int b_rows, int b_cols,
                                int *c_rows, int *c_cols)
{
    if (operation == MATRIX_OP_MULTIPLY) {
        if (a_cols != b_rows) {
            fprintf(stderr,
                    "Boyut hatasi: Matris carpimi icin A'nin sutun sayisi "
                    "B'nin satir sayisina esit olmali (%d != %d).\n",
                    a_cols, b_rows);
            return false;
        }
        *c_rows = a_rows;
        *c_cols = b_cols;
        return true;
    }

    if (a_rows != b_rows || a_cols != b_cols) {
        fprintf(stderr,
                "Boyut hatasi: Toplama, cikarma ve bolme icin A ve B "
                "ayni boyutta olmali (A=%dx%d, B=%dx%d).\n",
                a_rows, a_cols, b_rows, b_cols);
        return false;
    }

    *c_rows = a_rows;
    *c_cols = a_cols;
    return true;
}

/* Runs the interactive CLI. */
int main(void)
{
    printf("=== Fragment Shader Matrix Terminal Arayuzu ===\n"
           "Bu uygulama hesaplamalarda compute shader kullanmaz.\n");

    MatrixOperation operation;
    if (!choose_operation(&operation)) return 1;

    int a_rows, a_cols, b_rows, b_cols;
    if (!read_int("A satir sayisi : ", 1, CLI_MAX_DIMENSION, &a_rows) ||
        !read_int("A sutun sayisi : ", 1, CLI_MAX_DIMENSION, &a_cols) ||
        !read_int("B satir sayisi : ", 1, CLI_MAX_DIMENSION, &b_rows) ||
        !read_int("B sutun sayisi : ", 1, CLI_MAX_DIMENSION, &b_cols))
        return 1;

    int c_rows, c_cols;
    if (!validate_dimensions(operation, a_rows, a_cols, b_rows, b_cols,
                             &c_rows, &c_cols))
        return 1;

    Mat A = mat_alloc(a_rows, a_cols);
    Mat B = mat_alloc(b_rows, b_cols);
    Mat C = mat_alloc(c_rows, c_cols);

    bool division = operation == MATRIX_OP_DIVIDE;
    if (!read_matrix("A", &A, false) || !read_matrix("B", &B, division)) {
        fprintf(stderr, "Giris okunamadi.\n");
        mat_free(&A);
        mat_free(&B);
        mat_free(&C);
        return 1;
    }

    if (!gl_init()) {
        mat_free(&A);
        mat_free(&B);
        mat_free(&C);
        return 1;
    }

    GLint max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (a_rows > max_texture_size || a_cols > max_texture_size ||
        b_rows > max_texture_size || b_cols > max_texture_size ||
        c_rows > max_texture_size || c_cols > max_texture_size) {
        fprintf(stderr,
                "Matris boyutu GPU'nun texture sinirini asiyor (maksimum %d).\n",
                max_texture_size);
        gl_shutdown();
        mat_free(&A);
        mat_free(&B);
        mat_free(&C);
        return 1;
    }

    My_glInit();
    MyBuffer input_a = My_glCreateBuffer(A.cols, A.rows, A.data);
    MyBuffer input_b = My_glCreateBuffer(B.cols, B.rows, B.data);
    MyBuffer output = My_glCreateBuffer(C.cols, C.rows, NULL);
    My_glBindBufferBase(0, &input_a);
    My_glBindBufferBase(1, &input_b);
    My_glBindBufferBase(2, &output);

    GLuint program = My_glCreateComputeProgram(
        matrix_operation_fragment_shader(operation));
    bool success = program != 0;
    MyGLStateSnapshot state = {0};

    if (success) success = My_glBeginStateScope(&state);
    if (success) {
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uM"), (float)C.rows);
        glUniform1f(glGetUniformLocation(program, "uN"), (float)C.cols);
        glUniform1f(glGetUniformLocation(program, "uK"), (float)A.cols);

        const int local_size = 16;
        My_glProgramLocalSize(local_size, local_size);
        GLuint groups_x = (GLuint)((C.cols + local_size - 1) / local_size);
        GLuint groups_y = (GLuint)((C.rows + local_size - 1) / local_size);

        gl_timer_begin();
        My_glDispatchCompute(groups_x, groups_y, 1);
        double gpu_ms = gl_timer_end_ms();

        My_glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        My_glGetBufferSubData(&output, C.data);

        printf("Islem: %s\n", matrix_operation_name(operation));
        printf("GPU fragment suresi: %.4f ms\n", gpu_ms);
        print_matrix("C sonucu", &C);
    }

    My_glEndStateScope(&state);
    if (!success) fprintf(stderr, "Fragment shader islemi basarisiz oldu.\n");

    if (program) glDeleteProgram(program);
    My_glDeleteBuffer(&input_a);
    My_glDeleteBuffer(&input_b);
    My_glDeleteBuffer(&output);
    gl_shutdown();
    mat_free(&A);
    mat_free(&B);
    mat_free(&C);
    return success ? 0 : 1;
}
