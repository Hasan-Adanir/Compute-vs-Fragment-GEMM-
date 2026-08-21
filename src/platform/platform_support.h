/* Provides shared OpenGL and matrix helpers. */
#ifndef PLATFORM_SUPPORT_H
#define PLATFORM_SUPPORT_H

#include <glad/gl.h>
#include <stdbool.h>

/* OpenGL context helpers. */
bool gl_init(void);
void gl_shutdown(void);
bool gl_has_compute(void); /* Checks for OpenGL 4.3. */
void gl_print_info(void);

/* Shader helpers. */
GLuint gl_program_graphics(const char *vert_file, const char *frag_file);
GLuint gl_program_compute(const char *comp_file);

/* GPU timing helpers. */
void   gl_timer_begin(void);
double gl_timer_end_ms(void); /* Waits for the result. */

/* Row-major matrix. */
typedef struct {
    int    rows;
    int    cols;
    float *data;
} Mat;

Mat  mat_alloc(int rows, int cols);
void mat_free(Mat *m);

/* Prints a bounded matrix preview. */
void mat_print(const char *name, const Mat *m, int max);

/* Fills a matrix deterministically. */
void mat_fill_random(Mat *m, unsigned int seed);

void mat_mul_reference(const Mat *A, const Mat *B, Mat *C); /* Uses fp64 sums. */
void mat_mul_cpu(const Mat *A, const Mat *B, Mat *C); /* Uses blocked fp32. */

/* Compares GPU and reference results. */
bool mat_check(const Mat *ref, const Mat *test);

/* Benchmark settings. */
#define MM_WARMUP  3
#define MM_RUNS   10

double mm_median(double *samples, int count); /* Sorts samples. */
double mm_gflops(int N, double ms); /* Computes 2*N^3/t. */
double mm_now_ms(void);

#endif /* PLATFORM_SUPPORT_H */
