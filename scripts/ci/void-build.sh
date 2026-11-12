#!/usr/bin/env bash
# Container entry point. Never run xbps-src's ethereal mode on the host.
set -euo pipefail
[[ -f /.dockerenv && $(id -u) == 0 ]]
# shellcheck source=scripts/ci/void-env
source /source/scripts/ci/void-env
xbps-install -Syu xbps
xbps-install -yu
xbps-install -y python3
export PATH="/usr/libexec/chroot-git:$PATH"
mkdir /buildroot
cd /buildroot
git init
git remote add origin https://github.com/void-linux/void-packages.git
git fetch --depth 1 origin "$VOID_PACKAGES_REV"
git checkout --detach FETCH_HEAD
ln -s / masterdir
xbps-uhelper arch > /.xbps_chroot_init
export XBPS_CHROOT_CMD=ethereal XBPS_ALLOW_CHROOT_BREAKOUT=yes
export XBPS_MAKEJOBS=${CI_JOBS:-2}
mkdir -p srcpkgs/nordend
cp /source/packaging/void/nordend/template srcpkgs/nordend/template
# Only source metadata changes: use the checked-out tree, never upstream's old release.
python3 /source/scripts/ci/void-source.py /input /buildroot
./xbps-src pkg nordend
mkdir -p /output/packages
cp hostdir/binpkgs/nordend-*.xbps hostdir/binpkgs/*-repodata /output/packages/
cp srcpkgs/nordend/template /output/template
cp /input/source.json /output/source.json
