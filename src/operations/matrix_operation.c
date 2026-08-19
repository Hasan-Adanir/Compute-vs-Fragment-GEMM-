#include "matrix_operation.h"

#include <math.h>
#include <string.h>

bool matrix_operation_parse(const char *text, MatrixOperation *operation)
{
    if (strcmp(text, "multiply") == 0)      *operation = MATRIX_OP_MULTIPLY;
    else if (strcmp(text, "add") == 0)      *operation = MATRIX_OP_ADD;
    else if (strcmp(text, "subtract") == 0) *operation = MATRIX_OP_SUBTRACT;
    else if (strcmp(text, "divide") == 0)   *operation = MATRIX_OP_DIVIDE;
    else return false;
    return true;
}

const char *matrix_operation_name(MatrixOperation operation)
{
    switch (operation) {
        case MATRIX_OP_MULTIPLY: return "multiply";
        case MATRIX_OP_ADD:      return "add";
        case MATRIX_OP_SUBTRACT: return "subtract";
        case MATRIX_OP_DIVIDE:   return "divide";
    }
    return "unknown";
}

const char *matrix_operation_fragment_shader(MatrixOperation operation)
{
    switch (operation) {
        case MATRIX_OP_MULTIPLY: return "fragment/matrix_multiply.frag";
        case MATRIX_OP_ADD:      return "fragment/matrix_add.frag";
        case MATRIX_OP_SUBTRACT: return "fragment/matrix_subtract.frag";
        case MATRIX_OP_DIVIDE:   return "fragment/matrix_divide.frag";
    }
    return NULL;
}

const char *matrix_operation_compute_shader(MatrixOperation operation)
{
    switch (operation) {
        case MATRIX_OP_MULTIPLY: return "compute/matrix_multiply.comp";
        case MATRIX_OP_ADD:      return "compute/matrix_add.comp";
        case MATRIX_OP_SUBTRACT: return "compute/matrix_subtract.comp";
        case MATRIX_OP_DIVIDE:   return "compute/matrix_divide.comp";
    }
    return NULL;
}

void matrix_operation_prepare_inputs(MatrixOperation operation, Mat *B)
{
    if (operation != MATRIX_OP_DIVIDE) return;

    size_t count = (size_t)B->rows * (size_t)B->cols;
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(B->data[i]) < 0.125f)
            B->data[i] = B->data[i] < 0.0f ? -0.125f : 0.125f;
    }
}

static void elementwise(MatrixOperation operation,
                        const Mat *A, const Mat *B, Mat *C)
{
    size_t count = (size_t)A->rows * (size_t)A->cols;
    for (size_t i = 0; i < count; ++i) {
        switch (operation) {
            case MATRIX_OP_ADD:      C->data[i] = A->data[i] + B->data[i]; break;
            case MATRIX_OP_SUBTRACT: C->data[i] = A->data[i] - B->data[i]; break;
            case MATRIX_OP_DIVIDE:   C->data[i] = A->data[i] / B->data[i]; break;
            case MATRIX_OP_MULTIPLY: break;
        }
    }
}

void matrix_operation_cpu(MatrixOperation operation,
                          const Mat *A, const Mat *B, Mat *C)
{
    if (operation == MATRIX_OP_MULTIPLY) mat_mul_cpu(A, B, C);
    else elementwise(operation, A, B, C);
}

void matrix_operation_reference(MatrixOperation operation,
                                const Mat *A, const Mat *B, Mat *C)
{
    if (operation == MATRIX_OP_MULTIPLY) {
        mat_mul_reference(A, B, C);
        return;
    }

    size_t count = (size_t)A->rows * (size_t)A->cols;
    for (size_t i = 0; i < count; ++i) {
        double a = A->data[i];
        double b = B->data[i];
        switch (operation) {
            case MATRIX_OP_ADD:      C->data[i] = (float)(a + b); break;
            case MATRIX_OP_SUBTRACT: C->data[i] = (float)(a - b); break;
            case MATRIX_OP_DIVIDE:   C->data[i] = (float)(a / b); break;
            case MATRIX_OP_MULTIPLY: break;
        }
    }
}

double matrix_operation_gflops(MatrixOperation operation, int n, double ms)
{
    double operations = operation == MATRIX_OP_MULTIPLY
                      ? 2.0 * (double)n * n * n
                      : (double)n * n;
    return operations / (ms / 1000.0) / 1.0e9;
}
