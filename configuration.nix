{ config, lib, pkgs, ... }:

{
  imports = [
    ./hardware-configuration.nix
  ];

  # --- Boot ---
  boot = {
    kernelPackages = pkgs.linuxPackages_zen;
    loader = {
      systemd-boot = {
        enable = true;
        configurationLimit = 10;
      };
      efi.canTouchEfiVariables = true;
    };
  };

  # --- Networking ---
  networking = {
    hostName = "nixos";
    networkmanager.enable = true;
  };

  time.timeZone = "America/Costa_Rica";

  # --- Users ---
  users.users.pikdo = {
    isNormalUser = true;
    extraGroups = [ "wheel" "docker" "audio" "video" "networkmanager" ];
    shell = pkgs.zsh;
  };

  programs.zsh.enable = true;

  security.sudo.wheelNeedsPassword = false;

  # --- System packages ---
  environment.systemPackages = with pkgs; [
    vim
    wget
    gnome-tweaks
    gnome-extension-manager
  ];

  # --- GNOME ---
  services = {
    displayManager.gdm.enable = true;
    desktopManager.gnome.enable = true;
    flatpak.enable = true;
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

  # --- Audio ---
  services.pipewire = {
    enable = true;
    alsa = {
      enable = true;
      support32Bit = true;
    };
    pulse.enable = true;
    wireplumber.enable = true;
  };

  # --- Swap ---
  zramSwap = {
    enable = true;
    algorithm = "zstd"; # zstd = compresión rápida y eficiente, lz4 = compresión rápida pero menos eficiente
    memoryPercent = 50;
  };

  # --- OOMD ---
  systemd.oomd = {
    enable = true;
    enableRootSlice = true;
    enableSystemSlice = true;
    enableUserSlices = false; # Disables killing user sessions
  };


  # --- Virtualisation ---
  virtualisation.docker.enable = true;

  # --- Nix ---
  nix.settings.experimental-features = [ "nix-command" "flakes" ];
  nixpkgs.config.allowUnfree = true;

  system.stateVersion = "26.11";
}
