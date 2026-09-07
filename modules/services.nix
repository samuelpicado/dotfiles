{ config, lib, pkgs, ... }:

{
  services.logind.settings.Login = {
    HandlePowerKey = "ignore";
    HandlePowerKeyLongPress = "ignore";
  };

  services.flatpak.enable = true;

  services.power-profiles-daemon.enable = true;
  services.thermald.enable = true;

  services.gvfs.enable = true;
  services.udisks2.enable = true;

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

  virtualisation.docker.enable = false;
}
