#version 410 core

/* C = A * B, fragment shader ile.
 *
 * Her fragment sonuc matrisinin tek bir hucresini hesaplar. Rasterizer is
 * dagiticisi gorevini gorur: N x N'lik hedefe cizim yapinca N*N fragment
 * uretilir ve her biri kendi (i, j) indisini gl_FragCoord'dan okur.
 *
 * Yerlesim:
 *   uA dokusu: genislik K, yukseklik M  -> texel(k, i) = A[i][k]
 *   uB dokusu: genislik N, yukseklik K  -> texel(j, k) = B[k][j]
 *   hedef     : genislik N, yukseklik M -> fragment(j, i) = C[i][j]
 *
 * Compute shader'dan farki: burada nereye yazacagimizi secemiyoruz. Fragment
 * yalnizca kendi pikseline yazar (scatter yok, sadece gather). Ayrica paylasimli
 * bellek olmadigi icin ayni A satiri / B sutunu her fragment tarafindan
 * dokuma onbellegi uzerinden tekrar tekrar okunur. */

uniform sampler2D uA;
uniform sampler2D uB;
uniform int       uK;   /* toplama uzunlugu (A'nin sutun sayisi) */

out float outC;

void main()
{
    /* gl_FragCoord piksel merkezlerinde (0.5, 1.5, ...) gelir; tam sayiya
     * cevirince dogrudan texel indisi olur. */
    ivec2 idx = ivec2(gl_FragCoord.xy);
    int j = idx.x;
    int i = idx.y;

    float acc = 0.0;
    for (int k = 0; k < uK; ++k) {
        /* texelFetch: normalize edilmemis tam sayi indis, filtreleme yok,
         * mipmap yok -- tam olarak istedigimiz "diziden oku" davranisi. */
        float a = texelFetch(uA, ivec2(k, i), 0).r;
        float b = texelFetch(uB, ivec2(j, k), 0).r;
        acc += a * b;
    }

    outC = acc;
}
