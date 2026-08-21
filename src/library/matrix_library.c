#include "matrix_library.h"

#include "fragment_compute.h"
#include "matrix_operation.h"

#include <stdlib.h>

static bool g_initialized;

/* Initializes the internal GPU runtime. */
static bool ensure_initialized(void)
{
    if (g_initialized) return true;
    if (!gl_init()) return false;

    My_glInit();
    My_glProgramLocalSize(16, 16);
    g_initialized = true;
    return true;
}

/* Validates a matrix view. */
static bool valid_matrix(const Matrix *matrix)
{
    return matrix && matrix->rows > 0 && matrix->cols > 0 && matrix->data;
}

/* Runs one matrix operation. */
static bool run_operation(MatrixOperation operation, const Matrix *a,
                          const Matrix *b, Matrix *result)
{
    if (!valid_matrix(a) || !valid_matrix(b) || !result || result == a ||
        result == b) return false;

    int rows;
    int cols;
    int inner;

    if (operation == MATRIX_OP_MULTIPLY) {
        if (a->cols != b->rows) return false;
        rows = a->rows;
        cols = b->cols;
        inner = a->cols;
    } else {
        if (a->rows != b->rows || a->cols != b->cols) return false;
        rows = a->rows;
        cols = a->cols;
        inner = 1;
    }

    if (!ensure_initialized()) return false;

    size_t count = (size_t)rows * (size_t)cols;
    float *output_data = (float *)malloc(count * sizeof(float));
    if (!output_data) return false;

    GLuint program = My_glCreateComputeProgram(
        matrix_operation_fragment_shader(operation));
    if (!program) {
        free(output_data);
        return false;
    }

    MyGLStateSnapshot state = {0};
    if (!My_glBeginStateScope(&state)) {
        My_glEndStateScope(&state);
        glDeleteProgram(program);
        free(output_data);
        return false;
    }

    MyBuffer input_a = My_glCreateBuffer(a->cols, a->rows, a->data);
    MyBuffer input_b = My_glCreateBuffer(b->cols, b->rows, b->data);
    MyBuffer output = My_glCreateBuffer(cols, rows, NULL);

    My_glBindBufferBase(0, &input_a);
    My_glBindBufferBase(1, &input_b);
    My_glBindBufferBase(2, &output);

    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "uM"), (float)rows);
    glUniform1f(glGetUniformLocation(program, "uN"), (float)cols);
    glUniform1f(glGetUniformLocation(program, "uK"), (float)inner);

    GLuint groups_x = (GLuint)((cols + 15) / 16);
    GLuint groups_y = (GLuint)((rows + 15) / 16);
    My_glDispatchCompute(groups_x, groups_y, 1);
    My_glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    My_glGetBufferSubData(&output, output_data);

    bool success = glGetError() == GL_NO_ERROR;
    My_glDeleteBuffer(&input_a);
    My_glDeleteBuffer(&input_b);
    My_glDeleteBuffer(&output);
    My_glEndStateScope(&state);
    glDeleteProgram(program);

    if (!success) {
        free(output_data);
        return false;
    }

    result->rows = rows;
    result->cols = cols;
    result->data = output_data;
    return true;
}

/* Multiplies two matrices. */
bool matrix_multiply(const Matrix *a, const Matrix *b, Matrix *result)
{
    return run_operation(MATRIX_OP_MULTIPLY, a, b, result);
}

/* Adds two matrices. */
bool matrix_add(const Matrix *a, const Matrix *b, Matrix *result)
{
    return run_operation(MATRIX_OP_ADD, a, b, result);
}

/* Subtracts two matrices. */
bool matrix_subtract(const Matrix *a, const Matrix *b, Matrix *result)
{
    return run_operation(MATRIX_OP_SUBTRACT, a, b, result);
}

/* Divides two matrices element-wise. */
bool matrix_divide(const Matrix *a, const Matrix *b, Matrix *result)
{
    return run_operation(MATRIX_OP_DIVIDE, a, b, result);
}

/* Releases a result matrix. */
void matrix_release(Matrix *matrix)
{
    if (!matrix) return;
    free(matrix->data);
    matrix->rows = 0;
    matrix->cols = 0;
    matrix->data = NULL;
}

/* Releases internal GPU resources. */
void matrix_library_shutdown(void)
{
    if (!g_initialized) return;
    gl_shutdown();
    g_initialized = false;
}
