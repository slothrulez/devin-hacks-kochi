#!/usr/bin/env bash
# Rebuild + restart the OpenFight noVNC preview (game on :0, clipped stream on :8080).
set -e
cd "$(dirname "$0")"

# 1. deps (idempotent)
sudo apt-get install -y -qq cmake build-essential pkg-config libsdl2-dev \
  libsdl2-image-dev libsdl2-ttf-dev libglu1-mesa-dev libglew-dev libglm-dev \
  libyaml-cpp-dev x11vnc novnc websockify >/dev/null

# 2. build (vcpkg submodule intentionally bypassed with an empty toolchain file)
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/dev/null
cmake --build build -j"$(nproc)"

# 3. game on the real display :0 (Xvfb renders it nearly black - do not use it)
pkill -x openfight 2>/dev/null || true
nohup env DISPLAY=:0 ./openfight > /tmp/openfight.log 2>&1 < /dev/null &
sleep 2

# 4. locate window, stream only that region
GEO=$(DISPLAY=:0 xwininfo -name "തെരുവ് പോരാളി" | awk '/Absolute upper-left X/{x=$4}/Absolute upper-left Y/{y=$4}/Width:/{w=$2}/Height:/{h=$2}END{print w"x"h"+"x"+"y}')
pkill x11vnc 2>/dev/null || true
nohup x11vnc -display :0 -forever -shared -nopw -rfbport 5900 \
  -clip "$GEO" -threads -wait 5 -defer 5 \
  > /tmp/x11vnc.log 2>&1 < /dev/null &

# 5. websocket bridge
pgrep -f "websockify.*8080" >/dev/null || \
  nohup websockify --web=/usr/share/novnc 8080 localhost:5900 \
  > /tmp/websockify.log 2>&1 < /dev/null &

echo "Preview: http://localhost:8080/vnc.html?autoconnect=true&resize=scale  (clip=$GEO)"
