{ config, lib, pkgs, ... }:

{
  services.flatpak.enable = true;

  hardware.bluetooth.enable = true;

  systemd.oomd = {
    enable = true;
    enableRootSlice = true;
    enableSystemSlice = true;
    enableUserSlices = false;
  };

  hardware.steam-hardware.enable = true;
  programs.steam = {
    enable = true;
    extraCompatPackages = with pkgs; [ proton-ge-bin ];
  };

  services.zerotierone.enable = true;
  services.zerotierone.joinNetworks = [ "88c5b1f339c29ad7" ];

  virtualisation.docker.enable = true;
  virtualisation.docker.daemon.settings = {
    bip = "172.30.0.1/16";
    default-address-pools = [
      { base = "172.31.0.0/16"; size = 24; }
    ];
  };
}
