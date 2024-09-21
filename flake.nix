{
  description = "CS3502 Operating Systems Project 1";
  inputs.nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

  outputs = { self, nixpkgs }:
    let pkgs = nixpkgs.legacyPackages.x86_64-linux; in {
      devShells.x86_64-linux.default = pkgs.mkShell {
        buildInputs = with pkgs; [ zig_0_13 ];
      };

      packages.x86_64-linux.default = pkgs.stdenv.mkDerivation {
        pname = "banking-system";
        version = "0.1.0";
        src = ./.;
        nativeBuildInputs = with pkgs; [ zig_0_13.hook ];
      };

      apps.x86_64-linux = {
        client = {
          type = "app";
          program = "${self.packages.x86_64-linux.default}/bin/bank-client";
        };
        server = {
          type = "app";
          program = "${self.packages.x86_64-linux.default}/bin/bank-server";
        };
      };
    };
}
