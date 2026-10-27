#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Install or update the widget's private yt-dlp binary.
# Never calls apt, never runs "yt-dlp -U", and never writes to ~/.local/bin.

set -euo pipefail

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
ROOT="${DATA_HOME}/com.zrn59.ytdlp"
BIN="${ROOT}/bin"
DEST="${BIN}/yt-dlp"
BASE="https://github.com/yt-dlp/yt-dlp/releases/latest/download"
log() {
    printf '%s\n' "$*"
}

die() {
    log "$*"
    printf '%s\n' "$*" >&2
    exit 1
}

need_curl() {
    command -v curl >/dev/null 2>&1 || die "curl is required to download yt-dlp"
    command -v sha256sum >/dev/null 2>&1 || die "sha256sum is required to verify yt-dlp"
}

fetch() {
    local url="$1" dest="$2"
    curl -fsSL --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 300 \
        -A "kde-plasmoid-yt-dlp" \
        "$url" -o "$dest"
}

is_musl() {
    [[ -e /lib/libc.musl-x86_64.so.1 || -e /lib/libc.musl-aarch64.so.1 ]] && return 0
    if command -v ldd >/dev/null 2>&1 && ldd --version >/dev/null 2>&1; then
        return 1
    fi
    ldd /bin/sh 2>&1 | grep -qi musl
}

preferred_asset() {
    local arch
    arch="$(uname -m)"
    case "$arch" in
        x86_64)
            if is_musl; then
                printf '%s\n' yt-dlp_musllinux
            else
                printf '%s\n' yt-dlp_linux
            fi
            ;;
        aarch64|arm64)
            if is_musl; then
                printf '%s\n' yt-dlp_musllinux_aarch64
            else
                printf '%s\n' yt-dlp_linux_aarch64
            fi
            ;;
        armv7l|armv6l)
            printf '%s\n' yt-dlp_linux_armv7l
            ;;
        *)
            printf '%s\n' yt-dlp
            ;;
    esac
}

checksum_for() {
    local sums="$1" asset="$2"
    grep -E "[[:space:]]\\*?${asset}$" "$sums" | awk '{ print $1; exit }' || true
}

python_ok() {
    command -v python3 >/dev/null 2>&1 \
        && python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)'
}

# Download $asset into $DEST. Prints a status line. Returns 0 on success.
install_asset() {
    local asset="$1" sums="$2" expected tmp version
    expected="$(checksum_for "$sums" "$asset")"
    if [[ -z "$expected" ]]; then
        printf 'no checksum for %s\n' "$asset" >&2
        return 1
    fi

    if [[ "$asset" == "yt-dlp" ]] && ! python_ok; then
        printf 'yt-dlp zipapp needs Python 3.9+\n' >&2
        return 1
    fi

    if [[ -f "$DEST" ]] && echo "${expected}  ${DEST}" | sha256sum -c --status 2>/dev/null; then
        version="$("$DEST" --version 2>/dev/null || echo unknown)"
        log "yt-dlp ${version} -> ${DEST} (up to date)"
        return 0
    fi

    tmp="$(mktemp "${BIN}/.yt-dlp.XXXXXX")"
    if ! fetch "${BASE}/${asset}" "$tmp"; then
        rm -f "$tmp"
        printf 'failed to download %s\n' "$asset" >&2
        return 1
    fi

    local size
    size="$(wc -c < "$tmp" | tr -d ' ')"
    if [[ "$size" -lt 500000 ]]; then
        rm -f "$tmp"
        printf 'download of %s is too small (%s bytes)\n' "$asset" "$size" >&2
        return 1
    fi
    if ! echo "${expected}  ${tmp}" | sha256sum -c --status 2>/dev/null; then
        rm -f "$tmp"
        printf 'checksum mismatch for %s\n' "$asset" >&2
        return 1
    fi

    chmod 755 "$tmp"
    mv -f "$tmp" "$DEST"
    chmod 755 "$DEST"

    if ! version="$("$DEST" --version 2>/dev/null)"; then
        rm -f "$DEST"
        printf '%s failed to start\n' "$asset" >&2
        return 1
    fi
    log "yt-dlp ${version} -> ${DEST}"
}

main() {
    need_curl
    mkdir -p "$BIN"

    exec 9>"${ROOT}/update.lock"
    if command -v flock >/dev/null 2>&1; then
        if ! flock -n 9; then
            printf 'yt-dlp update already running\n' >&2
            exit 75
        fi
    fi

    local sums preferred
    sums="$(mktemp)"
    trap "rm -f $(printf '%q' "$sums")" EXIT
    fetch "${BASE}/SHA2-256SUMS" "$sums" || die "failed to download yt-dlp checksums"

    preferred="$(preferred_asset)"
    if install_asset "$preferred" "$sums"; then
        return 0
    fi
    if [[ "$preferred" != "yt-dlp" ]]; then
        log "Falling back to the Python yt-dlp zipapp"
        install_asset "yt-dlp" "$sums" || die "could not install yt-dlp"
        return 0
    fi
    die "could not install yt-dlp"
}

main "$@"
