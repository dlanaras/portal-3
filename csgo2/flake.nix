{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    utils,
  }:
    utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};
      in {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            rnix-lsp
            alejandra
            go
            gotests
            gomodifytags
            impl
            delve
            go-tools
            gopls
          ];
        };

        formatter = pkgs.alejandra;
      }
    );
}
