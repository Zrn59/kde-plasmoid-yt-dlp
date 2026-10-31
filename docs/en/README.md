# Install

[Русский](../README.md)

Simplest way:

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
```

KDE Plasma 6 is required. Root is not required. The other methods and the compiler packages are below. The short system list is in [README.md](../../README.md).

## File and system

| File | System | Architecture |
| --- | --- | --- |
| PPA `ppa:zrn59/kde-plasmoid-yt-dlp` | Ubuntu releases enabled in the PPA | amd64, arm64 |
| `kde-plasmoid-yt-dlp_2.1.0-1_amd64.deb` | Debian 13 | amd64 |
| `kde-plasmoid-yt-dlp_2.1.0-1_arm64.deb` | Debian 13 | arm64 |
| `kde-plasmoid-yt-dlp-2.1.0-1-x86_64.pkg.tar.zst` | Arch Linux | x86_64 |
| `kde-plasmoid-yt-dlp-2.1.0-1-aarch64.pkg.tar.xz` | Arch Linux | aarch64 |

The `.deb` is built on Debian 13. Its dependencies do not match Ubuntu. The Arch Linux package is built against the Arch Linux repositories at build time. Older library versions make `pacman -U` fail on dependencies.

Architecture:

```bash
dpkg --print-architecture
uname -m
```

`amd64` and `x86_64` are one architecture. `arm64` and `aarch64` are one architecture.

## After install

Panel: right-click → Add Widgets → kde-plasmoid-yt-dlp.

A new login is required when the widget was already on the panel. Otherwise the previously loaded plugin stays in memory.

An apt or pacman package and `./install.sh` are not used together.

## Build with install.sh

Root is not required. Compiler packages are installed first.

Debian, Ubuntu:

```bash
sudo apt install cmake g++ extra-cmake-modules pkg-config \
  qt6-base-dev qt6-declarative-dev \
  libkf6i18n-dev libkf6notifications-dev kpackagetool6 \
  ffmpeg curl git
```

Fedora:

```bash
sudo dnf install cmake gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel \
  ffmpeg curl git
```

Arch Linux:

```bash
sudo pacman -S --needed cmake gcc extra-cmake-modules \
  qt6-base qt6-declarative ki18n knotifications kpackage \
  ffmpeg curl git
```

openSUSE:

```bash
sudo zypper install cmake gcc-c++ extra-cmake-modules \
  qt6-base-devel qt6-declarative-devel \
  kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel \
  ffmpeg curl git
```

Run:

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
```

From a clone: `./install.sh`.

| Command | Action |
| --- | --- |
| `./install.sh` | menu |
| `./install.sh --install` | build and install |
| `./install.sh --update` | download the source and install |
| `./install.sh --repair` | build again without downloading the source |
| `./install.sh --uninstall` | remove the widget and its yt-dlp |
| `./install.sh --skip-restart` | do not restart Plasma |
| `./install.sh --skip-runtime` | do not download yt-dlp or Deno |
| `./uninstall.sh` | same as `--uninstall` |

Paths:

- widget: `~/.local/share/plasma/plasmoids/com.zrn59.ytdlp`
- yt-dlp: `~/.local/share/com.zrn59.ytdlp/bin/yt-dlp`
- timer: `systemctl --user list-timers com.zrn59.ytdlp-update.timer`

The path `~/.local/bin/yt-dlp` is not changed. `~/.config/yt-dlp/config` is not read.

## Runtime dependencies

`ffmpeg` merges video and audio. `curl` downloads the private yt-dlp. The apt and pacman packages install both. For `install.sh` they are installed first.

YouTube requires Deno or Node. `install.sh` installs Deno only when neither program is present. The apt and pacman packages do not install Deno:

```bash
sudo apt install deno
```

Cookies are read from Firefox, Chrome, Chromium, Brave, Edge, Vivaldi, or from a cookies file.

Run without a panel:

```bash
plasmoidviewer -a com.zrn59.ytdlp
```
