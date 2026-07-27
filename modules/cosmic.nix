{ config, lib, pkgs, ... }:

{
  services.xserver.enable = true;

  services.desktopManager.cosmic.enable = true;
  services.displayManager.cosmic-greeter.enable = true;

  services.gnome.gnome-keyring.enable = true;

  environment.sessionVariables.COSMIC_DATA_CONTROL_ENABLED = "1";

  environment.cosmic.excludePackages = with pkgs; [
    cosmic-store
  ];

  environment.systemPackages = with pkgs; [
    cosmic-applets
    cosmic-ext-applet-minimon
  ];
}
