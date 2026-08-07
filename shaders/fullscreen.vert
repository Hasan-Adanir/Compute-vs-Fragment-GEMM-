#version 410 core

/* Vertex buffer yok. gl_VertexID'den tum ekrani kaplayan tek bir ucgen
 * uretiyoruz: (-1,-1), (3,-1), (-1,3). Ucgen viewport'tan tasar, tasan kisim
 * kirpilir; iki ucgenli quad'a gore kose boyunca tekrar eden fragment olmaz.
 *
 * Buradaki tek is, hedef dokunun her texel'i icin bir fragment uretmek.
 * Asil hesap fragment shader'da. */
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
