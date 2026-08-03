{ config, lib, pkgs, ... }:

{
  environment.systemPackages = with pkgs; [
    vim
    wget
    flatpak
    gnome-software
    gnumake
    cmake
    cpio
    pkgconf
    hyprwayland-scanner
  ];
}
