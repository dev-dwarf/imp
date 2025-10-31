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
        buildPhase = ''
          mkdir -p $out/bin
          ${pkgs.stdenv.cc}/bin/cc main.c -o $out/bin/imp-sdl3 -lSDL3 -lm
        '';
      });

      default = imp-sdl3;
    };
  };
}

