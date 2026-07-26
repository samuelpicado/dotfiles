{ inputs, config, lib, pkgs, ... }:

{
  nixpkgs.overlays = [ inputs.nix-cachyos-kernel.overlays.pinned ];

  nix.settings = {
    experimental-features = [ "nix-command" "flakes" ];
    substituters = [ "https://attic.xuyh0120.win/lantian" ];
    trusted-public-keys = [ "lantian:EeAUQ+W+6r7EtwnmYjeVwx5kOGEBpjlBfPlzGlTNvHc=" ];
  };

  nixpkgs.config.allowUnfree = true;

  system.stateVersion = "26.11";
}
