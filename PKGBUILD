# Maintainer: Zrn59 <https://zrn59.me>
pkgname=kde-plasmoid-yt-dlp
pkgver=2.0.1
pkgrel=1
pkgdesc='Plasma 6 widget that downloads video and audio with yt-dlp'
arch=('x86_64' 'aarch64')
url='https://github.com/Zrn59/kde-plasmoid-yt-dlp'
license=('MIT')
depends=(
  'plasma-workspace'
  'qt6-base'
  'qt6-declarative'
  'ki18n'
  'knotifications'
  'ffmpeg'
  'curl'
)
makedepends=('cmake' 'extra-cmake-modules')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('fb0877ba2554a60bef06f9f26a8119104ea8472b5a087011fa46eb45f8c1f373')

build() {
  cmake -S "$pkgname-$pkgver" -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=ON
  cmake --build build
}

check() {
  ctest --test-dir build --output-on-failure
}

package() {
  DESTDIR="$pkgdir" cmake --install build
  install -Dm644 "$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
