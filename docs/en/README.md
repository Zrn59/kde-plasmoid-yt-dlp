# yt-dlp Plasma widget

<div align="center">

[English](../../README.md) · [Русский](../../README.ru.md) · **Docs** · [Документация](../README.md)

<br>

[![zrn59.me](https://img.shields.io/badge/⚡_Powered_by-zrn59.me-f59e0b?style=for-the-badge&labelColor=18181b)](https://zrn59.me)
[![Donate](https://img.shields.io/badge/☕_Support_/_Donate-ff5f57?style=for-the-badge&labelColor=18181b)](https://www.donationalerts.com/r/zrn59)

*Custom Bot & Web Development • Data Parsing • Isolated Docker Hosting — **[zrn59.me](https://zrn59.me)***

</div>

---

<p align="center">
  <img src="../screenshot.png" alt="yt-dlp widget" width="420">
</p>

Plasma 6.0 or newer. Plasma 5 is not supported.

## Install

```bash
cd kde-plasmoid-yt-dlp
./install.sh
```

The script builds the widget against the Qt and Plasma on this machine, installs a private yt-dlp under `~/.local/share/com.zrn59.ytdlp/bin/`, and enables a daily update timer (`com.zrn59.ytdlp-update.timer`). The binary is checked with SHA-256. The updater does not call `apt` or `yt-dlp -U`.

Deno is installed only when neither Deno nor Node is already on the system. The C++ plugin stays inside the widget package. `QML_IMPORT_PATH` is not set.

`~/.local/bin/yt-dlp` is left untouched.

`./install.sh` reloads Plasma once. That also removes an older `QML_IMPORT_PATH` override from previous installs of this widget.

```bash
./install.sh --skip-restart
./install.sh --skip-runtime
./uninstall.sh
```

## Dependencies

Runtime: Plasma 6, `ffmpeg`, `curl`. YouTube cookies need Deno or Node.

| Distribution | Packages |
| --- | --- |
| Debian / Ubuntu | `cmake g++ extra-cmake-modules pkg-config qt6-base-dev qt6-declarative-dev libkf6i18n-dev libkf6notifications-dev kpackagetool6 ffmpeg curl` |
| Fedora | `cmake gcc-c++ extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel ffmpeg curl` |
| Arch | `cmake gcc extra-cmake-modules qt6-base qt6-declarative ki18n knotifications kpackage ffmpeg curl` |
| openSUSE | `cmake gcc-c++ extra-cmake-modules qt6-base-devel qt6-declarative-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel ffmpeg curl` |

Preview without a panel:

```bash
plasmoidviewer -a com.zrn59.ytdlp
```

## Features

- Panel icon and active-download count
- Video+audio, video only, audio only
- Quality, codec, container, subtitles, speed limit, time range
- Preview while downloading (`.watch.mkv`)
- Pause and resume
- Retry with resume
- Up to 5 downloads
- KDE notifications
- Update button for the private yt-dlp

`~/.config/yt-dlp/config` is not loaded. Extra flags go in **Advanced**. `-o`, `--exec`, and config-file options are dropped.

## Tree

```
install.sh
uninstall.sh
plugin/
package/
scripts/yt-dlp-update.sh
systemd/
```

## License

MIT. See [LICENSE](../../LICENSE).
