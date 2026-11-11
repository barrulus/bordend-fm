{ runCommand, package }:
runCommand "nordend-installed-smoke" { } ''
  ${package}/bin/nordend --version | grep -F 'nordend ${package.version}'
  ${package}/bin/nordend --config ${../docs/config.example.toml} --check-config
  test -x ${package}/libexec/nordend/nordend-preview
  test -s ${package}/share/nordend/engines/base.toml
  test -s ${package}/share/applications/nordend.desktop
  test -s ${package}/share/icons/hicolor/scalable/apps/nordend.svg
  test -s ${package}/share/nordend/noctalia/nordend
  ${package}/bin/nordend --help | grep -F -- '--pick'
  test -s ${package}/share/xdg-desktop-portal/portals/nordend.portal
  grep -F 'org.freedesktop.impl.portal.FileChooser;' ${package}/share/xdg-desktop-portal/portals/nordend.portal
  grep -F 'Exec=${package}/bin/nordend --portal' ${package}/share/dbus-1/services/org.freedesktop.impl.portal.desktop.nordend.service
  touch "$out"
''
