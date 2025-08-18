# Maintainer: Minh Thuan Tran <thuanc177@gmail.com>
_pkgname=cs2-ec
pkgname=${_pkgname}-git
pkgver=r0.g0000000
pkgrel=1
pkgdesc="EC (cs2-ec) utility"
arch=('x86_64')
url="https://github.com/AtelierMizumi/EC"
license=('unknown')
depends=('sdl3')
makedepends=('git' 'gcc' 'pkgconf')
provides=(${_pkgname})
conflicts=(${_pkgname})
source=("${_pkgname}::git+https://github.com/AtelierMizumi/EC.git")
sha256sums=('SKIP')

pkgver() {
  cd "${srcdir}/${_pkgname}"
  # use commit count and short hash
  printf "r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

prepare() {
  cd "${srcdir}/${_pkgname}"
  git submodule update --init --recursive
}

build() {
  cd "${srcdir}/${_pkgname}"

  CFLAGS="$CFLAGS -O3 -DNDEBUG -Wall -Wno-format-truncation -Wno-strict-aliasing"
  SDL_CFLAGS="$(pkg-config --cflags sdl3)"
  SDL_LIBS="$(pkg-config --libs sdl3)"

  g++ -std=c++17 ${CFLAGS} ${SDL_CFLAGS} -o cs2-ec \
    projects/LINUX/main.cpp \
    cs2/shared/cs2.cpp cs2/shared/cs2features.cpp \
    apex/shared/apex.cpp apex/shared/apexfeatures.cpp \
    library/vm/linux/vm.cpp \
    -pthread ${SDL_LIBS}
}

package() {
  install -Dm755 "${srcdir}/${_pkgname}/cs2-ec" "${pkgdir}/usr/bin/cs2-ec"
}
