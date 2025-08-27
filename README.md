# GenFX - Looping Effects Exporter (wxWidgets + FFmpeg)

App C++ com GUI (wxWidgets) que pré-visualiza efeitos com alpha e exporta 4 arquivos WebM VP8 com transparência, em loop suave (10–20s):

- 1280x720
- 720x1280
- 1920x1080
- 1080x1920

Os nomes seguem o formato `nome-em-kebab-case-WxH.webm`.

## Efeitos incluídos
- black-noise
- golden-lights
- rain
- snow
- fireflies

Todos foram adaptados para loop contínuo. Além de movimentações e fases cíclicas, a exportação aplica crossfade do último 1s para o primeiro 1s.

## Requisitos
- Windows 10/11
- CMake 3.20+
- Compilador C++17 (MSVC/Visual Studio 2019+ recomendado)
- wxWidgets instalado (headers e libs) acessíveis ao CMake
- FFmpeg disponível no PATH (o comando `ffmpeg` precisa funcionar no terminal)

### Instalando wxWidgets (opções)
1. vcpkg:
   - `vcpkg install wxwidgets:x64-windows`
   - Integre com CMake: `-DCMAKE_TOOLCHAIN_FILE=<caminho>/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows`
2. Build manual do wxWidgets e configure `WXWIN`/`wxWidgets_ROOT_DIR`. O `find_package(wxWidgets ...)` deve localizar `core` e `base`.

## Build
1. Crie pasta de build e gere projeto:
   ```powershell
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```
   Se usar vcpkg, adicione:
   ```powershell
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
   ```
2. Compile:
   ```powershell
   cmake --build build --config Release -j
   ```
3. Execute:
   ```powershell
   build/Release/genfx.exe
   ```

## Uso
- Escolha o efeito, duração (10–20s), FPS (24–60) e densidade.
- A prévia mostra BGRA com alpha. O fundo da prévia é transparente (aparece como preto onde alpha=0).
- Clique em "Exportar" e escolha a pasta. Serão criados 4 `.webm` (VP8 com `yuva420p`).
- Exportação faz crossfade no final para garantir loop suave.

## Detalhes técnicos
- GUI: `src/MainFrame.h/.cpp`, `src/main.cpp`
- Renderização dos efeitos (BGRA): `src/Renderer.h/.cpp`
- Escrita de vídeo via pipe para FFmpeg: `src/FFmpegPipe.h/.cpp`
- CMake: `CMakeLists.txt`

### FFmpeg
O encoder usa VP8 com alfa:
```
ffmpeg -f rawvideo -pix_fmt bgra -s WxH -r FPS -i - \
  -an -c:v libvpx -pix_fmt yuva420p -b:v 0 -crf 22 -g 60 \
  -deadline good -cpu-used 4 -auto-alt-ref 0 -metadata:s:v:0 alpha_mode=1 out.webm
```
- A entrada é BGRA (stdin) e a saída `yuva420p` preserva alpha.
- Ajuste `-crf` para qualidade/tamanho. `-auto-alt-ref 0` evita problemas em alguns decoders.
- `alpha_mode=1` anuncia o canal alfa no contêiner WebM.

### Loop suave
- Efeitos usam parâmetros cíclicos/warp modular. Na exportação, os últimos `1s` fazem blend com os primeiros `1s`.
- Duração recomendada de 10–20s (configurável via slider).

## Licença
Uso livre para testes; ajuste conforme sua necessidade.
