#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build .deb and Arch packages in containers and install them in a clean root.
# Logs and package files stay in lab/ and are not committed.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LAB="$ROOT/lab"
mkdir -p "$LAB/debian" "$LAB/arch"

if command -v docker >/dev/null 2>&1; then
    RUNNER=(docker)
elif command -v podman >/dev/null 2>&1; then
    RUNNER=(podman)
else
    printf 'docker or podman is required\n' >&2
    exit 1
fi

log() { printf '==> %s\n' "$*"; }

stage_source() {
    local dest="$1"
    rm -rf "$dest"
    mkdir -p "$dest"
    tar -C "$ROOT" \
        --exclude lab \
        --exclude build \
        --exclude .git \
        --exclude yt-dlp.png \
        -cf - . | tar -C "$dest" -xf -
}

run_debian() {
    local platform="$1"
    local debarch="$2"
    log "Debian package ($debarch)"
    stage_source "$LAB/src"
    mkdir -p "$LAB/debian/apt-cache-$debarch"
    local -a extra=()
    if [[ "$debarch" == arm64 ]]; then
        extra+=(-e DEB_BUILD_OPTIONS=parallel=1)
    fi
    "${RUNNER[@]}" run --rm --platform "$platform" \
        "${extra[@]}" \
        -e "DEB_ARCH=$debarch" \
        -v "$LAB/src:/src:ro" \
        -v "$LAB/debian:/out" \
        -v "$LAB/debian/apt-cache-$debarch:/var/cache/apt/archives" \
        debian:trixie \
        bash -euxo pipefail -c '
            export DEBIAN_FRONTEND=noninteractive
            printf "Acquire::Retries \"8\";\nAcquire::http::Timeout \"30\";\nAcquire::https::Timeout \"30\";\n" >/etc/apt/apt.conf.d/80retries
            apt-get update
            apt-get install -y --no-install-recommends ca-certificates
            sed -i "s|http://deb.debian.org|https://deb.debian.org|g" /etc/apt/sources.list.d/debian.sources
            apt-get update
            apt-get install -y --no-install-recommends \
                build-essential cmake debhelper extra-cmake-modules pkg-config \
                qt6-base-dev qt6-declarative-dev \
                libkf6i18n-dev libkf6notifications-dev
            rm -rf /tmp/pkg && mkdir /tmp/pkg
            cp -a /src/. /tmp/pkg/
            cd /tmp/pkg
            dpkg-buildpackage -us -uc -b
            shopt -s nullglob
            debs=(/tmp/kde-plasmoid-yt-dlp_[0-9]*_"${DEB_ARCH}".deb)
            test "${#debs[@]}" -gt 0
            cp -a "${debs[@]}" /out/
            rm -rf /tmp/stage && mkdir /tmp/stage
            dpkg-deb -x "${debs[0]}" /tmp/stage
            so=/tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            test -f /tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/metadata.json
            test -f "$so"
            test -x /tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/scripts/yt-dlp-update.sh
            test -f /tmp/stage/usr/share/knotifications6/com.zrn59.ytdlp.notifyrc
            ldd "$so"
            if ldd "$so" | grep -q "not found"; then
                printf "missing libraries\n" >&2
                exit 1
            fi
            printf "debian %s install ok\n" "$DEB_ARCH"
        ' >"$LAB/debian/build-$debarch.log" 2>&1
    tail -n 20 "$LAB/debian/build-$debarch.log"
}

run_arch() {
    local platform="$1"
    local cpu="$2"
    local image="$3"
    local pkgver
    pkgver=$(sed -n 's/^pkgver=//p' "$ROOT/PKGBUILD" | head -n 1)
    log "Arch package ($cpu, $pkgver)"
    stage_source "$LAB/src"
    mkdir -p "$LAB/arch/pacman-cache-$cpu"
    local -a extra=()
    if [[ "$cpu" == aarch64 ]]; then
        extra+=(-e MAKEFLAGS=-j1)
    fi
    "${RUNNER[@]}" run --rm --platform "$platform" \
        "${extra[@]}" \
        -e "PKG_CPU=$cpu" \
        -e "PKG_VER=$pkgver" \
        -v "$LAB/src:/src:ro" \
        -v "$LAB/arch:/out" \
        -v "$LAB/arch/pacman-cache-$cpu:/var/cache/pacman/pkg" \
        "$image" \
        bash -euxo pipefail -c '
            grep -q "^DisableSandbox" /etc/pacman.conf || sed -i "/^\\[options\\]/a DisableSandbox" /etc/pacman.conf
            if [ "$PKG_CPU" = aarch64 ]; then
                pacman -Syu --noconfirm
            else
                pacman -Sy --noconfirm
            fi
            pacman -S --noconfirm --needed \
                base-devel cmake extra-cmake-modules \
                qt6-base qt6-declarative ki18n knotifications \
                git sudo
            id builder >/dev/null 2>&1 || useradd -m builder
            rm -rf /tmp/pkg && mkdir -p /tmp/pkg/src
            cp -a /src/. /tmp/pkg/src/kde-plasmoid-yt-dlp-"${PKG_VER}"
            tar -C /tmp/pkg/src -czf /tmp/pkg/kde-plasmoid-yt-dlp-"${PKG_VER}".tar.gz kde-plasmoid-yt-dlp-"${PKG_VER}"
            sum=$(sha256sum /tmp/pkg/kde-plasmoid-yt-dlp-"${PKG_VER}".tar.gz | awk "{print \$1}")
            sed -e "s|^source=.*|source=(kde-plasmoid-yt-dlp-${PKG_VER}.tar.gz)|" \
                -e "s|^sha256sums=.*|sha256sums=('\''${sum}'\'')|" \
                /src/PKGBUILD >/tmp/pkg/PKGBUILD
            chown -R builder:builder /tmp/pkg
            su builder -c "cd /tmp/pkg && makepkg --noconfirm --nodeps"
            shopt -s nullglob
            pkgs=(/tmp/pkg/kde-plasmoid-yt-dlp-[0-9]*-"${PKG_CPU}".pkg.tar.*)
            test "${#pkgs[@]}" -gt 0
            cp -a /tmp/pkg/kde-plasmoid-yt-dlp-*-"${PKG_CPU}".pkg.tar.* /out/
            rm -rf /tmp/stage && mkdir /tmp/stage
            bsdtar -C /tmp/stage -xf "${pkgs[0]}"
            so=/tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            test -f /tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/metadata.json
            test -f "$so"
            test -x /tmp/stage/usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/scripts/yt-dlp-update.sh
            ldd "$so"
            if ldd "$so" | grep -q "not found"; then
                printf "missing libraries\n" >&2
                exit 1
            fi
            printf "arch %s install ok\n" "$PKG_CPU"
        ' >"$LAB/arch/build-$cpu.log" 2>&1
    tail -n 20 "$LAB/arch/build-$cpu.log"
}

target="${1:-all}"
case "$target" in
    debian) run_debian linux/amd64 amd64 ;;
    debian-arm) run_debian linux/arm64 arm64 ;;
    arch) run_arch linux/amd64 x86_64 archlinux:latest ;;
    arch-arm) run_arch linux/arm64 aarch64 lopsided/archlinux:latest ;;
    all)
        run_debian linux/amd64 amd64
        run_debian linux/arm64 arm64
        run_arch linux/amd64 x86_64 archlinux:latest
        run_arch linux/arm64 aarch64 lopsided/archlinux:latest
        ;;
    *)
        printf 'Usage: %s [debian|debian-arm|arch|arch-arm|all]\n' "$0" >&2
        exit 2
        ;;
esac
