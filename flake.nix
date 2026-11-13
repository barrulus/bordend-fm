{
  description = "Nordend, a native file manager for Wayland: fast, light and safe";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux"; # the only one built and tried so far
      pkgs = nixpkgs.legacyPackages.${system};
      # The version CMake builds, so the two never disagree.
      version = builtins.head (
        builtins.match ".*project\\(nordend VERSION ([0-9.]+) .*" (
          builtins.replaceStrings [ "\n" ] [ " " ] (builtins.readFile ./CMakeLists.txt)
        )
      );
      build = pkgs: pkgs.callPackage ./packaging/nix/package.nix {
        inherit version;
        src = self;
      };
    in
    {
      packages.${system} = rec {
        nordend = build pkgs;
        default = nordend;
      };
      # For a NixOS configuration: nixpkgs.overlays = [ nordend.overlays.default ];
      overlays.default = final: _prev: { nordend = build final; };
      checks.${system}.nordend = self.packages.${system}.nordend;
    };
}
