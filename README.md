# kde-plasmoid-yt-dlp

<div align="center">

**English** · [Русский](README.ru.md) · [Docs](docs/en/README.md) · [Документация](docs/README.md)

<br>

[![zrn59.me](https://img.shields.io/badge/⚡_Powered_by-zrn59.me-f59e0b?style=for-the-badge&labelColor=18181b)](https://zrn59.me)
[![Donate](https://img.shields.io/badge/☕_Support_/_Donate-ff5f57?style=for-the-badge&labelColor=18181b)](https://www.donationalerts.com/r/zrn59)
[![KDE Store](https://img.shields.io/badge/KDE_Store-yt--dlp-1d99f3?style=for-the-badge)](https://store.kde.org/p/2380466/)

*Custom Bot & Web Development • Data Parsing • Isolated Docker Hosting — **[zrn59.me](https://zrn59.me)***

</div>

---

<p align="center">
  <img src="docs/screenshot-main.png" alt="Widget window" width="420"><br>
  Widget window
</p>

<p align="center">
  <img src="docs/screenshot-advanced.png" alt="Advanced settings" width="420"><br>
  Advanced settings
</p>

<p align="center">
  <img src="docs/screenshot-desktop.png" alt="Widget on the desktop" width="720"><br>
  Widget on the desktop
</p>

KDE Plasma 6 panel widget. Video and audio are downloaded with a private copy of [yt-dlp](https://github.com/yt-dlp/yt-dlp) in `~/.local/share/com.zrn59.ytdlp/`. That copy updates once a day. A system yt-dlp is not replaced.

Store page: [KDE Store](https://store.kde.org/p/2380466/).

## Install

Simplest way:

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
```

KDE Plasma 6 is required. Root is not required. Menu entries: install, update, repair, uninstall. From a clone the same menu is `./install.sh`.

The other methods depend on the system. The distro package and `install.sh` are not installed together.

Panel: right-click → Add Widgets → kde-plasmoid-yt-dlp. A new login is required when the widget was already on the panel.

| System | Architecture | Method |
| --- | --- | --- |
| Ubuntu | amd64, arm64 | PPA |
| Debian 13 | amd64, arm64 | release `.deb` |
| Arch Linux | x86_64, aarch64 | release `.pkg.tar.*` |
| Fedora, openSUSE, other Plasma 6 | amd64, arm64, x86_64, aarch64 | `install.sh` |

The `.deb` is built on Debian 13. Its dependencies do not match Ubuntu. When a Ubuntu release is absent from the PPA, `install.sh` is used.

Architecture is reported by `dpkg --print-architecture` and `uname -m`. The names `amd64` and `x86_64` are the same architecture. The names `arm64` and `aarch64` are the same architecture.

### Ubuntu

```bash
sudo add-apt-repository ppa:zrn59/kde-plasmoid-yt-dlp
sudo apt update
sudo apt install kde-plasmoid-yt-dlp
```

Update command: `sudo apt upgrade`. The PPA contains only the Ubuntu releases enabled on Launchpad.

### Debian 13, amd64

```bash
sudo apt install ./kde-plasmoid-yt-dlp_2.1.0-1_amd64.deb
```

### Debian 13, arm64

```bash
sudo apt install ./kde-plasmoid-yt-dlp_2.1.0-1_arm64.deb
```

Debian 12 and earlier do not install this package. The next version is installed from a new file. `apt upgrade` does not find a release file.

### Arch Linux, x86_64

```bash
sudo pacman -U kde-plasmoid-yt-dlp-2.1.0-1-x86_64.pkg.tar.zst
```

### Arch Linux, aarch64

```bash
sudo pacman -U kde-plasmoid-yt-dlp-2.1.0-1-aarch64.pkg.tar.xz
```

There is no AUR package: registration is temporarily closed 🥲

`pacman -Syu` does not update a package installed from a file. The next version is installed from a new file.

### Build from source

Compiler packages are listed in [docs/en/README.md](docs/en/README.md). Widget directory: `~/.local/share/plasma/plasmoids/`. yt-dlp directory: `~/.local/share/com.zrn59.ytdlp/bin/yt-dlp`. The path `~/.local/bin/yt-dlp` is not changed. Plasma restarts once.

```bash
./install.sh --skip-restart
./uninstall.sh
```

YouTube requires Deno or Node. `install.sh` downloads Deno when neither is present. The apt and pacman packages do not install Deno. `ffmpeg` and `curl` are package dependencies. For `install.sh` those programs are installed first.

## What it does

- Video and audio, video only, or audio only
- Quality, codec, container, subtitles, speed limit, and a time range
- Progress, pause, resume, and retry
- Preview while a video is still downloading
- Up to five downloads at once
- KDE notifications
- Files are saved under the title, for example `Bad Apple!!.mp4`

Packages and the PPA: [docs/en/packaging.md](docs/en/packaging.md).

Suggestions: [Discussions](https://github.com/Zrn59/kde-plasmoid-yt-dlp/discussions).

MIT. See [LICENSE](LICENSE).
