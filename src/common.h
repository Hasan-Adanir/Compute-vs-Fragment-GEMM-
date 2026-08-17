/* Iki varyantin (frag_naive.c, comp_naive.c) ortak kullandigi asgari yardimcilar.
 * Burada is mantigi yok: GL baglami, shader yukleme, zamanlayici ve CPU
 * referansi. Asil anlatilmak istenen sey varyant dosyalarinin icinde. */
#ifndef MM_COMMON_H
#define MM_COMMON_H

#include <glad/gl.h>
#include <stdbool.h>

/* ---------------------------------------------------------- GL baglami ---
 * Gizli pencere acilir: cizim yapmiyoruz ama OpenGL'in bir baglama ihtiyaci var.
 * Once 4.3 core istenir (compute shader icin gerekli), alinamazsa 4.1 core'a
 * dusulur -- macOS'un tavani budur. */
bool gl_init(void);
void gl_shutdown(void);
bool gl_has_compute(void);   /* GL >= 4.3 mu? */
void gl_print_info(void);

/* ------------------------------------------------------------- shader ---
 * Shader'lar calisma aninda shaders/ klasorunden okunur; duzenleyip programi
 * yeniden calistirmak yeterli, yeniden derleme gerekmez.
 * Hata olursa mesaj basilir ve 0 dondurulur. */
GLuint gl_program_graphics(const char *vert_file, const char *frag_file);
GLuint gl_program_compute(const char *comp_file);

/* --------------------------------------------------------- zamanlayici ---
 * GL_TIME_ELAPSED sorgusu: sadece GPU'da gecen sureyi olcer. */
void   gl_timer_begin(void);
double gl_timer_end_ms(void);   /* sonuc hazir olana kadar bloklar */

/* --------------------------------------------------------------- matris ---
 * Satir-oncelikli: data[r * cols + c] */
typedef struct {
    int    rows;
    int    cols;
    float *data;
} Mat;

Mat  mat_alloc(int rows, int cols);
void mat_free(Mat *m);

/* Matrisin sol ust kosesinden en fazla max x max hucre basar; matris daha
 * buyukse kesildigini "..." ile belirtir. --print bayragi bunu kullanir. */
void mat_print(const char *name, const Mat *m, int max);

/* Deterministik PRNG: rand() platformdan platforma degistigi icin Mac'te ve
 * Windows'ta ayni matrisleri uretebilmek adina kendi uretecimizi tasiyoruz. */
void mat_fill_random(Mat *m, unsigned int seed);

void mat_mul_reference(const Mat *A, const Mat *B, Mat *C);  /* fp64 akumulator */
void mat_mul_cpu(const Mat *A, const Mat *B, Mat *C);        /* cache-blocked fp32 */

/* GPU sonucunu fp64 referansla karsilastirir, ozeti basar, gecti/kaldi doner.
 * Olcut: |ref - test| <= abs_tol + rel_tol*|ref| (numpy allclose ile ayni).
 * Saf bagil hata burada yaniltici olurdu: rastgele [-1,1] matrislerin
 * carpiminda bazi sonuc hucreleri sifira cok yakin duser ve payda cokerek
 * hatayi yapay olarak buyutur. */
bool mat_check(const Mat *ref, const Mat *test);

/* --------------------------------------------------------------- olcum ---*/
#define MM_WARMUP  3
#define MM_RUNS   10

double mm_median(double *samples, int count);   /* diziyi siralar */
double mm_gflops(int N, double ms);             /* 2*N^3 / t */
double mm_now_ms(void);

#endif /* MM_COMMON_H */
