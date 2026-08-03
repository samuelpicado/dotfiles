{
  description = "Pikdo's NixOS configuration";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nix-cachyos-kernel.url = "github:xddxdd/nix-cachyos-kernel/release";
    hyprmod = {
      url = "github:BlueManCZ/hyprmod";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, home-manager, nix-cachyos-kernel, ... }@inputs:
    let
      system = "x86_64-linux";

      homeManagerModule = { ... }: {
        imports = [
          home-manager.nixosModules.home-manager
        ];
        home-manager = {
          useGlobalPkgs = true;
          useUserPackages = true;
          backupFileExtension = "backup";
          users.pikdo = import ./home.nix;
        };
      };
    in
    {
      nixosConfigurations = {
        x13 = nixpkgs.lib.nixosSystem {
          inherit system;
          specialArgs = { inherit inputs; };
          modules = [
            ./hosts/x13/configuration.nix
            homeManagerModule
          ];
        };
        hp830 = nixpkgs.lib.nixosSystem {
          inherit system;
          specialArgs = { inherit inputs; };
          modules = [
            ./hosts/hp830/configuration.nix
            homeManagerModule
          ];
        };
      };
    };
}
