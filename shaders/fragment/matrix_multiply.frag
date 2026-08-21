#version 120

#ifdef GL_ES
precision highp float; /* Preserve long sums. */
#endif

uniform sampler2D uBuf0;        /* A */
uniform vec2      uBuf0_size;
uniform sampler2D uBuf1;        /* B */
uniform vec2      uBuf1_size;

uniform float uM; /* Output rows. */
uniform float uN; /* Output columns. */
uniform float uK; /* Sum length. */

/* Loads one buffer value. */
float My_bufferLoad(sampler2D buf, vec2 size, float index)
{
    float y = floor(index / size.x);
    float x = index - y * size.x;
    return texture2D(buf, (vec2(x, y) + 0.5) / size).r;
}

const int MY_MAX_K = 4096;

/* Computes one output value. */
void main()
{
    float j = floor(gl_FragCoord.x);
    float i = floor(gl_FragCoord.y);


    if (i >= uM || j >= uN)
        discard;

    float acc = 0.0;
    for (int kk = 0; kk < MY_MAX_K; ++kk) {
        float k = float(kk);
        if (k >= uK) break;
        acc += My_bufferLoad(uBuf0, uBuf0_size, i * uK + k)    /* A[i][k] */
             * My_bufferLoad(uBuf1, uBuf1_size, k * uN + j);   /* B[k][j] */
    }

    gl_FragColor = vec4(acc, 0.0, 0.0, 1.0);
}
