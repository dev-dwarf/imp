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
      allowSubstitutes = false;
      dontStrip = true;
      buildPhase = ''
        mkdir -p $out/bin
        ${pkgs.stdenv.cc}/bin/cc main.c -o $out/bin/imp-sdl3 -lSDL3 -lm -g -Og
      '';
    });
  };
}
