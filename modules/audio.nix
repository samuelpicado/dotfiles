{ config, lib, pkgs, ... }:

{
  services.pipewire = {
    enable = true;
    alsa = {
      enable = true;
      support32Bit = true;
    };
    pulse.enable = true;
    wireplumber.enable = true;
  };

  hardware.firmware = with pkgs; [
    sof-firmware
  ];

  environment.systemPackages = with pkgs; [
    alsa-utils
  ];

  environment.etc."alsa/ucm2".source = "${pkgs.alsa-ucm-conf}/share/alsa/ucm2";

  services.pipewire.wireplumber.configPackages = [
    (pkgs.writeTextDir "share/wireplumber/wireplumber.conf.d/99-enable-ucm.conf" ''
      monitor.alsa.rules = [
        {
          matches = [
            {
              device.name = "~alsa_card.*"
            }
          ]
          actions = {
            update-props = {
              # Classic ACP mapping: ONE analog sink with selectable ports
              # ([Out] Speaker / [Out] Headphones) instead of exclusive UCM
              # profiles, so both are choosable regardless of jack state.
              api.alsa.use-acp = true
              api.alsa.use-ucm = false
              api.acp.auto-port = false
              api.acp.auto-profile = false
            }
          }
        }
        {
          # Analog outputs must outrank HDMI (route prio 700) so WirePlumber's
          # default-device policy prefers whichever one the user activated.
          matches = [
            {
              node.name = "~alsa_output.*HiFi__(Speaker|Headphones)__sink"
            }
          ]
          actions = {
            update-props = {
              priority.session = 3000
            }
          }
        }
      ]
    '')
  ];
}
