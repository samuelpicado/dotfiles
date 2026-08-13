{ config, lib, pkgs, ... }:

{
  environment.systemPackages = with pkgs; [
    vim
    wget
    flatpak
    intel-gpu-tools
    gnome-software
  ];
}
