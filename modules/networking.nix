{ config, lib, pkgs, ... }:

{
  time.timeZone = "America/Costa_Rica";
  networking.networkmanager.enable = true;
  networking.extraHosts = ''
    127.0.0.1 presence.gog.com
    127.0.0.1 galaxy.gog.com
    127.0.0.1 external-galaxy.gog.com
  '';
  networking.firewall.enable = true;
  networking.firewall.allowedTCPPorts = [
    8080
    9999
    24800
    27036
    27037
    24642
  ];
  networking.firewall.allowedUDPPorts = [
    27000 27001 27002 27003 27004 27005 27006 27007 27008 27009
    27010 27011 27012 27013 27014 27015 27016 27017 27018 27019
    27020 27021 27022 27023 27024 27025 27026 27027 27028 27029
    27030 27031
    27036
    3478
    4379
    4380
    24642
  ];

  boot.extraModprobeConfig = ''
    options iwlwifi power_save=0 disable_11ax=1
  '';
}
