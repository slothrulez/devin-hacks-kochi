#!/usr/bin/env bash
# Build the WebAssembly (Emscripten) target without Docker/vcpkg and serve it on :8000.
# Needs: emsdk at $EMSDK (default ~/emsdk), yaml-cpp built for wasm32 at $WASM_PREFIX.
set -e
cd "$(dirname "$0")"
EMSDK_DIR="${EMSDK:-$HOME/emsdk}"
WASM_PREFIX="${WASM_PREFIX:-$HOME/wasm-prefix}"

if [ ! -d "$EMSDK_DIR" ]; then
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  "$EMSDK_DIR/emsdk" install latest && "$EMSDK_DIR/emsdk" activate latest
fi
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null

if [ ! -f "$WASM_PREFIX/lib/libyaml-cpp.a" ]; then
  [ -d "$HOME/yaml-cpp" ] || git clone --depth 1 https://github.com/jbeder/yaml-cpp.git "$HOME/yaml-cpp"
  emcmake cmake -S "$HOME/yaml-cpp" -B "$HOME/yaml-cpp/build-wasm" -DCMAKE_BUILD_TYPE=Release \
    -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_BUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="$WASM_PREFIX"
  cmake --build "$HOME/yaml-cpp/build-wasm" -j"$(nproc)" --target install
fi

# CMakeLists uses pkg-config for SDL2/SDL2_image; the emscripten ports provide them via -sUSE_SDL*,
# so give pkg-config empty stub modules.
mkdir -p "$WASM_PREFIX/lib/pkgconfig"
printf 'Name: sdl2\nDescription: emscripten SDL2 port\nVersion: 2.28.0\nCflags:\nLibs:\n' > "$WASM_PREFIX/lib/pkgconfig/sdl2.pc"
printf 'Name: SDL2_image\nDescription: emscripten SDL2_image port\nVersion: 2.6.0\nCflags:\nLibs:\n' > "$WASM_PREFIX/lib/pkgconfig/SDL2_image.pc"

PORTS="-sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=png -sUSE_SDL_MIXER=2"
BUILD_DIR="${BUILD_DIR:-build-web}"
# emcmake supplies the Emscripten toolchain file, which takes precedence over the vcpkg one in CMakeLists.
PKG_CONFIG_LIBDIR="$WASM_PREFIX/lib/pkgconfig" emcmake cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$WASM_PREFIX" -Dyaml-cpp_DIR="$WASM_PREFIX/lib/cmake/yaml-cpp" \
  -DCMAKE_CXX_FLAGS="$PORTS -I$WASM_PREFIX/include" \
  -DCMAKE_EXE_LINKER_FLAGS="$PORTS -O2 -L$WASM_PREFIX/lib"
cmake --build "$BUILD_DIR" -j"$(nproc)"
cp "$BUILD_DIR/openfight.html" "$BUILD_DIR/index.html"

pgrep -f "http.server 8000" >/dev/null || \
  nohup python3 -m http.server 8000 --directory "$BUILD_DIR" > /tmp/openfight-web.log 2>&1 < /dev/null &
echo "Preview: http://localhost:8000/"
