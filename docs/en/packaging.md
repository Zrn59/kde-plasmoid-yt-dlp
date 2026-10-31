# Package build

Widget installation is documented in [README.md](../../README.md). This page covers release-file builds and the PPA upload.

Docker or Podman is required. Output is written to `lab/`. The `lab/` directory is not committed.

```bash
./scripts/package-lab.sh debian
./scripts/package-lab.sh debian-arm
./scripts/package-lab.sh arch
./scripts/package-lab.sh arch-arm
./scripts/package-lab.sh all
```

| File | Build |
| --- | --- |
| `lab/debian/kde-plasmoid-yt-dlp_*_amd64.deb` | Debian 13, amd64 |
| `lab/debian/kde-plasmoid-yt-dlp_*_arm64.deb` | Debian 13, arm64 |
| `lab/arch/kde-plasmoid-yt-dlp-*-x86_64.pkg.tar.zst` | Arch Linux, x86_64 |
| `lab/arch/kde-plasmoid-yt-dlp-*-aarch64.pkg.tar.xz` | Arch Linux, aarch64 |

Downloaded library packages are stored in `lab/debian/apt-cache-*` and `lab/arch/pacman-cache-*`. The container root is empty on the next run, so cached packages are installed again. arm64 and aarch64 builds run under an emulator.

## PPA

apt address: `ppa:zrn59/kde-plasmoid-yt-dlp`.

In `~/.dput.cf` the section name is `my-ppa` and the path is `~zrn59/ubuntu/kde-plasmoid-yt-dlp/`. The upload is a source package. A finished `.deb` is not uploaded. Launchpad builds the package for the Ubuntu releases and architectures enabled in the PPA. An arm64 build runs when the arm64 architecture is enabled.

```bash
dput my-ppa kde-plasmoid-yt-dlp_2.1.0-1_source.changes
```

Until that command is run, apt has no `kde-plasmoid-yt-dlp` package.

## Arch Linux

The AUR recipe is not published: registration is temporarily closed 🥲 The release contains the finished `.pkg.tar.*`.

`PKGBUILD` sets `sha256sums=('SKIP')` until the GitHub tag exists. Before the recipe is published, the value is replaced with the checksum of the `v2.1.0` tag archive.
