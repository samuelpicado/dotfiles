{ config, lib, pkgs, ... }:

{
  users.users.pikdo = {
    isNormalUser = true;
    extraGroups = [ "wheel" "docker" "audio" "video" "networkmanager" ];
    shell = pkgs.zsh;
  };

  programs.zsh.enable = true;

  security.sudo.wheelNeedsPassword = false;
}
