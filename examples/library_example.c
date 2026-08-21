#include "matrix_library.h"

#include <stdio.h>

/* Prints a matrix. */
static void print_matrix(const Matrix *matrix)
{
    for (int row = 0; row < matrix->rows; ++row) {
        for (int col = 0; col < matrix->cols; ++col)
            printf("%8.2f", matrix->data[row * matrix->cols + col]);
        printf("\n");
    }
}

/* Demonstrates the public API. */
int main(void)
{
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b_data[] = {5.0f, 6.0f, 7.0f, 8.0f};

    Matrix a = {2, 2, a_data};
    Matrix b = {2, 2, b_data};
    Matrix result = {0};

    if (!matrix_multiply(&a, &b, &result)) {
        fprintf(stderr, "Matrix operation failed.\n");
        matrix_library_shutdown();
        return 1;
    }

    print_matrix(&result);
    matrix_release(&result);
    matrix_library_shutdown();
    return 0;
}
