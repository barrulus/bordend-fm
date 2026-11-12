#!/usr/bin/env bash
set -euo pipefail
[[ -f /.dockerenv && $EUID == 0 ]] || { echo 'Run native.sh, not this script on the host.' >&2; exit 1; }
format=${1:?}
case "$format" in
    deb)
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends build-essential debhelper devscripts equivs python3 ca-certificates
        ;;
    rpm) dnf install -y rpm-build rpmdevtools python3 tar gzip ;;
    *) exit 1 ;;
esac
mkdir -p /build /output/packages
# Validate the archive before unpacking, and derive package versions from its identity.
python3 - <<'PY'
import hashlib, json, re
from pathlib import Path
m = json.loads(Path('/input/source.json').read_text())
assert re.fullmatch(r'[0-9a-f]{40}', m['commit'])
assert re.fullmatch(r'[0-9]{14}', m['timestamp'])
assert re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', m['version'])
assert hashlib.sha256(Path('/input/source.tar.gz').read_bytes()).hexdigest() == m['sha256']
Path('/build/version').write_text(m['version'])
Path('/build/snapshot').write_text(m['timestamp']+'.git'+m['commit'][:12])
PY
version=$(cat /build/version)
snapshot=$(cat /build/snapshot)
case "$format" in
    deb)
        tar -xzf /input/source.tar.gz -C /build
        cd /build/nordend-checkout
        cp -a packaging/debian debian
        DEBFULLNAME='Nordend fork maintainers' DEBEMAIL='barrulus@users.noreply.github.com' \
            dch --newversion "$version+git$snapshot-1" --distribution unstable 'Build exact CI source snapshot.'
        mk-build-deps --install --remove --tool 'apt-get -y --no-install-recommends' debian/control
        cp -a debian /output/recipe
        DEB_BUILD_OPTIONS="parallel=${CI_JOBS:-2}" dpkg-buildpackage -b -us -uc
        cp /build/*.deb /build/*.changes /build/*.buildinfo /output/packages/
        ;;
    rpm)
        mkdir -p /build/rpm/{SOURCES,SPECS}
        cp /input/source.tar.gz /build/rpm/SOURCES/nordend-source.tar.gz
        tar -xzf /input/source.tar.gz -C /build
        cp /build/nordend-checkout/packaging/rpm/nordend.spec /build/rpm/SPECS/
        spec=/build/rpm/SPECS/nordend.spec
        sed -i -e "s/^Version:.*/Version:        $version/" \
            -e "s/^Release:.*/Release:        1.$snapshot%{?dist}/" "$spec"
        # Use the same recipe for dependency resolution and building.
        rpmspec -q --buildrequires "$spec" > /build/dependencies
        mapfile -t dependencies < /build/dependencies
        dnf install -y "${dependencies[@]}"
        rpmbuild -ba --define '_topdir /build/rpm' \
            --define "_smp_mflags -j${CI_JOBS:-2}" "$spec"
        find /build/rpm/RPMS /build/rpm/SRPMS -name '*.rpm' -exec cp '{}' /output/packages/ \;
        cp "$spec" /output/nordend.spec
        ;;
esac
cp /input/source.json /output/source.json
