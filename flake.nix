{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in {
    packages.${system} = pkgs // rec {
      imp-sdl3 = pkgs.stdenv.mkDerivation ({
        pname = "imp-sdl3";
        version = "0.0.0";
        src = ./.;
        buildInputs = [ pkgs.sdl3 ];
        allowSubstitutes = false;
        dontStrip = true;

        # if we have to debug hairy macros, change
        # to use gcc -E to preprocess source and then compile that
        buildPhase = ''
          mkdir -p $out/bin
          ${pkgs.stdenv.cc}/bin/cc -std=c99 main.c -o $out/bin/imp-sdl3 -lSDL3 -lm -g -Og -Wall -Wextra -Wpedantic
        '';
      });

      default = imp-sdl3;
    };
  };
}

