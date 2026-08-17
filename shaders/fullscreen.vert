#version 120
/* GLES 2.0'da bu satir "#version 100" olur; GLSL 1.20 ayni sozdizimini
 * (attribute / varying / gl_FragColor / texture2D) masaustunde de kabul
 * ettigi icin tek dosyayla idare ediyoruz. */

/* GLES 2.0'da gl_VertexID yok: tam ekran ucgenin uc kosesi VBO'dan attribute
 * olarak geliyor. VBO ve attribute durumu My_glInit() icinde bir kez
 * kuruluyor -- VAO da olmadigi icin o durum global ve kalici.
 *
 * Buradaki tek is, hedefin her texel'i icin bir fragment uretmek: yani
 * compute'un invocation'larini dogurmak. Asil hesap fragment shader'da. */

attribute vec2 aPos;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
