#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/ubuntu2404-release}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist}"
DOCKERFILE="${DOCKERFILE:-$ROOT_DIR/docker/ubuntu-24.04.dockerfile}"
DOCKER_IMAGE_TAG="${DOCKER_IMAGE_TAG:-beagle-stream-server-ubuntu2404-build}"
PACKAGE_BASENAME="${PACKAGE_BASENAME:-beagle-stream-server}"
PACKAGE_SERIES="${PACKAGE_SERIES:-ubuntu-24.04}"
SHORT_SHA="$(git -C "$ROOT_DIR" rev-parse --short=7 HEAD)"
EXPORT_DIR="$BUILD_DIR/export"

command -v docker >/dev/null 2>&1 || {
  echo "docker is required to build the Ubuntu 24.04 guest package on a Debian host" >&2
  exit 1
}

docker buildx version >/dev/null 2>&1 || {
  echo "docker buildx is required for local artifact export" >&2
  exit 1
}

rm -rf "$BUILD_DIR"
mkdir -p "$EXPORT_DIR" "$DIST_DIR"

docker buildx build \
  --network host \
  --build-arg BASE=ubuntu \
  --build-arg TAG=24.04 \
  --build-arg BRANCH="$(git -C "$ROOT_DIR" rev-parse --abbrev-ref HEAD)" \
  --build-arg COMMIT="$(git -C "$ROOT_DIR" rev-parse HEAD)" \
  --build-arg BUILD_VERSION="$SHORT_SHA" \
  --file "$DOCKERFILE" \
  --tag "$DOCKER_IMAGE_TAG:$SHORT_SHA" \
  --target sunshine-build \
  --output "type=local,dest=$EXPORT_DIR" \
  "$ROOT_DIR"

deb="$(find "$EXPORT_DIR" -type f -name '*.deb' | sort | head -n 1)"
test -n "$deb"

versioned_deb="$DIST_DIR/${PACKAGE_BASENAME}-${SHORT_SHA}-${PACKAGE_SERIES}-amd64.deb"
latest_deb="$DIST_DIR/${PACKAGE_BASENAME}-latest-${PACKAGE_SERIES}-amd64.deb"

cp "$deb" "$versioned_deb"
cp "$deb" "$latest_deb"

sha256sum "$versioned_deb" "$latest_deb" > "$DIST_DIR/SHA256SUMS"
printf '%s\n' "$versioned_deb" "$latest_deb" "$DIST_DIR/SHA256SUMS"