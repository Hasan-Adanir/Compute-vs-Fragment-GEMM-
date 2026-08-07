# Fragment Shader ile Matris Çarpımı (GPGPU)

Compute shader ile yapılan bir GEMM (`C = A × B`) örneğinin **fragment shader**
ile yeniden kurulması ve iki yolun ölçümlü karşılaştırılması.

Görsel çıktı yok. Pencere gizli açılır (OpenGL'in bir bağlama ihtiyacı olduğu
için), hesap framebuffer'a / SSBO'ya yapılır, sonuç geri okunup CPU referansıyla
doğrulanır.

## Dosyalar

Her varyant kendi `main()`'i olan bağımsız bir program; baştan sona okunabilir.

| Dosya | Ne yapar |
|-------|----------|
| [src/frag_naive.c](src/frag_naive.c) | Fragment shader ile GEMM — asıl örnek |
| [src/comp_naive.c](src/comp_naive.c) | Compute shader ile GEMM — karşılaştırma |
| [src/common.c](src/common.c) | GL bağlamı, shader yükleme, zamanlayıcı, CPU referansı |
| [shaders/fullscreen.vert](shaders/fullscreen.vert) | Tam ekran üçgen (vertex buffer yok) |
| [shaders/gemm_naive.frag](shaders/gemm_naive.frag) | İç çarpım döngüsü — fragment |
| [shaders/gemm_naive.comp](shaders/gemm_naive.comp) | İç çarpım döngüsü — compute |

`common.c`'de iş mantığı yok, sadece tekrar eden kurulum kodu var. Anlatılmak
istenen şey varyant dosyalarının içinde.

## Nasıl çalışıyor

**Fragment yolu.** A ve B birer `GL_R32F` dokuya yüklenir. Çıktı için N×N'lik
bir doku FBO'ya renk eki olarak bağlanır, viewport N×N yapılır ve tam ekran bir
üçgen çizilir. Rasterizer her hedef texel için bir fragment üretir; fragment
kendi `(i, j)` indisini `gl_FragCoord`'dan okur ve N adımlık iç çarpımı
hesaplar. Sonuç `glReadPixels` ile geri alınır.

```
rasterizer → iş dağıtıcı        fragment → thread
doku       → read-only buffer   FBO      → çıktı buffer'ı
```

**Compute yolu.** Aynı algoritma; veri dokuda değil SSBO'da, iş dağıtımını
rasterizer değil `glDispatchCompute` yapıyor, çıktı tek bir piksele bağlı değil.

**Aradaki fark neden önemli.** Fragment shader **scatter yapamaz** — yalnızca
kendi pikseline yazar, sadece gather yapar. Ve **shared memory'si yoktur**:
aynı A satırı / B sütunu her fragment tarafından doku önbelleği üzerinden
yeniden okunur. Compute shader'ın tiled versiyonu bir tile'ı `shared` diziye bir
kez okuyup defalarca kullanabilir. Projenin ölçmek istediği fark bu.

## Kurulum ve çalıştırma

Gereken: CMake ≥ 3.16, C11 derleyici, GLFW 3.3+.
GLAD (`gl:core=4.3`) repoda vendor'lanmıştır. GLFW sistemde bulunamazsa CMake
kaynaktan indirir.

```sh
brew install glfw cmake            # macOS

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

./build/frag_naive                 # N = 512 (varsayılan)
./build/frag_naive 1024            # N seçerek
./build/frag_naive 2048 --noverify # fp64 referansı atla (büyük N'de yavaş)
./build/comp_naive 1024
```

Shader'lar çalışma anında `shaders/` klasöründen okunur — düzenleyip programı
yeniden çalıştırmak yeterli, yeniden derleme gerekmez.

## Platform notu — önemli

macOS OpenGL 4.1'de tavan yapar, **compute shader 4.3 gerektirir**. Programlar
önce 4.3 core bağlamı ister, alamazsa 4.1 core'a düşer. `comp_naive` Mac'te
derlenir ama çalışırken açıklayıcı bir mesajla çıkar (exit 2). `frag_naive`
her iki platformda da çalışır.

Pratik iş bölümü: geliştirme macOS'ta, compute ölçümleri GL 4.3+ destekleyen
bir makinede.

## Ölçüm

- GPU: `GL_TIME_ELAPSED` sorgusu — sadece GPU'da geçen süre.
- 3 warmup + 10 koşu, **medyan** raporlanır.
- `GFLOP/s = 2·N³ / t`.
- Doğruluk: fp64 akümülatörlü referansa karşı karışık tolerans
  `|ref − test| ≤ abs_tol + rel_tol·|ref|` (numpy `allclose` ölçütü).

Saf bağıl hata bu problemde yanıltıcı olurdu: rastgele `[-1,1]` matrislerin
çarpımında bazı sonuç hücreleri sıfıra çok yakın düşer, payda çökünce hata
yapay olarak büyür. `abs_tol` sonucun büyüklüğüne (`1e-5 · max|C|`) göre
ölçeklenir.

## Şimdiye kadarki sonuçlar

Apple M2, OpenGL 4.1:

| N | CPU blocked | fragment | hızlanma |
|---|---|---|---|
| 100 | 0.11 ms (18.7 GFLOP/s) | 0.21 ms (9.5 GFLOP/s) | 0.51x |
| 512 | 12.60 ms (21.3 GFLOP/s) | 3.13 ms (85.9 GFLOP/s) | 4.03x |
| 1024 | 109.16 ms (19.7 GFLOP/s) | 24.23 ms (88.6 GFLOP/s) | 4.50x |

Doğruluk her N'de geçiyor (N=512'de `max_abs 3.24e-05`, `rms 3.06e-06`).

Not edilmeye değer iki nokta:

- **N=100'de GPU daha yavaş.** Çizim çağrısı ve senkronizasyon maliyeti
  hesaptan büyük. GPU'nun kazanmaya başladığı bir eşik var.
- **Fragment ~86–88 GFLOP/s'te doyuyor.** N iki katına çıkınca iş 8 katına
  çıkıyor ama GFLOP/s sabit kalıyor — yani darboğaz hesap değil, bellek/önbellek
  trafiği. Shared memory olmadığı için her fragment aynı satır ve sütunları
  yeniden okuyor. Tiled compute varyantının açması gereken tıkanma tam burası.

## Sırada ne var

| Varyant | Fikir | Durum |
|---|---|---|
| `frag_naive` | fragment başına 1 hücre | ✅ |
| `comp_naive` | invocation başına 1 hücre, `local_size 16×16` | ✅ (GL 4.3 makinesinde ölçülecek) |
| `comp_tiled` | `shared` tile + `barrier()` | ⬜ |
| `frag_vec4` | `RGBA32F` paketli, 1 texel = 4 `k`, `dot()` | ⬜ |
