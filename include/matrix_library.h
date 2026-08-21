#ifndef MATRIX_LIBRARY_H
#define MATRIX_LIBRARY_H

#include <stdbool.h>

typedef struct {
    int rows;
    int cols;
    float *data;
} Matrix;

/* Multiplies two matrices. */
bool matrix_multiply(const Matrix *a, const Matrix *b, Matrix *result);

/* Adds two matrices. */
bool matrix_add(const Matrix *a, const Matrix *b, Matrix *result);

/* Subtracts two matrices. */
bool matrix_subtract(const Matrix *a, const Matrix *b, Matrix *result);

/* Divides two matrices element-wise. */
bool matrix_divide(const Matrix *a, const Matrix *b, Matrix *result);

/* Releases a result matrix. */
void matrix_release(Matrix *matrix);

/* Releases internal GPU resources. */
void matrix_library_shutdown(void);

#endif
