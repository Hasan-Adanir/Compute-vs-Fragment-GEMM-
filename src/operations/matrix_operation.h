#ifndef MATRIX_OPERATION_H
#define MATRIX_OPERATION_H

#include "platform_support.h"

typedef enum {
    MATRIX_OP_MULTIPLY,
    MATRIX_OP_ADD,
    MATRIX_OP_SUBTRACT,
    MATRIX_OP_DIVIDE
} MatrixOperation;

bool matrix_operation_parse(const char *text, MatrixOperation *operation);
const char *matrix_operation_name(MatrixOperation operation);
const char *matrix_operation_fragment_shader(MatrixOperation operation);
const char *matrix_operation_compute_shader(MatrixOperation operation);

/* Stabilizes division inputs. */
void matrix_operation_prepare_inputs(MatrixOperation operation, Mat *B);

void matrix_operation_cpu(MatrixOperation operation,
                          const Mat *A, const Mat *B, Mat *C);
void matrix_operation_reference(MatrixOperation operation,
                                const Mat *A, const Mat *B, Mat *C);
double matrix_operation_gflops(MatrixOperation operation, int n, double ms);

#endif
