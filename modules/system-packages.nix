{ config, lib, pkgs, ... }:

{
  environment.systemPackages = with pkgs; [
    vim
    wget
    gnome-tweaks
    gnome-extension-manager
  ];
}
