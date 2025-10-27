{
  description = "Template for a plotting program using SDL3 on NixOS";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";  # Adjust if needed for other architectures
    pkgs = nixpkgs.legacyPackages.${system};
    deps = {
      buildInputs = with pkgs; [ sdl3 ];
    };
  in {
    packages.${system}.default = pkgs.stdenv.mkDerivation (deps // {
      pname = "imp-sdl3";
      version = "0.0.0";
      src = ./.;
      buildPhase = ''
        mkdir -p $out/bin
        ${pkgs.stdenv.cc}/bin/cc main.c -o $out/bin/imp-sdl3 -lSDL3 -lm
      '';
    });
  };
}

### Usage Instructions:
# 1. Save the above as `flake.nix`.
# 2. Create a file named `main.c` with the content below.
# 3. Run `nix develop` to enter the development shell.
# 4. Compile with: `gcc main.c -o sinewave $(pkg-config --cflags --libs SDL3)`
# 5. Run with: `./sinewave`
# 6. Alternatively, build the package with `nix build` and run `./result/bin/sinewave`.

