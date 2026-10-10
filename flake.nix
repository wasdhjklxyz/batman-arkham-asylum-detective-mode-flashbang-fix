{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    systems.url = "github:nix-systems/default";
  };
  outputs = { self, nixpkgs, systems }:
    let
      eachSystem = nixpkgs.lib.genAttrs (import systems);
    in {
      packages = eachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          web = pkgs.stdenv.mkDerivation {
            name = "baadmff-web";
            src = pkgs.lib.cleanSource ./.;
            nativeBuildInputs = with pkgs; [ emscripten gnumake ];
            buildPhase = ''
              make web
            '';
            installPhase = "cp -r build/web $out";
          };
        });

      devShells = eachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            name = "baadmff";
            packages = with pkgs; [ gnumake emscripten ];
            inputsFrom = [];
            env = {};
            shellHook = '''';
          };
        });
    };
}
