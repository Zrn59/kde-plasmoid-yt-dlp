# Пакеты: apt и AUR

Один пакет каждой архитектуры на дистрибутив. Более новый выпуск того же дистрибутива может ставить пакет, собранный на самом старом Plasma 6 этого семейства. Debian и Arch это разные пакеты.

Проверка в контейнерах, логи и готовые файлы лежат в `lab/`. Эта папка не попадает в git.

```bash
./scripts/package-lab.sh debian
./scripts/package-lab.sh arch
./scripts/package-lab.sh all
```

Нужны Docker или Podman. Скрипт собирает пакет, ставит его в чистый корень и проверяет, что плагин загружается без недостающих библиотек.

## Свой компьютер

Debian, Ubuntu и производные:

```bash
sudo apt install ./lab/debian/kde-plasmoid-yt-dlp_*_amd64.deb
```

Arch:

```bash
sudo pacman -U lab/arch/kde-plasmoid-yt-dlp-*.pkg.tar.zst
```

Обновление такого пакета — поставить новый файл тем же способом. Пока пакет не в официальном репозитории, `apt upgrade` сам его не найдёт.

## Как попасть в apt

`apt install kde-plasmoid-yt-dlp` заработает у чужих людей только когда пакет лежит в репозитории, который прописан у них в системе.

Ближайший путь для Ubuntu — свой PPA на Launchpad:

1. Аккаунт на https://launchpad.net и загрузка OpenPGP-ключа.
2. Создать PPA, например `kde-plasmoid-yt-dlp`.
3. Собрать исходный пакет (`dpkg-buildpackage -S`) и отправить его: `dput ppa:Zrn59/kde-plasmoid-yt-dlp файл_source.changes`.
4. Люди добавляют репозиторий один раз:

```bash
sudo add-apt-repository ppa:Zrn59/kde-plasmoid-yt-dlp
sudo apt update
sudo apt install kde-plasmoid-yt-dlp
```

Дальше обновление для них обычное: `sudo apt upgrade`, когда в PPA залита новая версия.

В официальный Debian пакет попадает отдельно: заявка на https://mentors.debian.net и сопровождающий, который его загрузит. Это не замена PPA и занимает больше времени.

В `debian/changelog` при каждом выпуске увеличивается версия, например `2.0.2-1`.

## Как попасть в AUR

1. Аккаунт на https://aur.archlinux.org и SSH-ключ в настройках.
2. Локально:

```bash
git clone ssh://aur@aur.archlinux.org/kde-plasmoid-yt-dlp.git
cp PKGBUILD .SRCINFO kde-plasmoid-yt-dlp/
cd kde-plasmoid-yt-dlp
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Add kde-plasmoid-yt-dlp 2.0.1"
git push
```

Люди ставят так:

```bash
yay -S kde-plasmoid-yt-dlp
```

Обновление: в `PKGBUILD` меняются `pkgver` и `sha256sums` на новый тег GitHub, снова `makepkg --printsrcinfo > .SRCINFO`, коммит и `git push`. У пользователя это `yay -Syu`.

Сумма в текущем `PKGBUILD` посчитана для тега `v2.0.1`. После нового тега её нужно заменить:

```bash
curl -fsSL -o src.tar.gz https://github.com/Zrn59/kde-plasmoid-yt-dlp/archive/refs/tags/v2.0.1.tar.gz
sha256sum src.tar.gz
```
