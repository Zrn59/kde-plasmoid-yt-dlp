# Packages: apt and AUR

One package per architecture for each distribution. A newer release of that same distribution can install the package built on the oldest Plasma 6 release in the family. Debian and Arch still need different packages.

Container logs and built packages go to `lab/`, which is not committed.

```bash
./scripts/package-lab.sh debian
./scripts/package-lab.sh arch
./scripts/package-lab.sh all
```

Docker or Podman is required. The script builds the package, installs it into a clean root, and checks that the plugin has no missing libraries.

## On this machine

Debian, Ubuntu and derivatives:

```bash
sudo apt install ./lab/debian/kde-plasmoid-yt-dlp_*_amd64.deb
```

Arch:

```bash
sudo pacman -U lab/arch/kde-plasmoid-yt-dlp-*.pkg.tar.zst
```

Until the package is in a public repository, `apt upgrade` will not find a newer build by itself. Install the new file the same way.

## Publishing for apt

Other people get `apt install kde-plasmoid-yt-dlp` only after the package is in a repository their system already trusts.

The shortest path for Ubuntu is a Launchpad PPA:

1. Create a Launchpad account and upload an OpenPGP key.
2. Create a PPA, for example `kde-plasmoid-yt-dlp`.
3. Build a source package (`dpkg-buildpackage -S`) and upload it with `dput ppa:Zrn59/kde-plasmoid-yt-dlp <file>.changes`.
4. Users add it once:

```bash
sudo add-apt-repository ppa:Zrn59/kde-plasmoid-yt-dlp
sudo apt update
sudo apt install kde-plasmoid-yt-dlp
```

Later updates are a normal `sudo apt upgrade` after a new version is uploaded to the PPA.

Official Debian is a separate request on https://mentors.debian.net. It does not replace a PPA.

Bump `debian/changelog` for every upload, for example `2.0.2-1`.

## Publishing on the AUR

1. Create an account at https://aur.archlinux.org and add an SSH key.
2. Locally:

```bash
git clone ssh://aur@aur.archlinux.org/kde-plasmoid-yt-dlp.git
cp PKGBUILD kde-plasmoid-yt-dlp/
cd kde-plasmoid-yt-dlp
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Add kde-plasmoid-yt-dlp 2.0.1"
git push
```

People install it with:

```bash
yay -S kde-plasmoid-yt-dlp
```

For an update, change `pkgver` and `sha256sums` to the new Git tag, regenerate `.SRCINFO`, commit, and push. Users then run `yay -Syu`.

The checksum in `PKGBUILD` matches tag `v2.0.1`. After a new tag:

```bash
curl -fsSL -o src.tar.gz https://github.com/Zrn59/kde-plasmoid-yt-dlp/archive/refs/tags/v2.0.1.tar.gz
sha256sum src.tar.gz
```
