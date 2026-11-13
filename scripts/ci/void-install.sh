#!/usr/bin/env bash
set -euo pipefail
[[ -f /.dockerenv && $(id -u) == 0 ]]
xbps-install -Syu xbps
xbps-install -yu
xbps-install -y -R /packages nordend
xbps-pkgdb nordend
test ! -e /usr/libexec/nordend/nordend-media
# Check linked runtime dependencies before adding compositor/test dependencies.
for executable in /usr/bin/nordend /usr/libexec/nordend/nordend-preview; do
    dependencies=$(ldd "$executable")
    printf '%s\n' "$dependencies"
    if grep -F 'not found' <<< "$dependencies"; then exit 1; fi
done
xbps-install -y desktop-file-utils
bash /source/scripts/ci/check-installed.sh /usr "$1"
xbps-install -y weston mesa-dri dbus dejavu-fonts-ttf
bash /source/scripts/ci/check-wayland.sh /usr

# Install the optional sibling after checking the base package independently.
xbps-install -y -R /packages nordend-media
ldd /usr/libexec/nordend/nordend-media > /tmp/nordend-media-libraries
if grep -F 'not found' /tmp/nordend-media-libraries; then exit 1; fi
