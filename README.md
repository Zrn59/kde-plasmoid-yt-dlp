# yt-dlp Plasma widget

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
  <img src="docs/screenshot.png" alt="yt-dlp widget" width="420">
</p>

Panel widget for KDE Plasma 6. Paste a link, pick video or audio, and it downloads with its own copy of [yt-dlp](https://github.com/yt-dlp/yt-dlp). That copy lives in `~/.local/share/com.zrn59.ytdlp/` and updates itself once a day. System yt-dlp and other widgets are left alone.

Also on the [KDE Store](https://store.kde.org/p/2380466/).

## Install

Plasma 6, a C++ compiler, CMake, Qt 6 and KDE Frameworks 6 development packages, plus `ffmpeg` and `curl`. The package list for Debian, Fedora, Arch and openSUSE is in the [docs](docs/en/README.md).

```bash
git clone https://github.com/Zrn59/kde-plasmoid-yt-dlp.git
cd kde-plasmoid-yt-dlp
./install.sh
```

Then right-click the panel → **Add Widgets** → **yt-dlp**.

`./install.sh` builds the widget for the Qt and Plasma installed on this machine and reloads Plasma once. The first time the widget opens, it downloads yt-dlp if it is not there yet and enables the daily update timer. No root password.

```bash
./install.sh --skip-restart
./uninstall.sh
```

The KDE Store page is for finding the project. The widget includes a small C++ plugin, so a one-click install from Discover cannot compile it for you. Use `./install.sh`.

## What it does

- Video and audio, video only, or audio only
- Quality, codec, container, subtitles, speed limit, and a time range
- Progress, pause, resume, and retry
- Preview while a video is still downloading
- Up to five downloads at once
- KDE notifications
- Files are saved under the title, for example `Bad Apple!!.mp4`

MIT. See [LICENSE](LICENSE).
