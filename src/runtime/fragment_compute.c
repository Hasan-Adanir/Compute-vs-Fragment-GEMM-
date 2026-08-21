/* Emulates compute operations with fragment shaders. */

#include "fragment_compute.h"

#include <stdio.h>
#include <stdlib.h>

/* Selects a portable float texture format. */
#ifdef MY_GLES2
#define MY_TEX_INTERNAL GL_RGBA
#else
#define MY_TEX_INTERNAL GL_RGBA32F
#endif

static MyBuffer *g_bound[MY_GL_MAX_BINDINGS];
static GLuint    g_vbo;
static int       g_local_x = 1;
static int       g_local_y = 1;

/* Initializes the fullscreen triangle. */
void My_glInit(void)
{
    /* Cover the viewport with one triangle. */
    static const GLfloat tri[6] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };

    GLint previous_array_buffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_array_buffer);
}

/* Restores an OpenGL capability. */
static void restore_capability(GLenum capability, GLboolean enabled)
{
    if (enabled) glEnable(capability);
    else glDisable(capability);
}

/* Captures the current OpenGL state. */
bool My_glBeginStateScope(MyGLStateSnapshot *state)
{
    if (!state) return false;

    glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state->framebuffer);
    glGetIntegerv(GL_VIEWPORT, state->viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state->active_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->array_buffer);

    for (int i = 0; i < MY_GL_MAX_BINDINGS; ++i) {
        glActiveTexture(GL_TEXTURE0 + (GLenum)i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture_2d[i]);
    }

    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                       &state->attrib0_enabled);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE,
                       &state->attrib0_size);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE,
                       &state->attrib0_stride);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE,
                       &state->attrib0_type);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                       &state->attrib0_normalized);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                       &state->attrib0_buffer);
    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER,
                             &state->attrib0_pointer);

    state->depth_test = glIsEnabled(GL_DEPTH_TEST);
    state->blend = glIsEnabled(GL_BLEND);
    state->cull_face = glIsEnabled(GL_CULL_FACE);
    state->scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK, state->color_mask);
    state->active = true;

    /* Restore the active texture unit. */
    glActiveTexture((GLenum)state->active_texture);
    return glGetError() == GL_NO_ERROR;
}

/* Restores the captured OpenGL state. */
void My_glEndStateScope(MyGLStateSnapshot *state)
{
    if (!state || !state->active) return;

    for (int i = 0; i < MY_GL_MAX_BINDINGS; ++i) {
        glActiveTexture(GL_TEXTURE0 + (GLenum)i);
        glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture_2d[i]);
    }
    glActiveTexture((GLenum)state->active_texture);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)state->framebuffer);
    glViewport(state->viewport[0], state->viewport[1],
               state->viewport[2], state->viewport[3]);
    glUseProgram((GLuint)state->program);

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->attrib0_buffer);
    glVertexAttribPointer(0, state->attrib0_size, (GLenum)state->attrib0_type,
                          (GLboolean)state->attrib0_normalized,
                          state->attrib0_stride, state->attrib0_pointer);
    if (state->attrib0_enabled) glEnableVertexAttribArray(0);
    else glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->array_buffer);

    restore_capability(GL_DEPTH_TEST, state->depth_test);
    restore_capability(GL_BLEND, state->blend);
    restore_capability(GL_CULL_FACE, state->cull_face);
    restore_capability(GL_SCISSOR_TEST, state->scissor_test);
    glColorMask(state->color_mask[0], state->color_mask[1],
                state->color_mask[2], state->color_mask[3]);
    state->active = false;
}

/* Creates a texture-backed buffer. */
MyBuffer My_glCreateBuffer(int w, int h, const float *data)
{
    MyBuffer b;
    GLint previous_texture = 0;
    GLint previous_framebuffer = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    b.w   = w;
    b.h   = h;
    b.fbo = 0;

    /* Store values in the red channel. */
    float *rgba = NULL;
    if (data) {
        rgba = (float *)calloc((size_t)w * (size_t)h * 4, sizeof(float));
        for (size_t i = 0; i < (size_t)w * (size_t)h; ++i)
            rgba[i * 4] = data[i];
    }

    glGenTextures(1, &b.tex);
    glBindTexture(GL_TEXTURE_2D, b.tex);
    /* Use exact texel sampling. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, MY_TEX_INTERNAL, w, h, 0,
                 GL_RGBA, GL_FLOAT, rgba);
    free(rgba);

    if (!data) {
        /* Attach output textures to an FBO. */
        glGenFramebuffers(1, &b.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, b.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, b.tex, 0);
        /* GLES 2.0 uses one color attachment. */
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fprintf(stderr, "My_glCreateBuffer: FBO tamamlanamadi "
                            "(float doku hedefi desteklenmiyor olabilir)\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    return b;
}


/* Deletes a texture-backed buffer. */
void My_glDeleteBuffer(MyBuffer *buf)
{
    if (buf->fbo) glDeleteFramebuffers(1, &buf->fbo);
    glDeleteTextures(1, &buf->tex);
    buf->tex = buf->fbo = 0;
}





/* Records a buffer binding. */
void My_glBindBufferBase(GLuint binding, MyBuffer *buf)
{
    /* Apply bindings during dispatch. */
    if (binding < MY_GL_MAX_BINDINGS)
        g_bound[binding] = buf;
}

/* Creates a fragment-based compute program. */
GLuint My_glCreateComputeProgram(const char *frag_file)
{
    /* Link the kernel with a fullscreen vertex shader. */
    return gl_program_graphics("common/fullscreen.vert", frag_file);
}

/* Sets the emulated local size. */
void My_glProgramLocalSize(int local_x, int local_y)
{
    g_local_x = local_x;
    g_local_y = local_y;
}

/* Dispatches emulated compute work. */
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

    /* Configure a deterministic pipeline state. */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    for (int i = 0; i < MY_GL_MAX_BINDINGS; ++i) {
        MyBuffer *b = g_bound[i];
        char      name[32];

        if (!b) continue;
        if (b->fbo) { out = b; continue; } /* Skip output textures. */

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

    /* Map invocations to viewport pixels. */
    glViewport(0, 0,
               (GLsizei)(num_groups_x * (GLuint)g_local_x),
               (GLsizei)(num_groups_y * (GLuint)g_local_y));

    glDrawArrays(GL_TRIANGLES, 0, 3); /* Emulate compute dispatch. */
}




/* Waits for fragment writes. */
void My_glMemoryBarrier(GLbitfield barriers)
{
    /* Finish before reading results. */
    (void)barriers;
    glFinish();
}

/* Reads values from a buffer. */
void My_glGetBufferSubData(MyBuffer *buf, float *dst)
{
    /* Read RGBA data and extract red values. */
    GLint previous_framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);

    size_t n    = (size_t)buf->w * (size_t)buf->h;
    float *rgba = (float *)malloc(n * 4 * sizeof(float));

    glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
    glReadPixels(0, 0, buf->w, buf->h, GL_RGBA, GL_FLOAT, rgba);

    for (size_t i = 0; i < n; ++i)
        dst[i] = rgba[i * 4];

    free(rgba);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
}
