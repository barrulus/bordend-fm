#!/usr/bin/env bash
set -euo pipefail
prefix=${1:?usage: check-installed.sh PREFIX VERSION}
version=${2:?usage: check-installed.sh PREFIX VERSION}
test -s "${3:-$prefix/share/licenses/nordend/LICENSE}"
app="$prefix/bin/nordend"
"$app" --version | grep -Fx "nordend $version"
"$app" --help | grep -F -- '--pick'
"$app" --config "$prefix/share/nordend/config.example.toml" --check-config
for file in \
    share/nordend/config.example.toml \
    share/nordend/engines/base.toml \
    share/applications/nordend.desktop \
    share/icons/hicolor/scalable/apps/nordend.svg \
    share/nordend/noctalia/nordend \
    share/xdg-desktop-portal/portals/nordend.portal \
    share/dbus-1/services/org.freedesktop.impl.portal.desktop.nordend.service; do
    test -s "$prefix/$file"
done
test -x "$prefix/libexec/nordend/nordend-preview"
desktop-file-validate "$prefix/share/applications/nordend.desktop"
grep -Fx 'Interfaces=org.freedesktop.impl.portal.FileChooser;' \
    "$prefix/share/xdg-desktop-portal/portals/nordend.portal"
grep -Fx "Exec=$app --portal" \
    "$prefix/share/dbus-1/services/org.freedesktop.impl.portal.desktop.nordend.service"
# The installed binary must understand the features documented in the installed reference.
config=$(mktemp)
trap 'rm -f "$config"' EXIT
cat > "$config" <<'TOML'
[[actions]]
id = "compress"
label = "Compress"
argv = ["7z", "a", "{destination}", "--", "{files}"]
selection = "many"
destination = "file"
destination_name = "Archive.zip"
notify_on_completion = true
[context_menu]
entry = [{ id = "action:compress" }, { remaining = true }]
TOML
"$app" --config "$config" --check-config
