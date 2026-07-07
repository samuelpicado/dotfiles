{
  description = "Pikdo's NixOS configuration";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, home-manager, ... }:
    let
      system = "x86_64-linux";

      homeManagerModule = { ... }: {
        imports = [
          home-manager.nixosModules.home-manager
        ];
        home-manager = {
          useGlobalPkgs = true;
          useUserPackages = true;
          users.pikdo = import ./home.nix;
        };
      };
    in
    {
      nixosConfigurations = {
        x13 = nixpkgs.lib.nixosSystem {
          inherit system;
          modules = [
            ./hosts/x13/configuration.nix
            homeManagerModule
          ];
        };
        hp830 = nixpkgs.lib.nixosSystem {
          inherit system;
          modules = [
            ./hosts/hp830/configuration.nix
            homeManagerModule
          ];
        };
      };
    };
}
