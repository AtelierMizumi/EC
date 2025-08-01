# Maintainer: Minh Thuan Tran <thuanc177@gmail.com>
{
  description = "Nix flake for open-source External Cheat for CS2 (cs2-ec)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" ];
      forEachSystem = f: nixpkgs.lib.genAttrs systems (system: f system);
    in
    {
      packages = forEachSystem (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          vmSrc = pkgs.fetchFromGitHub {
            owner = "ekknod";
            repo = "vm";
            rev = "fcb3d2d2a3511b9c78bf0a9d13985ee2ac1157a2"; # pinned commit
            sha256 = "sha256-JXdQRZ1KtsLY24bz50PKFYI5yojKwOwhTjEOGbDLebI=";
          };
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "cs2-ec";
            version = "0.1.0";
            src = ./.;

            strictDeps = true;

            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [
              pkgs.sdl3
              pkgs.linuxHeaders
            ];

            dontConfigure = true;
            NIX_CFLAGS_COMPILE = "-O3 -DNDEBUG -Wall -Wno-format-truncation -Wno-strict-aliasing";

            buildPhase = ''
              runHook preBuild

              # Vendor vm submodule if missing
              if [ ! -e library/vm ]; then
                mkdir -p library
                cp -r ${vmSrc} library/vm
              fi

              # Use pkg-config for flags
              SDL_CFLAGS="$(pkg-config --cflags sdl3)"
              SDL_LDFLAGS="$(pkg-config --libs sdl3)"

              $CXX -std=c++17 $CXXFLAGS $SDL_CFLAGS -o cs2-ec \
                projects/LINUX/main.cpp \
                cs2/shared/cs2.cpp cs2/shared/cs2features.cpp \
                apex/shared/apex.cpp apex/shared/apexfeatures.cpp \
                library/vm/linux/vm.cpp \
                -pthread $SDL_LDFLAGS -ldl

              runHook postBuild
            '';

            installPhase = ''
              install -Dm755 cs2-ec "$out/bin/cs2-ec"
            '';

            meta = {
              description = "EC (cs2-ec) utility";
              homepage = "https://github.com/AtelierMizumi/EC";
              platforms = [ "x86_64-linux" ];
            };
          };
        }
      );

      apps = forEachSystem (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/cs2-ec";
        };
      });
    };
}
