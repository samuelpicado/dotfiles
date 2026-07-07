{ config, lib, pkgs, ... }:

{
  imports = [
    ./hardware-configuration.nix
    ../../modules
  ];

  networking.hostName = "hp830";

  boot = {
    resumeDevice = lib.mkForce null;
    kernelParams = [ "noresume" ];
  };
}
