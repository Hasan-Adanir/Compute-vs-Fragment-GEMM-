#version 120
/* GLES 2.0'da bu satir "#version 100" olur. */

/* C = A * B -- gemm.comp'un GLES 2.0 fragment shader karsiligi.
 * Ayni algoritma, satir sirasi da ayni. Cevirinin tamami su tablo:
 *
 *   gemm.comp                            gemm.frag
 *   ---------                            ---------
 *   gl_GlobalInvocationID.xy             gl_FragCoord.xy
 *   layout(std430, binding = 0) A[]      uBuf0 dokusu + uBuf0_size
 *   A[i * uK + k]                        My_bufferLoad(uBuf0, uBuf0_size, ...)
 *   C[i * uN + j] = acc                  gl_FragColor  (scatter yok, sadece
 *                                        kendi texel'ine yazabilir)
 *   if (i >= uM || j >= uN) return;      ... discard;
 *   for (k = 0; k < uK; ++k)             sabit ust sinir + break
 *   uniform int                          uniform float
 */

#ifdef GL_ES
precision highp float;   /* mediump 512 terimlik toplami tasiyamaz */
#endif

/* Binding numaralari doku birimi numaralarina donustu; her buffer icin
 * boyutu da ayrica gerekiyor, cunku 1B indisi 2B texel'e cevirecegiz. */
uniform sampler2D uBuf0;        /* A */
uniform vec2      uBuf0_size;
uniform sampler2D uBuf1;        /* B */
uniform vec2      uBuf1_size;

/* GLES 2.0'da tam sayi aritmetigi zayif (int'in genisligi garanti degil),
 * indisleri float tutmak daha guvenli. fp32 mantisi 24 bit: N = 4096'ya
 * kadar i * uK + k tam olarak temsil edilir. */
uniform float uM;   /* C'nin satir sayisi */
uniform float uN;   /* C'nin sutun sayisi */
uniform float uK;   /* toplama uzunlugu */

/* SSBO taklidi: duz dizi indisi -> 2B texel koordinati.
 * GLES 2.0'da texelFetch yok, normalize koordinat gerekiyor; texel merkezleri
 * (x + 0.5) / genislik'te oturur, NEAREST filtre ile tam o texel gelir. */
float My_bufferLoad(sampler2D buf, vec2 size, float index)
{
    float y = floor(index / size.x);
    float x = index - y * size.x;
    return texture2D(buf, (vec2(x, y) + 0.5) / size).r;
}

/* GLSL ES 1.00 dongu sinirinin derleme zamani sabiti olmasini sart kosar;
 * gercek sinira break ile ulasiyoruz. */
const int MY_MAX_K = 4096;

void main()
{
    /* My_glDispatchCompute viewport'u tam olarak dispatch izgarasi kadar
     * actigi icin gl_FragCoord.xy = gl_GlobalInvocationID.xy. Piksel
     * merkezleri 0.5'te geldiginden floor ile tam sayiya cekiyoruz. */
    float j = floor(gl_FragCoord.x);
    float i = floor(gl_FragCoord.y);

    /* N, local_size'in kati degilse tasan fragment'lar olusur. Compute'ta bu
     * "return" ile eleniyordu; fragment'in karsiligi discard. */
    if (i >= uM || j >= uN)
        discard;

    float acc = 0.0;
    for (int kk = 0; kk < MY_MAX_K; ++kk) {
        float k = float(kk);
        if (k >= uK) break;
        acc += My_bufferLoad(uBuf0, uBuf0_size, i * uK + k)    /* A[i][k] */
             * My_bufferLoad(uBuf1, uBuf1_size, k * uN + j);   /* B[k][j] */
    }

    /* C[i * uN + j] = acc. Adresi biz secemiyoruz: fragment nereye dustuyse
     * oraya yazar. Doku RGBA oldugu icin deger .r kanalinda. */
    gl_FragColor = vec4(acc, 0.0, 0.0, 1.0);
}
