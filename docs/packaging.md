# Сборка пакетов

Установка виджета описана в [README.ru.md](../README.ru.md). Здесь сборка файлов релиза и отправка в PPA.

Требуются Docker или Podman. Файлы записываются в `lab/`. Каталог `lab/` в git не входит.

```bash
./scripts/package-lab.sh debian
./scripts/package-lab.sh debian-arm
./scripts/package-lab.sh arch
./scripts/package-lab.sh arch-arm
./scripts/package-lab.sh all
```

| Файл | Сборка |
| --- | --- |
| `lab/debian/kde-plasmoid-yt-dlp_*_amd64.deb` | Debian 13, amd64 |
| `lab/debian/kde-plasmoid-yt-dlp_*_arm64.deb` | Debian 13, arm64 |
| `lab/arch/kde-plasmoid-yt-dlp-*-x86_64.pkg.tar.zst` | Arch Linux, x86_64 |
| `lab/arch/kde-plasmoid-yt-dlp-*-aarch64.pkg.tar.xz` | Arch Linux, aarch64 |

Загруженные пакеты библиотек хранятся в `lab/debian/apt-cache-*` и `lab/arch/pacman-cache-*`. Корневая файловая система контейнера при следующем запуске пустая, поэтому установка из кэша выполняется снова. Сборка arm64 и aarch64 выполняется через эмулятор.

## PPA

Адрес для apt: `ppa:zrn59/kde-plasmoid-yt-dlp`.

В `~/.dput.cf` имя секции — `my-ppa`, каталог — `~zrn59/ubuntu/kde-plasmoid-yt-dlp/`. Загружается исходный пакет. Готовый `.deb` в PPA не загружается. Launchpad собирает пакет для выпусков Ubuntu и архитектур, включённых в настройках PPA. Сборка arm64 выполняется при включённой архитектуре arm64.

```bash
dput my-ppa kde-plasmoid-yt-dlp_2.1.0-1_source.changes
```

До выполнения команды пакет `kde-plasmoid-yt-dlp` в apt отсутствует.

## Arch Linux

Рецепт в AUR не опубликован: регистрация временно закрыта 🥲 В релиз входит готовый `.pkg.tar.*`.

В `PKGBUILD` указано `sha256sums=('SKIP')` до появления тега GitHub. Перед публикацией рецепта значение заменяется контрольной суммой архива тега `v2.1.0`.
