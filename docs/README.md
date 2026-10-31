# Установка

[English](en/README.md)

Самый простой способ:

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
```

Требуется KDE Plasma 6. Права root не нужны. Остальные способы и пакеты компилятора перечислены ниже. Краткий выбор системы: [README.ru.md](../README.ru.md).

## Соответствие файла и системы

| Файл | Система | Архитектура |
| --- | --- | --- |
| PPA `ppa:zrn59/kde-plasmoid-yt-dlp` | Ubuntu, выпуски, включённые в PPA | amd64, arm64 |
| `kde-plasmoid-yt-dlp_2.1.0-1_amd64.deb` | Debian 13 | amd64 |
| `kde-plasmoid-yt-dlp_2.1.0-1_arm64.deb` | Debian 13 | arm64 |
| `kde-plasmoid-yt-dlp-2.1.0-1-x86_64.pkg.tar.zst` | Arch Linux | x86_64 |
| `kde-plasmoid-yt-dlp-2.1.0-1-aarch64.pkg.tar.xz` | Arch Linux | aarch64 |

Файл `.deb` собран в Debian 13. Зависимости этого файла на Ubuntu не совпадают. Пакет Arch Linux собран для репозиториев Arch Linux на момент сборки. При более старых версиях библиотек `pacman -U` завершается ошибкой зависимостей.

Определение архитектуры:

```bash
dpkg --print-architecture
uname -m
```

`amd64` и `x86_64` — одна архитектура. `arm64` и `aarch64` — одна архитектура.

## После установки

Добавление на панель: ПКМ по панели → Добавить виджеты → kde-plasmoid-yt-dlp.

При уже добавленном виджете требуется новый вход в сессию. Иначе остаётся ранее загруженный плагин.

Пакет apt или pacman и `./install.sh` одновременно не используются.

## Сборка через install.sh

Права root не требуются. Перед запуском устанавливаются пакеты компилятора.

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

Запуск:

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Zrn59/kde-plasmoid-yt-dlp/main/install.sh)"
```

Из клона: `./install.sh`.

| Команда | Действие |
| --- | --- |
| `./install.sh` | меню |
| `./install.sh --install` | сборка и установка |
| `./install.sh --update` | загрузка исходников и установка |
| `./install.sh --repair` | повторная сборка без загрузки исходников |
| `./install.sh --uninstall` | удаление виджета и его yt-dlp |
| `./install.sh --skip-restart` | без перезапуска Plasma |
| `./install.sh --skip-runtime` | без загрузки yt-dlp и Deno |
| `./uninstall.sh` | то же, что `--uninstall` |

Пути:

- виджет: `~/.local/share/plasma/plasmoids/com.zrn59.ytdlp`
- yt-dlp: `~/.local/share/com.zrn59.ytdlp/bin/yt-dlp`
- таймер: `systemctl --user list-timers com.zrn59.ytdlp-update.timer`

Путь `~/.local/bin/yt-dlp` не изменяется. Файл `~/.config/yt-dlp/config` не читается.

## Зависимости при работе

`ffmpeg` используется для склейки видео и звука. `curl` используется для загрузки собственного yt-dlp. Пакет apt или pacman устанавливает обе программы. Для `install.sh` они устанавливаются заранее.

Для YouTube требуется Deno или Node. `install.sh` устанавливает Deno только при отсутствии обеих программ. Пакет apt или pacman Deno не устанавливает:

```bash
sudo apt install deno
```

Допустимы cookies из Firefox, Chrome, Chromium, Brave, Edge, Vivaldi и из файла cookies.

Запуск без панели:

```bash
plasmoidviewer -a com.zrn59.ytdlp
```
