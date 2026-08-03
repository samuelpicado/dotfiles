{ config, lib, pkgs, ... }:

{
  imports = [
    ./hardware-configuration.nix
    ../../modules
  ];

  networking.hostName = "hp830";

  boot = {
    kernelParams = [ "noresume" ];
  };

  services.pipewire.wireplumber.configPackages = [
    (pkgs.writeTextDir "share/wireplumber/wireplumber.conf.d/90-audio-preferences.conf" ''
      wireplumber.settings = {
        default.audio.sink = "alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__Speaker__sink"
      }
      device.profile.priority.rules = [
        {
          matches = [
            {
              device.name = "alsa_card.pci-0000_00_1f.3-platform-skl_hda_dsp_generic"
            }
          ]
          actions = {
            update-props = {
              priorities = [
                "HiFi (HDMI1, HDMI2, HDMI3, Mic1, Mic2, Speaker)"
                "HiFi (HDMI1, HDMI2, HDMI3, Headphones, Mic1, Mic2)"
              ]
            }
          }
        }
      ]
    '')
  ];
}
