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
    log "Debian package"
    stage_source "$LAB/src"
    "${RUNNER[@]}" run --rm \
        -v "$LAB/src:/src:ro" \
        -v "$LAB/debian:/out" \
        debian:trixie \
        bash -euxo pipefail -c '
            export DEBIAN_FRONTEND=noninteractive
            apt-get update
            apt-get install -y --no-install-recommends \
                build-essential cmake debhelper extra-cmake-modules pkg-config \
                qt6-base-dev qt6-declarative-dev \
                libkf6i18n-dev libkf6notifications-dev \
                curl ffmpeg plasma-workspace ca-certificates
            rm -rf /tmp/pkg && mkdir /tmp/pkg
            cp -a /src/. /tmp/pkg/
            cd /tmp/pkg
            dpkg-buildpackage -us -uc -b
            install -m 644 /tmp/kde-plasmoid-yt-dlp_*.deb /out/
            apt-get install -y /out/kde-plasmoid-yt-dlp_*.deb
            test -f /usr/share/plasma/plasmoids/com.zrn59.ytdlp/metadata.json
            test -f /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            test -x /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/scripts/yt-dlp-update.sh
            test -f /usr/share/knotifications6/com.zrn59.ytdlp.notifyrc
            ldd /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            ldd /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so | grep -q "not found" && exit 1 || true
            printf "debian install ok\n"
        ' | tee "$LAB/debian/build.log"
}

run_arch() {
    log "Arch package"
    stage_source "$LAB/src"
    "${RUNNER[@]}" run --rm \
        -v "$LAB/src:/src:ro" \
        -v "$LAB/arch:/out" \
        archlinux:latest \
        bash -euxo pipefail -c '
            pacman -Syu --noconfirm
            pacman -S --noconfirm --needed \
                base-devel cmake extra-cmake-modules \
                qt6-base qt6-declarative ki18n knotifications \
                plasma-workspace ffmpeg curl git sudo
            id builder >/dev/null 2>&1 || useradd -m builder
            echo "builder ALL=(ALL) NOPASSWD: ALL" >/etc/sudoers.d/builder
            rm -rf /tmp/pkg && mkdir /tmp/pkg
            cp -a /src/. /tmp/pkg/kde-plasmoid-yt-dlp
            tar -C /tmp/pkg -czf /tmp/kde-plasmoid-yt-dlp.tar.gz kde-plasmoid-yt-dlp
            sum=$(sha256sum /tmp/kde-plasmoid-yt-dlp.tar.gz | awk "{print \$1}")
            sed -e "s|source=.*|source=(kde-plasmoid-yt-dlp.tar.gz)|" \
                -e "s|sha256sums=.*|sha256sums=(\"${sum}\")|" \
                -e "s|\\\$pkgname-\\\$pkgver|kde-plasmoid-yt-dlp|" \
                /src/PKGBUILD >/tmp/pkg/PKGBUILD
            cp /tmp/kde-plasmoid-yt-dlp.tar.gz /tmp/pkg/
            chown -R builder:builder /tmp/pkg
            su builder -c "cd /tmp/pkg && makepkg -sf --noconfirm"
            cp /tmp/pkg/*.pkg.tar.zst /out/ || cp /tmp/pkg/*.pkg.tar.* /out/
            pacman -U --noconfirm /out/*.pkg.tar.*
            test -f /usr/share/plasma/plasmoids/com.zrn59.ytdlp/metadata.json
            test -f /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            ldd /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so
            ldd /usr/share/plasma/plasmoids/com.zrn59.ytdlp/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so | grep -q "not found" && exit 1 || true
            printf "arch install ok\n"
        ' | tee "$LAB/arch/build.log"
}

target="${1:-all}"
case "$target" in
    debian) run_debian ;;
    arch) run_arch ;;
    all)
        run_debian
        run_arch
        ;;
    *)
        printf 'Usage: %s [debian|arch|all]\n' "$0" >&2
        exit 2
        ;;
esac
