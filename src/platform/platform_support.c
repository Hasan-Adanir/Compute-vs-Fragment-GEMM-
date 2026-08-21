#include "platform_support.h"

#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef MM_ASSET_DIR
#define MM_ASSET_DIR "."
#endif

static GLFWwindow *g_window;
static GLuint      g_query;
static bool        g_has_compute;

/* Reports GLFW errors. */
static void glfw_error_cb(int code, const char *desc)
{
    fprintf(stderr, "[GLFW %d] %s\n", code, desc);
}

/* Initializes the OpenGL context. */
bool gl_init(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0); /* Keep output immediate. */

    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) {
        fprintf(stderr, "GLFW baslatilamadi\n");
        return false;
    }

    /* Use a compatibility profile for the VAO-free fragment path. */
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); /* Hide the window. */
    g_window = glfwCreateWindow(64, 64, "matmul", NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "OpenGL baglami olusturulamadi\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    if (gladLoadGL((GLADloadfunc)glfwGetProcAddress) == 0) {
        fprintf(stderr, "GLAD yuklenemedi\n");
        return false;
    }

    if (!GLAD_GL_VERSION_3_0) {
        fprintf(stderr,
                "OpenGL %s alindi. FBO ve float doku icin en az 3.0 gerekiyor;\n"
                "macOS compatibility profile'i 2.1'de birakir -- Linux'ta calistirin.\n",
                (const char *)glGetString(GL_VERSION));
        return false;
    }

    g_has_compute = (GLAD_GL_VERSION_4_3 != 0);
    glGenQueries(1, &g_query);
    return true;
}

/* Releases OpenGL resources. */
void gl_shutdown(void)
{
    if (g_query) glDeleteQueries(1, &g_query);
    if (g_window) glfwDestroyWindow(g_window);
    glfwTerminate();
}

/* Reports compute-shader support. */
bool gl_has_compute(void) { return g_has_compute; }

/* Prints OpenGL device details. */
void gl_print_info(void)
{
    printf("GPU      : %s (%s)\n", glGetString(GL_RENDERER), glGetString(GL_VENDOR));
    printf("OpenGL   : %s | GLSL %s\n",
           glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Compute  : %s\n", g_has_compute ? "var" : "yok (GL 4.3 gerekiyor)");
}

/* Reads a shader file. */
static char *read_file(const char *rel)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/shaders/%s", MM_ASSET_DIR, rel);

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "shader acilamadi: %s\n", path); return NULL; }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)len + 1);
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

/* Compiles a shader stage. */
static GLuint compile(GLenum type, const char *file)
{
    char *src = read_file(file);
    if (!src) return 0;

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, (const char **)&src, NULL);
    glCompileShader(sh);
    free(src);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "\n[%s derlenemedi]\n%s\n", file, log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

/* Links shader stages. */
static GLuint link_of(GLuint *stages, int n)
{
    GLuint prog = glCreateProgram();
    for (int i = 0; i < n; ++i) glAttachShader(prog, stages[i]);

    /* Bind the GLES 2.0 vertex attribute. */
    glBindAttribLocation(prog, 0, "aPos");

    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "\n[link hatasi]\n%s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    for (int i = 0; i < n; ++i) glDeleteShader(stages[i]);
    return prog;
}

/* Creates a graphics program. */
GLuint gl_program_graphics(const char *vert_file, const char *frag_file)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vert_file);
    if (!vs) return 0;
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag_file);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint stages[2] = { vs, fs };
    return link_of(stages, 2);
}

/* Creates a compute program. */
GLuint gl_program_compute(const char *comp_file)
{
    GLuint cs = compile(GL_COMPUTE_SHADER, comp_file);
    if (!cs) return 0;
    return link_of(&cs, 1);
}

/* Starts GPU timing. */
void gl_timer_begin(void)
{
    glBeginQuery(GL_TIME_ELAPSED, g_query);
}

/* Stops GPU timing. */
double gl_timer_end_ms(void)
{
    glEndQuery(GL_TIME_ELAPSED);

    GLint available = 0;
    while (!available)
        glGetQueryObjectiv(g_query, GL_QUERY_RESULT_AVAILABLE, &available);

    GLuint64 ns = 0;
    glGetQueryObjectui64v(g_query, GL_QUERY_RESULT, &ns);
    return (double)ns / 1.0e6;
}

/* Allocates a matrix. */
Mat mat_alloc(int rows, int cols)
{
    Mat m;
    m.rows = rows;
    m.cols = cols;
    m.data = (float *)malloc((size_t)rows * (size_t)cols * sizeof(float));
    if (!m.data) {
        fprintf(stderr, "bellek yetmedi: %dx%d\n", rows, cols);
        exit(1);
    }
    return m;
}

/* Frees a matrix. */
void mat_free(Mat *m)
{
    free(m->data);
    m->data = NULL;
}

/* Prints a matrix preview. */
void mat_print(const char *name, const Mat *m, int max)
{
    int R = m->rows < max ? m->rows : max;
    int C = m->cols < max ? m->cols : max;

    printf("%s  (%dx%d", name, m->rows, m->cols);
    if (R < m->rows || C < m->cols) printf(", sol ust %dx%d gosteriliyor", R, C);
    printf(")\n");

    for (int i = 0; i < R; ++i) {
        printf("   ");
        for (int j = 0; j < C; ++j)
            printf("%10.4f", m->data[(size_t)i * m->cols + j]);
        printf(C < m->cols ? "   ...\n" : "\n");
    }
    if (R < m->rows) printf("        ...\n");
    printf("\n");
}

/* Fills a matrix deterministically. */
void mat_fill_random(Mat *m, unsigned int seed)
{
    unsigned int s = seed ? seed : 1u;
    size_t n = (size_t)m->rows * (size_t)m->cols;
    for (size_t i = 0; i < n; ++i) {
        s ^= s << 13; 
        s ^= s >> 17; 
        s ^= s << 5; /* xorshift32 */
        float u = (float)(s >> 8) / (float)(1u << 24); /* [0,1) */
        m->data[i] = u * 2.0f - 1.0f; /* [-1,1) */
    }
}

/* Multiplies matrices accurately. */
void mat_mul_reference(const Mat *A, const Mat *B, Mat *C)
{
    int M = A->rows, K = A->cols, N = B->cols;
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < N; ++j) {
            double acc = 0.0;
            for (int k = 0; k < K; ++k)
                acc += (double)A->data[(size_t)i * K + k] *
                       (double)B->data[(size_t)k * N + j];
            C->data[(size_t)i * N + j] = (float)acc;
        }
}

/* Multiplies matrices on the CPU. */
void mat_mul_cpu(const Mat *A, const Mat *B, Mat *C)
{
    int M = A->rows, K = A->cols, N = B->cols;
    const int BS = 64;

    memset(C->data, 0, (size_t)M * N * sizeof(float));

    for (int ii = 0; ii < M; ii += BS)
        for (int kk = 0; kk < K; kk += BS)
            for (int jj = 0; jj < N; jj += BS) {
                int i_end = ii + BS < M ? ii + BS : M;
                int k_end = kk + BS < K ? kk + BS : K;
                int j_end = jj + BS < N ? jj + BS : N;
                for (int i = ii; i < i_end; ++i)
                    for (int k = kk; k < k_end; ++k) {
                        float a = A->data[(size_t)i * K + k];
                        const float *brow = &B->data[(size_t)k * N];
                        float *crow = &C->data[(size_t)i * N];
                        for (int j = jj; j < j_end; ++j)
                            crow[j] += a * brow[j];
                    }
            }
}

/* Compares matrix results. */
bool mat_check(const Mat *ref, const Mat *test)
{
    size_t n = (size_t)ref->rows * (size_t)ref->cols;

    double cmax = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double v = fabs((double)ref->data[i]);
        if (v > cmax) cmax = v;
    }

    const double rel_tol = 1e-4;
    const double abs_tol = 1e-5 * cmax;

    double max_abs = 0.0, sq = 0.0;
    int    bad = 0;

    for (size_t i = 0; i < n; ++i) {
        double a  = (double)ref->data[i];
        double ae = fabs(a - (double)test->data[i]);
        if (ae > max_abs) max_abs = ae;
        if (ae > abs_tol + rel_tol * fabs(a)) bad++;
        sq += ae * ae;
    }

    bool ok = (bad == 0);
    printf("  dogruluk : %s  max_abs %.3e  rms %.3e  basarisiz %d/%zu"
           "  (olcut: %.2e + %.0e*|ref|)\n",
           ok ? "GECTI" : "KALDI", max_abs, sqrt(sq / (double)n), bad, n,
           abs_tol, rel_tol);
    return ok;
}

/* Compares two doubles. */
static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Returns the sample median. */
double mm_median(double *samples, int count)
{
    qsort(samples, (size_t)count, sizeof(double), cmp_double);
    return (count % 2) ? samples[count / 2]
                       : 0.5 * (samples[count / 2 - 1] + samples[count / 2]);
}

/* Calculates multiplication throughput. */
double mm_gflops(int N, double ms)
{
    /* Count one multiply and one add. */
    return (2.0 * (double)N * N * N) / (ms / 1000.0) / 1.0e9;
}

/* Returns monotonic time in milliseconds. */
double mm_now_ms(void)
{
    /* Use GLFW for portable timing. */
    return glfwGetTime() * 1000.0;
}
