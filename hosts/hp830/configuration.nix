{ config, lib, pkgs, ... }:

{
  imports = [
    ./hardware-configuration.nix
    ../../modules
    ../../modules/virtualization.nix
  ];

  networking.hostName = "hp830";

  boot = {
    kernelParams = [ "noresume" ];
  };

  services.pipewire.wireplumber.configPackages = [
    (pkgs.writeTextDir "share/wireplumber/wireplumber.conf.d/90-audio-preferences.conf" ''
      wireplumber.settings = {
        node.restore-default-targets = false
      }
      # Profile choice is persisted by WirePlumber itself (device.restore-profile
      # defaults to true); no priority override so the user's pick always sticks.
    '')
  ];
}
