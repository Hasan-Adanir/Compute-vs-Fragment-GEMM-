/* mygl.c -- My_gl* API'sinin fragment shader ile gerceklenmesi. Bu dosyadaki
 * her fonksiyon, mygl.h'deki tabloda karsisinda duran compute cagrisinin
 * yaptigi isi GLES 2.0 araclariyla yapar. */

#include "mygl.h"

#include <stdio.h>
#include <stdlib.h>

/* Tek kanalli float doku GLES 2.0'da yok; RGBA kullanip .r kanalini
 * okuyoruz. Masaustu GL boyutlu internal format ister, GLES 2.0 ise
 * (OES_texture_float ile) internalformat == format olmasini. Tek fark bu. */
#ifdef MY_GLES2
#define MY_TEX_INTERNAL GL_RGBA
#else
#define MY_TEX_INTERNAL GL_RGBA32F
#endif

#define MY_MAX_BINDINGS 8

static MyBuffer *g_bound[MY_MAX_BINDINGS];
static GLuint    g_vbo;
static int       g_local_x = 1;
static int       g_local_y = 1;

/* ============================================================ baslatma ==*/

void My_glInit(void)
{
    /* Tam ekran ucgen: (-1,-1), (3,-1), (-1,3). Viewport'u tamamen kaplar,
     * tasan kisim kirpilir; iki ucgenli quad'a gore kose boyunca tekrar eden
     * fragment olmaz. GLES 2.0'da gl_VertexID olmadigi icin koseler gercek
     * bir VBO'dan geliyor. */
    static const GLfloat tri[6] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);

    /* VAO yok: attribute durumu global. Bir kez kuruluyor ve oyle kaliyor.
     * aPos, link sirasinda 0 numarali konuma baglaniyor (common.c). */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);   /* karistirma acik kalirsa sonuc bozulur */
}

/* ============================================================== buffer ==*/

MyBuffer My_glCreateBuffer(int w, int h, const float *data)
{
    MyBuffer b;
    b.w   = w;
    b.h   = h;
    b.fbo = 0;

    /* Doku RGBA oldugu icin veriyi genisletmek gerekiyor: deger .r kanalinda,
     * digerleri sifir. */
    float *rgba = NULL;
    if (data) {
        rgba = (float *)calloc((size_t)w * (size_t)h * 4, sizeof(float));
        for (size_t i = 0; i < (size_t)w * (size_t)h; ++i)
            rgba[i * 4] = data[i];
    }

    glGenTextures(1, &b.tex);
    glBindTexture(GL_TEXTURE_2D, b.tex);
    /* Filtreleme ve mipmap istemiyoruz: doku burada bir dizi, resim degil. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, MY_TEX_INTERNAL, w, h, 0,
                 GL_RGBA, GL_FLOAT, rgba);
    free(rgba);

    if (!data) {
        /* Cikti buffer'i. Compute'ta writeonly bir SSBO yeterdi; burada
         * dokuyu FBO'ya renk eki olarak baglamak gerekiyor, */
        glGenFramebuffers(1, &b.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, b.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, b.tex, 0);
        /* GLES 2.0'da glDrawBuffers / glReadBuffer yok: tek renk eki var,
         * secim yapilacak bir sey de yok. */
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fprintf(stderr, "My_glCreateBuffer: FBO tamamlanamadi "
                            "(float doku hedefi desteklenmiyor olabilir)\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    return b;
}


void My_glDeleteBuffer(MyBuffer *buf)
{
    if (buf->fbo) glDeleteFramebuffers(1, &buf->fbo);
    glDeleteTextures(1, &buf->tex);
    buf->tex = buf->fbo = 0;
}





void My_glBindBufferBase(GLuint binding, MyBuffer *buf)
{
    /* Baglama noktasi burada sadece not ediliyor; doku birimine bagli hale
     * gelmesi ve uniform'larinin doldurulmasi dispatch aninda oluyor, cunku
     * o ana kadar hangi programin kullanildigi belli degil. */
    if (binding < MY_MAX_BINDINGS)
        g_bound[binding] = buf;
}

/* ============================================================= program ==*/




GLuint My_glCreateComputeProgram(const char *frag_file)
{
    /* Numaranin tamami bu satirda: "compute" kernel'i bir fragment shader ve
     * onu is uretecek bir vertex shader'la linkliyoruz. */
    return gl_program_graphics("fullscreen.vert", frag_file);
}

void My_glProgramLocalSize(int local_x, int local_y)
{
    g_local_x = local_x;
    g_local_y = local_y;
}

/* ============================================================ dispatch ==*/


/*
Compute shader                     Fragment shader taklidi
--------------                     -----------------------
glDispatchCompute()        →        glDrawArrays()
work group sayısı          →        viewport boyutu
gl_GlobalInvocationID.xy   →        gl_FragCoord.xy
SSBO input                 →        texture input
SSBO output                →        FBO'ya bağlı texture
*/


void My_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                          GLuint num_groups_z)
{
    GLint     prog = 0;
    MyBuffer *out  = NULL;

    if (num_groups_z != 1) {
        fprintf(stderr, "My_glDispatchCompute: num_groups_z 1 olmali "
                        "(fragment yolunda katmanli render yok)\n");
        return;
    }

    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

    /* Girdi buffer'lari doku birimlerine baglanir. Shader tarafinda binding i
     * icin uBuf<i> ve uBuf<i>_size uniform'lari bekleniyor; compute'taki
     * layout(std430, binding = i) buffer bloklarinin karsiligi bu. */
    for (int i = 0; i < MY_MAX_BINDINGS; ++i) {
        MyBuffer *b = g_bound[i];
        char      name[32];

        if (!b) continue;
        if (b->fbo) { out = b; continue; }   /* cikti; doku birimine gitmez */

        glActiveTexture(GL_TEXTURE0 + (GLenum)i);
        glBindTexture(GL_TEXTURE_2D, b->tex);

        snprintf(name, sizeof(name), "uBuf%d", i);
        glUniform1i(glGetUniformLocation((GLuint)prog, name), i);
        snprintf(name, sizeof(name), "uBuf%d_size", i);
        glUniform2f(glGetUniformLocation((GLuint)prog, name),
                    (float)b->w, (float)b->h);
    }

    if (!out) {
        fprintf(stderr, "My_glDispatchCompute: cikti buffer'i baglanmamis\n");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, out->fbo);

    /* Isin dagitimi. Compute'ta num_groups_x * num_groups_y workgroup ve her
     * birinde local_x * local_y invocation vardi. Burada viewport tam o kadar
     * piksel aciliyor; rasterizer ayni sayida fragment uretiyor ve
     * gl_FragCoord.xy dogrudan gl_GlobalInvocationID.xy oluyor.
     *
     * N, local_size'in kati degilse viewport hedeften buyuk kalir. Tasan
     * fragment'lari shader'daki sinir kontrolu (discard) eler -- compute'taki
     * "return" ile ayni is. */
    glViewport(0, 0,
               (GLsizei)(num_groups_x * (GLuint)g_local_x),
               (GLsizei)(num_groups_y * (GLuint)g_local_y));

    glDrawArrays(GL_TRIANGLES, 0, 3);   /* glDispatchCompute'un karsiligi */
}




void My_glMemoryBarrier(GLbitfield barriers)
{
    /* GLES 2.0'da glMemoryBarrier yok ve gerekmiyor: fragment yazmalari
     * pipeline sirasina uyar. Geri okumadan once isin bittiginden emin olmak
     * icin glFinish yeterli. */
    (void)barriers;
    glFinish();
}

void My_glGetBufferSubData(MyBuffer *buf, float *dst)
{
    /* Veri buffer'da degil dokuda; okuma yolu da glGetBufferSubData degil
     * glReadPixels. GLES 2.0'da tek kanalli okuma yok, RGBA okuyup .r
     * kanalini ayikliyoruz. */
    size_t n    = (size_t)buf->w * (size_t)buf->h;
    float *rgba = (float *)malloc(n * 4 * sizeof(float));

    glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
    glReadPixels(0, 0, buf->w, buf->h, GL_RGBA, GL_FLOAT, rgba);

    for (size_t i = 0; i < n; ++i)
        dst[i] = rgba[i * 4];

    free(rgba);
}
