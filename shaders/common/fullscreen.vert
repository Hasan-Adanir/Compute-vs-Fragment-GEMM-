#version 120
/* Draws a portable fullscreen triangle. */

attribute vec2 aPos;

/* Emits the fullscreen position. */
void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
