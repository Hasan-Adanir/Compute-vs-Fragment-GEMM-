# Compute Shader'ın Fragment Shader ile Taklidi (GPGPU)

Bu proje aynı matris çarpımını (`C = A × B`) iki farklı yolla hesaplar:

1. **Compute shader** ile — GPU'da genel amaçlı hesap için normal yol.
2. **Fragment shader** ile — compute shader'ın hiç olmadığı bir ortamda
   (OpenGL ES 2.0) aynı işi yapmanın yolu.

İkinci yolda compute API'sinin her fonksiyonunun bir `My_` karşılığı yazıldı.
Örneğin `glDispatchCompute` yerine `My_glDispatchCompute` çağrılıyor. Amaç,
iki programı yan yana koyunca farkın tek bakışta görülmesi.

Ekrana bir şey çizilmiyor. Pencere gizli açılıyor (OpenGL'in bir bağlama
ihtiyacı var), hesap GPU'da yapılıyor, sonuç geri okunup CPU ile doğrulanıyor.

## Dosyalar

| Dosya | Ne yapar |
|-------|----------|
| [src/comp.c](src/comp.c) | Compute shader ile GEMM |
| [src/frag.c](src/frag.c) | Fragment shader ile GEMM — `comp.c`'nin aynası |
| [src/mygl.h](src/mygl.h) | `My_gl*` API'si: compute çağrılarının karşılıkları |
| [src/mygl.c](src/mygl.c) | Bu API'nin fragment shader ile gerçeklenmesi |
| [src/common.c](src/common.c) | GL bağlamı, shader yükleme, zamanlayıcı, CPU referansı |
| [shaders/gemm.comp](shaders/gemm.comp) | İç çarpım döngüsü — compute |
| [shaders/gemm.frag](shaders/gemm.frag) | İç çarpım döngüsü — fragment |
| [shaders/fullscreen.vert](shaders/fullscreen.vert) | Tam ekran üçgen |

`comp.c` ile `frag.c` bilerek satır satır aynı yazıldı. Aradaki farkı görmek
için:

```sh
diff -u src/comp.c src/frag.c
```

## Eşleme tablosu

Projenin özü bu tablo. Solda compute yolu, sağda fragment karşılığı.

| Compute (`comp.c` / `gemm.comp`) | Fragment (`frag.c` / `gemm.frag`) |
|---|---|
| `glGenBuffers` + `glBufferData` | `My_glCreateBuffer` — SSBO yerine doku |
| `glBindBufferBase` | `My_glBindBufferBase` — binding yerine doku birimi |
| `gl_program_compute` | `My_glCreateComputeProgram` |
| `layout(local_size_x = 16, ...)` | `My_glProgramLocalSize(16, 16)` |
| `glDispatchCompute(gx, gy, 1)` | `My_glDispatchCompute(gx, gy, 1)` |
| `glMemoryBarrier` | `My_glMemoryBarrier` |
| `glGetBufferSubData` | `My_glGetBufferSubData` — `glReadPixels` ile |
| `gl_GlobalInvocationID.xy` | `gl_FragCoord.xy` |
| `A[i * uK + k]` | `My_bufferLoad(uBuf0, uBuf0_size, i * uK + k)` |
| `C[i * uN + j] = acc` | `gl_FragColor = vec4(acc, 0, 0, 1)` |
| `if (taşma) return;` | `if (taşma) discard;` |

`glUseProgram`, `glUniform*` ve `glGetUniformLocation` OpenGL ES 2.0'da da
aynen var. Bu yüzden sarmalanmadılar, iki dosyada da aynı satır olarak
duruyorlar.

### `My_glDispatchCompute` ne yapıyor

Compute shader'da işi biz dağıtıyoruz: `num_groups × local_size` kadar
invocation başlatılıyor. Fragment shader'da bunu **rasterizer** yapar. O yüzden
`My_glDispatchCompute` şunları yapıyor:

1. Viewport'u tam olarak `num_groups × local_size` piksel boyutunda açar.
2. Ekranı kaplayan tek bir üçgen çizer.
3. Rasterizer o kadar fragment üretir. Her fragment bir invocation olur ve
   kendi `(i, j)` indisini `gl_FragCoord`'dan okur.

## OpenGL ES 2.0 kısıtları

Fragment yolu, OpenGL ES 2.0'da olmayan hiçbir şeyi kullanmaz:

| Yok olan | Yerine ne kullanıldı |
|---|---|
| VAO | Sadece bir VBO; attribute durumu global |
| `gl_VertexID` | Üçgenin üç köşesi VBO'dan `attribute vec2 aPos` ile |
| SSBO | Float doku; 1B dizi indisi 2B texel koordinatına çevriliyor |
| `texelFetch` | `texture2D` + normalize koordinat `(x + 0.5) / genişlik` |
| Tek kanallı float doku | RGBA doku, değer `.r` kanalında |
| `out` değişkeni, MRT | `gl_FragColor`, tek renk eki |
| `glDrawBuffers`, `glReadBuffer` | Hiçbiri gerekmiyor, tek ek var |
| `layout(location = ...)` | Link'ten önce `glBindAttribLocation` |
| Dinamik döngü sınırı | Sabit üst sınır + `break` |
| `glGetBufferSubData` | `glReadPixels` |

Bağlam açılırken **core profile istenmiyor**, çünkü core profile çizim için
VAO bağlı olmasını zorunlu kılar — ES 2.0'da ise VAO diye bir şey yoktur.

Gerçek bir ES 2.0 bağlamına taşımak için iki değişiklik yeter: shader'lardaki
`#version 120` satırlarını `#version 100` yapmak ve derlerken `MY_GLES2`
tanımlamak (doku formatı otomatik uyum sağlar).

## Fragment yolunun yapamadıkları

Bunlar eksik iş değil, yöntemin yapısal sınırları:

- **Scatter yok.** Fragment yalnızca kendi texel'ine yazabilir. Yani sadece
  "çıktı indisi = invocation indisi" olan çekirdekler taklit edilebilir.
  Naive GEMM böyledir, ama örneğin histogram veya prefix-sum böyle değildir.
- **`shared` bellek ve `barrier()` yok.** Compute'un asıl hız kazancı olan
  tiled GEMM bu yolla yazılamaz.
- **Atomik işlem yok.**
- **`num_groups_z` 1 olmak zorunda.**

## Kurulum ve çalıştırma

Gereken: CMake ≥ 3.16, C11 derleyici, GLFW 3.3+ (bulunamazsa CMake indirir).
GLAD (`gl:core=4.3`) repoda hazır geliyor.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

./build/comp 512                 # compute yolu
./build/frag 512                 # fragment yolu
./build/frag 4 --print           # matrisleri ekrana bas
./build/comp 2048 --noverify     # fp64 referansı atla (büyük N'de yavaş)
```

Windows / Visual Studio için `--config Release` gerekir ve programlar
`build\Release\` altına düşer:

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\comp.exe 1024 --noverify
build\Release\frag.exe 1024 --noverify
```

Komut satırı seçenekleri:

| Seçenek | Anlamı |
|---|---|
| `<sayı>` | Matris boyutu N (varsayılan 512) |
| `--print` | A, B ve C'nin sol üst 8×8 köşesini basar |
| `--noverify` | fp64 CPU doğrulamasını atlar |

Shader'lar çalışma anında `shaders/` klasöründen okunur. Shader'ı düzenleyip
programı yeniden çalıştırmak yeterli, yeniden derlemek gerekmez.

## Platform notu

- **Linux, Windows:** her iki program da çalışır.
- **macOS:** çalışmaz. Sürücü compatibility profile'ı OpenGL 2.1'de bırakıyor,
  FBO ve float doku için en az 3.0 gerekiyor. Programlar bunu tespit edip
  açıklayıcı bir mesajla çıkar. Ayrıca compute shader 4.3 ister, macOS 4.1'de
  tavan yapar.

Pratik iş bölümü: kod yazma macOS'ta, ölçümler Linux veya Windows'ta.

## Ölçüm yöntemi

- GPU süresi: `GL_TIME_ELAPSED` sorgusu — sadece GPU'da geçen zaman.
- 3 ısınma koşusu + 10 ölçüm koşusu, **medyan** raporlanır.
- `GFLOP/s = 2·N³ / t`.
- Doğruluk: fp64 akümülatörlü referansa karşı
  `|ref − test| ≤ abs_tol + rel_tol·|ref|` (numpy `allclose` ölçütü).

Saf bağıl hata bu problemde yanıltıcı olurdu: rastgele `[-1,1]` matrislerin
çarpımında bazı sonuç hücreleri sıfıra çok yakın düşer, payda çökünce hata
yapay olarak büyür. Bu yüzden `abs_tol` sonucun büyüklüğüne (`1e-5 · max|C|`)
göre ölçeklenir.

## Sonuçlar

Apple M2, OpenGL 4.1 (proje ES 2.0'a taşınmadan önceki ölçümler):

| N | CPU blocked | fragment | hızlanma |
|---|---|---|---|
| 100 | 0.11 ms (18.7 GFLOP/s) | 0.21 ms (9.5 GFLOP/s) | 0.51x |
| 512 | 12.60 ms (21.3 GFLOP/s) | 3.13 ms (85.9 GFLOP/s) | 4.03x |
| 1024 | 109.16 ms (19.7 GFLOP/s) | 24.23 ms (88.6 GFLOP/s) | 4.50x |

İki gözlem:

- **N=100'de GPU daha yavaş.** Çizim çağrısının ve senkronizasyonun maliyeti,
  hesabın kendisinden büyük. GPU'nun kazanmaya başladığı bir eşik var.
- **Fragment ~86–88 GFLOP/s'te doyuyor.** N iki katına çıkınca iş 8 katına
  çıkıyor ama GFLOP/s sabit kalıyor. Yani darboğaz hesap değil, bellek
  trafiği: shared memory olmadığı için her fragment aynı satır ve sütunları
  tekrar tekrar okuyor.

Compute yolunun ölçümleri henüz alınmadı (Linux/Windows makinesi gerekiyor).

## Sırada ne var

| Varyant | Fikir | Durum |
|---|---|---|
| `frag` | Fragment başına 1 hücre, `My_gl*` API'si üzerinden | ✅ |
| `comp` | Invocation başına 1 hücre, `local_size 16×16` | ✅ (ölçüm bekliyor) |
| `comp_tiled` | `shared` tile + `barrier()` | ⬜ |
| `frag_vec4` | RGBA'nın dört kanalı = dört `k` adımı, `dot()` ile | ⬜ |
