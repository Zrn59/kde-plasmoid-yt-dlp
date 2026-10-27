# kde-plasmoid-yt-dlp

<div align="center">

[English](../README.md) · [Русский](../README.ru.md) · [Docs](en/README.md) · **Документация**

<br>

[![zrn59.me](https://img.shields.io/badge/⚡_Powered_by-zrn59.me-f59e0b?style=for-the-badge&labelColor=18181b)](https://zrn59.me)
[![Donate](https://img.shields.io/badge/☕_Support_/_Donate-ff5f57?style=for-the-badge&labelColor=18181b)](https://www.donationalerts.com/r/zrn59)

*Custom Bot & Web Development • Data Parsing • Isolated Docker Hosting — **[zrn59.me](https://zrn59.me)***

</div>

---

<p align="center">
  <img src="screenshot.png" alt="Виджет yt-dlp" width="420">
</p>

Нужна Plasma 6.0 или новее. Plasma 5 не поддерживается.

Страница в магазине: https://store.kde.org/p/2380466/

Discover может показать проект, но C++-плагин всё равно собирается через `./install.sh`. После этого виджет при первом открытии сам скачивает yt-dlp и включает ежедневное обновление. Пароль root не нужен.

## Установка

```bash
cd kde-plasmoid-yt-dlp
./install.sh
```

Скрипт собирает виджет под локальные Qt и Plasma, ставит отдельный yt-dlp в `~/.local/share/com.zrn59.ytdlp/bin/` и включает ежедневный таймер `com.zrn59.ytdlp-update.timer`. Файл сверяется по SHA-256. Обновление не вызывает `apt` и `yt-dlp -U`.

Deno скачивается только если в системе нет ни Deno, ни Node. C++-плагин лежит внутри пакета виджета. `QML_IMPORT_PATH` не задаётся.

`~/.local/bin/yt-dlp` не перезаписывается.

`./install.sh` один раз перезагружает Plasma и снимает старую подмену `QML_IMPORT_PATH`, если она осталась от прошлой установки этого виджета.

```bash
./install.sh --skip-restart
./install.sh --skip-runtime
./uninstall.sh
```

## Зависимости

Для работы: Plasma 6, `ffmpeg`, `curl`. Для cookies YouTube нужен Deno или Node.

| Дистрибутив | Пакеты |
| --- | --- |
| Debian / Ubuntu | `cmake g++ extra-cmake-modules pkg-config qt6-base-dev qt6-declarative-dev libkf6i18n-dev libkf6notifications-dev kpackagetool6 ffmpeg curl` |
| Fedora | `cmake gcc-c++ extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel ffmpeg curl` |
| Arch | `cmake gcc extra-cmake-modules qt6-base qt6-declarative ki18n knotifications kpackage ffmpeg curl` |
| openSUSE | `cmake gcc-c++ extra-cmake-modules qt6-base-devel qt6-declarative-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kpackage-devel ffmpeg curl` |

Проверка без панели:

```bash
plasmoidviewer -a com.zrn59.ytdlp
```

## Возможности

- Иконка на панели и число активных загрузок
- Видео+звук, только видео, только аудио
- Качество, кодек, контейнер, субтитры, лимит скорости, отрезок по времени
- Просмотр во время загрузки (`.watch.mkv`)
- Пауза и продолжение
- Повтор с докачкой
- До 5 загрузок сразу
- Уведомления KDE
- Кнопка обновления своего yt-dlp

Файл `~/.config/yt-dlp/config` не читается. Свои флаги задаются в **Advanced**. `-o`, `--exec` и подключение чужого конфига отбрасываются.

## Состав

```
install.sh
uninstall.sh
plugin/
package/
scripts/yt-dlp-update.sh
systemd/
```

## Лицензия

MIT. Текст в [LICENSE](../LICENSE).
