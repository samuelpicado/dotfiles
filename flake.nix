{
  description = "Pikdo's NixOS configuration";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    hyprmod = {
      url = "github:BlueManCZ/hyprmod";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    astal = {
      url = "github:Aylur/astal/fd94e333c8bd45557291c805f1df79d5f787d590";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    ags = {
      # The island uses the AGS 2 / Astal JSX API.
      url = "github:Aylur/ags/v2.3.0";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.astal.follows = "astal";
    };
  };

  outputs = { self, nixpkgs, home-manager, ... }@inputs:
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
      packages.${system}.cascade-overview =
        nixpkgs.legacyPackages.${system}.callPackage ./pkgs/cascade-overview.nix { };

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
