{ config, lib, pkgs, ... }:

{
  services.flatpak.enable = true;

  systemd.oomd = {
    enable = true;
    enableRootSlice = true;
    enableSystemSlice = true;
    enableUserSlices = false;
  };

  virtualisation.docker.enable = true;
}
