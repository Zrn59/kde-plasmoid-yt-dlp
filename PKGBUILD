# Maintainer: Zrn59 <https://zrn59.me>
pkgname=kde-plasmoid-yt-dlp
pkgver=2.1.0
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
# Local package builds replace this. After the v2.1.0 tag exists, put the
# checksum of the GitHub tag archive here before anyone builds from that tag.
sha256sums=('SKIP')

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
