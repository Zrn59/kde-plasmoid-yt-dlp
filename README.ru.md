# Виджет yt-dlp для Plasma

<div align="center">

[English](README.md) · **Русский** · [Docs](docs/en/README.md) · [Документация](docs/README.md)

<br>

[![zrn59.me](https://img.shields.io/badge/⚡_Powered_by-zrn59.me-f59e0b?style=for-the-badge&labelColor=18181b)](https://zrn59.me)
[![Donate](https://img.shields.io/badge/☕_Support_/_Donate-ff5f57?style=for-the-badge&labelColor=18181b)](https://www.donationalerts.com/r/zrn59)
[![KDE Store](https://img.shields.io/badge/KDE_Store-yt--dlp-1d99f3?style=for-the-badge)](https://store.kde.org/p/2380466/)

*Custom Bot & Web Development • Data Parsing • Isolated Docker Hosting — **[zrn59.me](https://zrn59.me)***

</div>

---

<p align="center">
  <img src="docs/screenshot.png" alt="Виджет yt-dlp" width="420">
</p>

Виджет панели для KDE Plasma 6. Вставляешь ссылку, выбираешь видео или аудио, и он качает своей копией [yt-dlp](https://github.com/yt-dlp/yt-dlp). Копия лежит в `~/.local/share/com.zrn59.ytdlp/` и раз в сутки обновляется сама. Системный yt-dlp и другие виджеты не трогает.

Страница в [KDE Store](https://store.kde.org/p/2380466/).

## Установка

Нужны Plasma 6, компилятор C++, CMake, заголовки Qt 6 и KDE Frameworks 6, а ещё `ffmpeg` и `curl`. Список пакетов для Debian, Fedora, Arch и openSUSE есть в [документации](docs/README.md).

```bash
git clone https://github.com/Zrn59/kde-plasmoid-yt-dlp.git
cd kde-plasmoid-yt-dlp
./install.sh
```

Дальше: ПКМ по панели → **Добавить виджеты** → **yt-dlp**.

`./install.sh` собирает виджет под Qt и Plasma на этом компьютере и один раз перезагружает Plasma. При первом открытии виджет сам скачивает yt-dlp, если его ещё нет, и включает ежедневное обновление. Пароль root не нужен.

```bash
./install.sh --skip-restart
./uninstall.sh
```

Страница в магазине KDE нужна, чтобы проект было легко найти. Внутри виджета маленький C++-плагин, поэтому Discover не может собрать его одним нажатием. Установка — `./install.sh`.

## Что умеет

- Видео и звук, только видео или только аудио
- Качество, кодек, контейнер, субтитры, лимит скорости и отрезок по времени
- Прогресс, пауза, продолжение и повтор
- Просмотр, пока видео ещё качается
- До пяти загрузок сразу
- Уведомления KDE
- Файл сохраняется по названию, например `Bad Apple!!.mp4`

MIT. Текст в [LICENSE](LICENSE).
