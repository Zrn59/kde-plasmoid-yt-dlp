#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Install, update, repair, or remove the kde-plasmoid-yt-dlp widget.
# Online:
#   bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"

set -euo pipefail

REPO_URL="https://github.com/Zrn59/kde-plasmoid-yt-dlp.git"
PLASMOID_ID="com.zrn59.ytdlp"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
RUNTIME="${DATA_HOME}/com.zrn59.ytdlp"
RUNTIME_BIN="${RUNTIME}/bin"
SYSTEMD_USER="${CONFIG_HOME}/systemd/user"
UNIT_NAME="com.zrn59.ytdlp-update"
OLD_QML_DIR="${HOME}/.local/lib/qml"
CACHE_SRC="${XDG_CACHE_HOME:-$HOME/.cache}/kde-plasmoid-yt-dlp/src"

ROOT=""
BUILD_DIR=""
PLASMA_STOPPED=0
SKIP_RESTART=0
SKIP_RUNTIME=0
ACTION=""

log() { printf '==> %s\n' "$*"; }
warn() { printf 'Warning: %s\n' "$*" >&2; }
die() { printf 'Error: %s\n' "$*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1; }

usage() {
    cat <<EOF
Usage: install.sh [action] [options]

Actions:
  --install        Build and install
  --update         Download the latest source, then install
  --repair         Install again without downloading new source
  --uninstall      Remove the widget and its private yt-dlp

Options:
  --skip-restart   Do not reload Plasma
  --skip-runtime   Do not download yt-dlp or Deno
  -h, --help       Show this help

With no action, a terminal shows a menu. Without a terminal, --install is used.

Online:
  bash -c "\$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
EOF
}

script_dir() {
    if [[ -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]}" ]]; then
        cd "$(dirname "${BASH_SOURCE[0]}")" && pwd
    fi
}

sources_ready() {
    [[ -n "$ROOT" && -f "$ROOT/CMakeLists.txt" ]]
}

download_and_exec() {
    need_cmd git || die "git is required to download kde-plasmoid-yt-dlp"
    log "Downloading kde-plasmoid-yt-dlp"
    if [[ -d "$CACHE_SRC/.git" ]]; then
        git -C "$CACHE_SRC" pull --ff-only
    else
        rm -rf "$CACHE_SRC"
        git clone --depth 1 "$REPO_URL" "$CACHE_SRC"
    fi
    exec "$CACHE_SRC/install.sh" "$@"
}

print_dep_help() {
    local id like
    id="$(. /etc/os-release && printf '%s' "${ID:-}")"
    like="$(. /etc/os-release && printf '%s' "${ID_LIKE:-}")"
    echo
    echo "Build dependencies are missing. Install them, then run the installer again."
    echo
    case "${id}:${like}" in
        fedora:*|*:fedora|rhel:*|*:rhel)
            echo "  sudo dnf install cmake gcc-c++ extra-cmake-modules \\"
            echo "    qt6-qtbase-devel qt6-qtdeclarative-devel \\"
            echo "    kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel \\"
            echo "    ffmpeg curl git"
            ;;
        arch:*|*:arch)
            echo "  sudo pacman -S --needed cmake gcc extra-cmake-modules \\"
            echo "    qt6-base qt6-declarative ki18n knotifications kpackage \\"
            echo "    ffmpeg curl git"
            ;;
        opensuse*|sles*|*:suse*)
            echo "  sudo zypper install cmake gcc-c++ extra-cmake-modules \\"
            echo "    qt6-base-devel qt6-declarative-devel \\"
            echo "    kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel \\"
            echo "    ffmpeg curl git"
            ;;
        *)
            echo "  sudo apt install cmake g++ extra-cmake-modules pkg-config \\"
            echo "    qt6-base-dev qt6-declarative-dev \\"
            echo "    libkf6i18n-dev libkf6notifications-dev kpackagetool6 \\"
            echo "    ffmpeg curl git"
            ;;
    esac
    echo
}

start_plasma() {
    if [[ "$PLASMA_STOPPED" != 1 ]]; then
        return 0
    fi
    PLASMA_STOPPED=0
    if ! systemctl --user start plasma-plasmashell.service; then
        warn "Plasma did not start. Run: systemctl --user start plasma-plasmashell.service"
    fi
}

on_exit() {
    local status=$?
    trap - EXIT
    start_plasma || true
    exit "$status"
}

stop_plasma() {
    if [[ "$SKIP_RESTART" == 1 ]]; then
        return 0
    fi
    if systemctl --user is-active --quiet plasma-plasmashell.service; then
        log "Reloading Plasma"
        systemctl --user stop plasma-plasmashell.service
        PLASMA_STOPPED=1
    fi
}

clean_import_var() {
    local name="$1" value part joined="" oldifs
    value="$(systemctl --user show-environment 2>/dev/null | sed -n "s/^${name}=//p" || true)"
    [[ -n "$value" ]] || return 0
    oldifs="$IFS"
    IFS=':'
    for part in $value; do
        [[ -z "$part" || "$part" == "$OLD_QML_DIR" ]] && continue
        if [[ -n "$joined" ]]; then
            joined="${joined}:${part}"
        else
            joined="$part"
        fi
    done
    IFS="$oldifs"
    if [[ -z "$joined" ]]; then
        systemctl --user unset-environment "$name" >/dev/null 2>&1 || true
    else
        systemctl --user set-environment "${name}=${joined}" >/dev/null 2>&1 || true
    fi
}

remove_qml_path_hacks() {
    local env_dir="${CONFIG_HOME}/plasma-workspace/env"
    local dropin="${SYSTEMD_USER}/plasma-plasmashell.service.d/qml-import.conf"
    rm -f "${env_dir}/ytdlp-qml.sh"
    if [[ -f "${env_dir}/qmlimports.sh" ]] && grep -q 'kde-plasmoid-yt-dlp' "${env_dir}/qmlimports.sh"; then
        rm -f "${env_dir}/qmlimports.sh"
    fi
    if [[ -f "$dropin" ]] && grep -q "${OLD_QML_DIR}" "$dropin"; then
        if grep -vE '^(#|\[Service\]|[[:space:]]*$)' "$dropin" \
            | grep -vqE "^Environment=QML2?_IMPORT_PATH=${OLD_QML_DIR}$"; then
            warn "Left ${dropin} in place because it has other settings."
            warn "Remove QML_IMPORT_PATH lines that point only at ${OLD_QML_DIR}."
        else
            rm -f "$dropin"
        fi
    fi
    clean_import_var QML_IMPORT_PATH
    clean_import_var QML2_IMPORT_PATH
    rm -rf "${OLD_QML_DIR}/com/zrn59/ytdlp"
    rmdir "${OLD_QML_DIR}/com/zrn59" "${OLD_QML_DIR}/com" "${OLD_QML_DIR}" 2>/dev/null || true
}

remove_old_updater() {
    local old_unit="${SYSTEMD_USER}/yt-dlp-update.service"
    local old_script="${HOME}/.local/bin/yt-dlp-update.sh"
    if [[ -f "$old_unit" ]] && grep -q 'yt-dlp-update.sh' "$old_unit"; then
        systemctl --user disable --now yt-dlp-update.timer >/dev/null 2>&1 || true
        rm -f "$old_unit" "${SYSTEMD_USER}/yt-dlp-update.timer"
    fi
    if [[ -f "$old_script" ]] && grep -q 'releases/latest/download/yt-dlp' "$old_script"; then
        rm -f "$old_script"
    fi
    local old_notify="${DATA_HOME}/knotifications6/ytdlp.notifyrc"
    if [[ -f "$old_notify" ]] && grep -q 'Event/download' "$old_notify"; then
        rm -f "$old_notify"
    fi
}

find_tool() {
    local name candidate
    for candidate in \
        "${RUNTIME_BIN}/$1" \
        "${HOME}/.local/bin/$1" \
        "/usr/local/bin/$1" \
        "/usr/bin/$1"
    do
        if [[ -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    if command -v "$1" >/dev/null 2>&1; then
        command -v "$1"
        return 0
    fi
    return 1
}

install_deno() {
    local existing
    if existing="$(find_tool deno)"; then
        log "JavaScript runtime: ${existing}"
        return 0
    fi
    if existing="$(find_tool node)"; then
        log "JavaScript runtime: ${existing}"
        return 0
    fi
    if ! need_cmd curl; then
        warn "curl is missing, so Deno was not installed. YouTube may need it."
        return 0
    fi
    local arch zip url tmp
    arch="$(uname -m)"
    case "$arch" in
        x86_64) zip="deno-x86_64-unknown-linux-gnu.zip" ;;
        aarch64|arm64) zip="deno-aarch64-unknown-linux-gnu.zip" ;;
        *)
            warn "No Deno build for ${arch}. Install deno or node yourself if YouTube asks for it."
            return 0
            ;;
    esac
    log "Installing Deno for this widget (YouTube)"
    url="https://github.com/denoland/deno/releases/latest/download/${zip}"
    tmp="$(mktemp -d)"
    if ! curl -fsSL --retry 3 --retry-delay 2 -A "kde-plasmoid-yt-dlp" "$url" -o "${tmp}/deno.zip"; then
        rm -rf "$tmp"
        warn "Could not download Deno. YouTube downloads may ask for it later."
        return 0
    fi
    if need_cmd unzip; then
        unzip -qo "${tmp}/deno.zip" -d "$tmp"
    else
        python3 - "${tmp}/deno.zip" "$tmp" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY
    fi
    if [[ ! -x "${tmp}/deno" ]]; then
        rm -rf "$tmp"
        warn "Deno archive did not contain a binary."
        return 0
    fi
    mkdir -p "$RUNTIME_BIN"
    install -m 755 "${tmp}/deno" "${RUNTIME_BIN}/deno"
    rm -rf "$tmp"
    log "Deno $("${RUNTIME_BIN}/deno" --version 2>/dev/null | head -1 || echo installed)"
}

install_package() {
    local stage="$1"
    local pkg="${stage}/share/plasma/plasmoids/${PLASMOID_ID}"
    [[ -f "${pkg}/metadata.json" ]] || die "package metadata missing after build"
    [[ -f "${pkg}/contents/qml/com/zrn59/ytdlp/libytdlpplugin.so" ]] || die "QML plugin was not built"
    mkdir -p "${DATA_HOME}/knotifications6" "$RUNTIME_BIN"
    install -m 644 "${stage}/share/knotifications6/com.zrn59.ytdlp.notifyrc" \
        "${DATA_HOME}/knotifications6/com.zrn59.ytdlp.notifyrc"
    install -m 755 "${stage}/share/com.zrn59.ytdlp/bin/yt-dlp-update.sh" \
        "${RUNTIME_BIN}/yt-dlp-update.sh"

    if kpackagetool6 --type Plasma/Applet --show "$PLASMOID_ID" >/dev/null 2>&1; then
        if ! kpackagetool6 --type Plasma/Applet --upgrade "$pkg"; then
            warn "Widget files are in use. Reloading Plasma and trying again."
            stop_plasma
            kpackagetool6 --type Plasma/Applet --upgrade "$pkg"
        fi
    else
        kpackagetool6 --type Plasma/Applet --install "$pkg"
    fi
}

enable_timer() {
    local unit_src="${SYSTEMD_USER}/${UNIT_NAME}.service"
    mkdir -p "$SYSTEMD_USER"
    sed "s|@YTDLP_UPDATE_BIN@|${RUNTIME_BIN}/yt-dlp-update.sh|" \
        "${ROOT}/systemd/com.zrn59.ytdlp-update.service.in" > "$unit_src"
    install -m 644 "${ROOT}/systemd/com.zrn59.ytdlp-update.timer" \
        "${SYSTEMD_USER}/${UNIT_NAME}.timer"
    systemctl --user daemon-reload
    if ! systemctl --user enable --now "${UNIT_NAME}.timer"; then
        warn "Could not enable the daily yt-dlp update timer."
    fi
}

finish_reload() {
    if [[ "$PLASMA_STOPPED" == 1 ]]; then
        start_plasma
    elif [[ "$SKIP_RESTART" != 1 ]] && systemctl --user is-active --quiet plasma-plasmashell.service; then
        log "Reloading Plasma"
        systemctl --user restart plasma-plasmashell.service
    fi
}

do_install() {
    need_cmd cmake || { print_dep_help; die "cmake was not found"; }
    need_cmd g++ || { print_dep_help; die "g++ was not found"; }
    need_cmd kpackagetool6 || { print_dep_help; die "kpackagetool6 was not found"; }

    if ! need_cmd ffmpeg; then
        warn "ffmpeg is not installed. Downloads that merge or convert media will fail until you install it."
    fi

    log "Building"
    if ! cmake -S "$ROOT" -B "$BUILD_DIR" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${HOME}/.local"; then
        print_dep_help
        die "cmake configuration failed"
    fi
    if ! cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || echo 2)"; then
        print_dep_help
        die "build failed"
    fi
    ctest --test-dir "$BUILD_DIR" --output-on-failure

    local stage
    stage="$(mktemp -d)"
    cmake --install "$BUILD_DIR" --prefix "$stage" >/dev/null

    remove_old_updater
    if [[ "$SKIP_RUNTIME" != 1 ]]; then
        install -D -m 755 "${stage}/share/com.zrn59.ytdlp/bin/yt-dlp-update.sh" \
            "${RUNTIME_BIN}/yt-dlp-update.sh"
        if ! "${RUNTIME_BIN}/yt-dlp-update.sh"; then
            warn "yt-dlp was not downloaded. The widget can download it when you open it."
        fi
        install_deno
    fi

    remove_qml_path_hacks
    install_package "$stage"
    rm -rf "$stage"
    enable_timer
    finish_reload

    echo
    echo "Installed."
    echo "  Widget:  ${DATA_HOME}/plasma/plasmoids/${PLASMOID_ID}"
    echo "  yt-dlp:  ${RUNTIME_BIN}/yt-dlp"
    echo "  Updates: systemctl --user list-timers ${UNIT_NAME}.timer"
    echo
    echo "Add it from the panel: Add Widgets → kde-plasmoid-yt-dlp"
    if [[ "$SKIP_RESTART" == 1 ]]; then
        echo "Plasma was not reloaded. Log out once if the widget was already on the panel."
    fi
}

forward_flags() {
    FORWARD=()
    [[ "$SKIP_RESTART" == 1 ]] && FORWARD+=(--skip-restart)
    [[ "$SKIP_RUNTIME" == 1 ]] && FORWARD+=(--skip-runtime)
}

do_update() {
    forward_flags
    if [[ -d "$ROOT/.git" ]]; then
        log "Updating source"
        git -C "$ROOT" pull --ff-only
        exec "$ROOT/install.sh" --install "${FORWARD[@]}"
    fi
    download_and_exec --install "${FORWARD[@]}"
}

do_uninstall() {
    log "Removing ${PLASMOID_ID}"
    if need_cmd kpackagetool6 && kpackagetool6 --type Plasma/Applet --show "$PLASMOID_ID" >/dev/null 2>&1; then
        kpackagetool6 --type Plasma/Applet --remove "$PLASMOID_ID" || true
    fi
    rm -rf "${DATA_HOME}/plasma/plasmoids/${PLASMOID_ID}"
    rm -f "${DATA_HOME}/knotifications6/com.zrn59.ytdlp.notifyrc"
    systemctl --user disable --now "${UNIT_NAME}.timer" >/dev/null 2>&1 || true
    rm -f "${SYSTEMD_USER}/${UNIT_NAME}.service" "${SYSTEMD_USER}/${UNIT_NAME}.timer"
    remove_old_updater
    remove_qml_path_hacks
    rm -rf "$RUNTIME"
    systemctl --user daemon-reload || true
    if [[ "$SKIP_RESTART" != 1 ]] && systemctl --user is-active --quiet plasma-plasmashell.service; then
        systemctl --user restart plasma-plasmashell.service || warn "Reload Plasma yourself"
    fi
    log "Uninstalled. ~/.local/bin/yt-dlp was left in place if you had one."
}

choose_action() {
    local -a items=("Install" "Update" "Repair" "Uninstall" "Quit")
    local -a actions=(install update repair uninstall quit)
    local i=0 key rest
    while true; do
        printf '\033[2J\033[H'
        printf 'kde-plasmoid-yt-dlp\n\n'
        local n
        for n in "${!items[@]}"; do
            if [[ "$n" -eq "$i" ]]; then
                printf '  > %s\n' "${items[$n]}"
            else
                printf '    %s\n' "${items[$n]}"
            fi
        done
        printf '\nUse the arrow keys to move. Press Enter to confirm.\n'
        IFS= read -rsn1 key || die "could not read the menu"
        if [[ "$key" == $'\x1b' ]]; then
            IFS= read -rsn2 rest || true
            case "$rest" in
                '[A') i=$(( (i + ${#items[@]} - 1) % ${#items[@]} )) ;;
                '[B') i=$(( (i + 1) % ${#items[@]} )) ;;
            esac
        elif [[ -z "$key" ]]; then
            ACTION="${actions[$i]}"
            printf '\033[2J\033[H'
            return 0
        fi
    done
}

ROOT="$(script_dir)"
if ! sources_ready; then
    download_and_exec "$@"
fi
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --install) ACTION=install ;;
        --update) ACTION=update ;;
        --repair) ACTION=repair ;;
        --uninstall) ACTION=uninstall ;;
        --skip-restart) SKIP_RESTART=1 ;;
        --skip-runtime) SKIP_RUNTIME=1 ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option: $1 (try --help)" ;;
    esac
    shift
done

trap on_exit EXIT

if [[ -z "$ACTION" ]]; then
    if [[ -t 0 && -t 1 ]]; then
        choose_action
    else
        ACTION=install
    fi
fi

case "$ACTION" in
    install|repair) do_install ;;
    update) do_update ;;
    uninstall) do_uninstall ;;
    quit) exit 0 ;;
    *) die "Unknown action: $ACTION" ;;
esac
