{ config, lib, pkgs, ... }:

{
  time.timeZone = "America/Costa_Rica";
  networking.networkmanager.enable = true;
}
