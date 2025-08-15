{
  description = "Nix flake to build and install cs2-ec";

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
          lib = pkgs.lib;
          vmSrc = pkgs.fetchFromGitHub {
            owner = "ekknod";
            repo = "vm";
            rev = "master"; # Pin to a commit for reproducibility when known
            sha256 = lib.fakeSha256; # Replace with the hash Nix prints on first build
          };
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "cs2-ec";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [ pkgs.SDL3 ];

            dontConfigure = true;
            NIX_CFLAGS_COMPILE = "-O3 -DNDEBUG -Wall -Wno-format-truncation -Wno-strict-aliasing";

            buildPhase = ''
              runHook preBuild

              # Vendor vm submodule if missing
              if [ ! -e library/vm ]; then
                mkdir -p library
                cp -r ${vmSrc} library/vm
              fi

              # Provide SDL3 headers on expected path used by the codebase
              mkdir -p library/SDL3/include
              ln -sf ${pkgs.SDL3.dev}/include/SDL3 library/SDL3/include/SDL3

              # Resolve SDL3 link flags (ensures rpath is set)
              SDL_LDFLAGS="$(pkg-config --libs sdl3)"

              $CXX -std=c++17 -o cs2-ec \
                projects/LINUX/main.cpp \
                cs2/shared/cs2.cpp cs2/shared/cs2features.cpp \
                apex/shared/apex.cpp apex/shared/apexfeatures.cpp \
                library/vm/linux/vm.cpp \
                -pthread $SDL_LDFLAGS

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
