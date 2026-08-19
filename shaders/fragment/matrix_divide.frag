#version 120
#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D uBuf0;
uniform vec2 uBuf0_size;
uniform sampler2D uBuf1;
uniform vec2 uBuf1_size;
uniform float uM;
uniform float uN;

float bufferLoad(sampler2D bufferTexture, vec2 size, float index)
{
    float y = floor(index / size.x);
    float x = index - y * size.x;
    return texture2D(bufferTexture, (vec2(x, y) + 0.5) / size).r;
}

void main()
{
    float column = floor(gl_FragCoord.x);
    float row = floor(gl_FragCoord.y);
    if (row >= uM || column >= uN) discard;
    float index = row * uN + column;
    float result = bufferLoad(uBuf0, uBuf0_size, index)
                 / bufferLoad(uBuf1, uBuf1_size, index);
    gl_FragColor = vec4(result, 0.0, 0.0, 1.0);
}
