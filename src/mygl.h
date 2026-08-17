/* ===========================================================================
 * mygl -- compute shader API'sinin GLES 2.0 fragment shader ile taklidi.
 *
 * comp.c'deki her compute'a ozgu cagrinin burada bir My_ karsiligi var:
 *
 *   glGenBuffers + glBufferData    ->  My_glCreateBuffer
 *   glBindBufferBase               ->  My_glBindBufferBase
 *   gl_program_compute             ->  My_glCreateComputeProgram
 *   layout(local_size_x = ...)     ->  My_glProgramLocalSize
 *   glDispatchCompute              ->  My_glDispatchCompute
 *   glMemoryBarrier                ->  My_glMemoryBarrier
 *   glGetBufferSubData             ->  My_glGetBufferSubData
 *
 * glUseProgram / glUniform / glGetUniformLocation GLES 2.0'da da aynen var,
 * o yuzden sarmalanmadi: comp.c ile frag.c'de ayni satir olarak duruyorlar.
 *
 * GLES 2.0'in dayattiklari:
 *   VAO yok          -> tek bir VBO, vertex attribute durumu global
 *   gl_VertexID yok  -> ucgenin koseleri VBO'dan attribute olarak geliyor
 *   SSBO yok         -> her buffer bir float doku; 1B indis 2B texel'e cevrilir
 *   texelFetch yok   -> normalize koordinat, (x + 0.5) / genislik
 *
 * Yapisal sinirlar (compute'un yapip fragment'in yapamadigi seyler):
 *   - scatter yok: fragment yalnizca kendi texel'ine yazar, yani sadece
 *     "cikti indisi = invocation indisi" olan kernel'ler taklit edilebilir
 *   - shared bellek, barrier(), atomik islem yok
 *   - num_groups_z 1 olmak zorunda (katmanli render yok)
 * ===========================================================================*/
#ifndef MY_GL_H
#define MY_GL_H

#include "common.h"

/* SSBO'nun karsiligi. Veri dokuda durur; cikti buffer'i ayrica bir FBO'ya
 * renk eki olarak baglanir, cunku fragment shader ancak oraya yazabilir. */
typedef struct {
    GLuint tex;
    GLuint fbo;   /* 0 ise girdi buffer'i */
    int    w, h;
} MyBuffer;

/* Tam ekran ucgenin VBO'sunu kurar. Bir kez cagrilir. */
void My_glInit(void);

/* glGenBuffers + glBufferData karsiligi.
 * data NULL ise cikti buffer'i olur (writeonly buffer'in karsiligi). */
MyBuffer My_glCreateBuffer(int w, int h, const float *data);
void     My_glDeleteBuffer(MyBuffer *buf);

/* glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buf) karsiligi.
 * Girdilerde binding = doku birimi, ciktida = render hedefi. */
void My_glBindBufferBase(GLuint binding, MyBuffer *buf);

/* gl_program_compute karsiligi: kernel'i tam ekran ucgen vertex shader'iyla
 * linkler. */
GLuint My_glCreateComputeProgram(const char *frag_file);

/* Shader'daki layout(local_size_x, local_size_y) karsiligi. Fragment yolunda
 * bu bilgi shader'da degil burada durur: viewport'un boyunu belirler. */
void My_glProgramLocalSize(int local_x, int local_y);

/* glDispatchCompute karsiligi: gx*local_x, gy*local_y boyunda bir viewport
 * acip tam ekran ucgeni cizer. Rasterizer o kadar fragment uretir. */
void My_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                          GLuint num_groups_z);

/* glMemoryBarrier karsiligi. */
void My_glMemoryBarrier(GLbitfield barriers);

/* glGetBufferSubData karsiligi: FBO'dan glReadPixels ile geri okur.
 * dst en az buf->w * buf->h float tutacak kadar buyuk olmali. */
void My_glGetBufferSubData(MyBuffer *buf, float *dst);

#endif /* MY_GL_H */
