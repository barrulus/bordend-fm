{
  self,
  nixpkgs,
  system,
}:
let
  pkgs = nixpkgs.legacyPackages.${system};
  lib = nixpkgs.lib;
  chooser = "org.freedesktop.impl.portal.FileChooser";
  evaluate =
    settings:
    (lib.nixosSystem {
      inherit system;
      modules = [
        self.nixosModules.default
        { system.stateVersion = "26.11"; }
        settings
      ];
    }).config;
  disabled = evaluate { };
  app = evaluate { programs.nordend.enable = true; };
  picker = evaluate {
    programs.nordend = {
      enable = true;
      filePicker.enable = true;
    };
    xdg.portal = {
      extraPortals = [ pkgs.xdg-desktop-portal-gtk ];
      config.umbriel = {
        default = [
          "umbriel"
          "gtk"
        ];
        "org.freedesktop.impl.portal.ScreenCast" = [ "umbriel" ];
      };
    };
  };
  replacement = pkgs.runCommand "nordend-module-test-package" { } "mkdir -p $out";
  custom = evaluate {
    programs.nordend = {
      enable = true;
      package = replacement;
      filePicker = {
        enable = true;
        desktops = [
          "niri"
          "common"
        ];
      };
    };
  };
  invalid = evaluate { programs.nordend.filePicker.enable = true; };
  emptyDesktops = evaluate {
    programs.nordend = {
      enable = true;
      filePicker.enable = true;
      filePicker.desktops = [ ];
    };
  };
  rejected =
    config: lib.any (a: !a.assertion && lib.hasPrefix "programs.nordend." a.message) config.assertions;
in
assert !disabled.xdg.portal.enable;
assert !(lib.elem disabled.programs.nordend.package disabled.environment.systemPackages);
assert lib.elem app.programs.nordend.package app.environment.systemPackages;
assert !app.xdg.portal.enable;
assert app.xdg.portal.config == { };
assert picker.xdg.portal.enable;
assert lib.elem picker.programs.nordend.package picker.xdg.portal.extraPortals;
assert lib.elem pkgs.xdg-desktop-portal-gtk picker.xdg.portal.extraPortals;
assert lib.elem picker.programs.nordend.package picker.services.dbus.packages;
assert picker.xdg.portal.config.umbriel.${chooser} == "nordend";
assert picker.xdg.portal.config.umbriel.default == "umbriel;gtk";
assert picker.xdg.portal.config.umbriel."org.freedesktop.impl.portal.ScreenCast" == "umbriel";
assert lib.hasInfix "${chooser}=nordend"
  picker.environment.etc."xdg/xdg-desktop-portal/umbriel-portals.conf".text;
assert lib.elem replacement custom.environment.systemPackages;
assert lib.elem replacement custom.xdg.portal.extraPortals;
assert lib.elem replacement custom.services.dbus.packages;
assert custom.xdg.portal.config.niri.${chooser} == "nordend";
assert custom.xdg.portal.config.common.${chooser} == "nordend";
assert !(custom.xdg.portal.config ? umbriel);
assert !rejected picker;
assert rejected invalid;
assert rejected emptyDesktops;
pkgs.runCommand "nordend-module-check" { } ''
  touch "$out"
''
