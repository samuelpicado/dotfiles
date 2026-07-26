{ config, lib, pkgs, ... }:

{
  boot = {
    kernelPackages = pkgs.cachyosKernels.linuxPackages-cachyos-bore-lto;
    loader = {
      systemd-boot = {
        enable = true;
        configurationLimit = 10;
      };
      efi.canTouchEfiVariables = true;
    };
  };

  fileSystems."/tmp" = {
    device = "tmpfs";
    fsType = "tmpfs";
    options = [ "size=8G" ];
  };

  zramSwap = {
    enable = true;
    algorithm = "zstd";
    memoryPercent = 50;
  };
}
