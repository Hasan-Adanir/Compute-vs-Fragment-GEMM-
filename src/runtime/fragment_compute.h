/* Emulates compute operations with GLES 2.0 fragment shaders. */
#ifndef FRAGMENT_COMPUTE_H
#define FRAGMENT_COMPUTE_H

#include "platform_support.h"

/* Stores buffer data in a texture. */
typedef struct {
    GLuint tex;
    GLuint fbo; /* Zero for input buffers. */
    int    w, h;
} MyBuffer;

#define MY_GL_MAX_BINDINGS 8

/* Stores OpenGL state for restoration. */
typedef struct {
    GLint program;
    GLint framebuffer;
    GLint viewport[4];

    GLint active_texture;
    GLint texture_2d[MY_GL_MAX_BINDINGS];
    GLint array_buffer;

    GLint attrib0_enabled;
    GLint attrib0_size;
    GLint attrib0_stride;
    GLint attrib0_type;
    GLint attrib0_normalized;
    GLint attrib0_buffer;
    void *attrib0_pointer;

    GLboolean depth_test;
    GLboolean blend;
    GLboolean cull_face;
    GLboolean scissor_test;
    GLboolean color_mask[4];
    bool active;
} MyGLStateSnapshot;

/* Captures and restores OpenGL state. */
bool My_glBeginStateScope(MyGLStateSnapshot *snapshot);
void My_glEndStateScope(MyGLStateSnapshot *snapshot);

/* Initializes the fullscreen triangle. */
void My_glInit(void);

/* Creates or deletes a buffer. */
MyBuffer My_glCreateBuffer(int w, int h, const float *data);
void     My_glDeleteBuffer(MyBuffer *buf);

/* Records a buffer binding. */
void My_glBindBufferBase(GLuint binding, MyBuffer *buf);

/* Creates a fragment-based compute program. */
GLuint My_glCreateComputeProgram(const char *frag_file);

/* Sets the emulated local size. */
void My_glProgramLocalSize(int local_x, int local_y);

/* Dispatches emulated compute work. */
void My_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                          GLuint num_groups_z);

/* Waits for fragment writes. */
void My_glMemoryBarrier(GLbitfield barriers);

/* Reads buffer values. */
void My_glGetBufferSubData(MyBuffer *buf, float *dst);

#endif /* FRAGMENT_COMPUTE_H */
