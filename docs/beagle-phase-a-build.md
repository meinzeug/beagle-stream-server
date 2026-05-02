# BeagleStream Server Phase A Build

This fork keeps upstream Sunshine as the default build. Beagle Control Plane
integration is opt-in via `BEAGLE_INTEGRATION`.

## Ubuntu 24.04 build host

Install the build prerequisites used for the Phase A verification build:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config curl \
  libssl-dev libcurl4-openssl-dev libopus-dev \
  libminiupnpc-dev libcap-dev libnuma-dev libva-dev libvdpau-dev \
  libdrm-dev libevdev-dev libxtst-dev libx11-dev libxrandr-dev libxfixes-dev \
  libayatana-appindicator3-dev libnotify-dev \
  libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev libxcb-randr0-dev \
  libxcb-shape0-dev libxcb-image0-dev libxcb-keysyms1-dev \
  libxcb-icccm4-dev libxcb-render-util0-dev
```

Initialize the submodules needed by the Linux server build:

```bash
git submodule update --init \
  third-party/doxyconfig \
  third-party/glad \
  third-party/googletest \
  third-party/inputtino \
  third-party/libdisplaydevice \
  third-party/moonlight-common-c \
  third-party/nanors \
  third-party/nv-codec-headers \
  third-party/Simple-Web-Server \
  third-party/TPCircularBuffer \
  third-party/tray \
  third-party/wayland-protocols
git -C third-party/moonlight-common-c submodule update --init enet nanors/deps/simde
```

## Beagle build

For a release build on a full Sunshine build host:

```bash
cmake -S . -B build/beagle-release \
  -DBEAGLE_INTEGRATION=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/beagle-release --target sunshine -j"$(nproc)"
```

For a minimal CI/dev host without CUDA, Vulkan, Wayland portal or tray support:

```bash
cmake -S . -B build/beagle-check \
  -DBEAGLE_INTEGRATION=ON \
  -DBUILD_TESTS=OFF \
  -DBUILD_DOCS=OFF \
  -DNPM_OFFLINE=ON \
  -DSUNSHINE_ENABLE_CUDA=OFF \
  -DCUDA_FAIL_ON_MISSING=OFF \
  -DSUNSHINE_ENABLE_VULKAN=OFF \
  -DSUNSHINE_ENABLE_WAYLAND=OFF \
  -DSUNSHINE_ENABLE_PORTAL=OFF \
  -DSUNSHINE_ENABLE_TRAY=ON
cmake --build build/beagle-check --target sunshine -j"$(nproc)"
```

Runtime configuration is read from `/etc/beagle/stream-server.env`. Tokens must
not be committed. TLS verification is enabled by default; only set
`BEAGLE_TLS_INSECURE=1` for local test systems.

## Release DEB

The `BeagleStream Server Release` workflow runs on `beagle/phase-a` pushes and
publishes a mutable prerelease named `beagle-phase-a`.

Stable Beagle OS VM guest-prep URL:

```text
https://github.com/meinzeug/beagle-stream-server/releases/download/beagle-phase-a/beagle-stream-server-latest-ubuntu-24.04-amd64.deb
```

Beagle OS VM provisioning tries this package first and falls back to the
upstream Sunshine `.deb` only if this mutable Phase A asset is not available.
