{ runCommand, package }:
runCommand "nordend-installed-smoke" { } ''
  ${package}/bin/nordend --version | grep -F 'nordend ${package.version}'
  ${package}/bin/nordend --config ${../docs/config.example.toml} --check-config
  test -x ${package}/libexec/nordend/nordend-preview
  test -s ${package}/share/nordend/engines/base.toml
  test -s ${package}/share/applications/nordend.desktop
  test -s ${package}/share/icons/hicolor/scalable/apps/nordend.svg
  test -s ${package}/share/nordend/noctalia/nordend
  touch "$out"
''
