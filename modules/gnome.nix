{ config, lib, pkgs, ... }:

{
  services = {
    displayManager.gdm.enable = true;
    desktopManager.gnome.enable = true;
    gnome = {
      core-apps.enable = true;
      games.enable = false;
      gnome-keyring.enable = true;
    };
  };

  environment.gnome.excludePackages = with pkgs; [
    gnome-tour
    gnome-user-docs
    gnome-weather
    gnome-contacts
    epiphany
    gnome-maps
    gnome-connections
  ];
}
