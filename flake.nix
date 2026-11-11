{
  description = "Nordend, a native file manager for Wayland";

  inputs.nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.zst";

  outputs =
    { self, nixpkgs }:
    let
      forEachSystem = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
      ];
    in
    {
      nixosModules = {
        nordend = import ./nix/module.nix self;
        default = self.nixosModules.nordend;
      };

      packages = forEachSystem (system: {
        nordend = nixpkgs.legacyPackages.${system}.callPackage ./nix/package.nix { };
        default = self.packages.${system}.nordend;
      });

      checks = forEachSystem (system: {
        nixos-module = import ./nix/module-check.nix { inherit self nixpkgs system; };
        smoke = nixpkgs.legacyPackages.${system}.callPackage ./nix/smoke-test.nix {
          package = self.packages.${system}.nordend;
        };
      });

      devShells = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.nordend ];
            packages = with pkgs; [
              clang-tools
              doctest
              gdb
              git
              libjpeg_turbo
              nixfmt
            ];
            # Keep CMake's setup hook from configuring a build on shell entry.
            dontUseCmakeConfigure = true;
            shellHook = ''
              export XDG_DATA_DIRS="''${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:${pkgs.shared-mime-info}/share"
            '';
          };
        }
      );

      formatter = forEachSystem (system: nixpkgs.legacyPackages.${system}.nixfmt);
    };
}
