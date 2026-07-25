{ config, lib, pkgs, ... }:

let
  enabledExtensions = with pkgs.gnomeExtensions; [
    appindicator
    blur-my-shell
    dash-to-dock
    gnome-40-ui-improvements
    hide-top-bar
    impatience
    luminus-desktop
    user-themes
  ];
in
{
  services = {
    displayManager.gdm.enable = true;
    desktopManager.gnome.enable = true;
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

  environment.systemPackages = enabledExtensions;

  programs.dconf = {
    enable = true;
    profiles.user.databases = [
      {
        settings = {
          "org/gnome/shell" = {
            enabled-extensions = map (ext: ext.extensionUuid) enabledExtensions;
            favorite-apps = [
              "firefox.desktop"
              "org.gnome.Terminal.desktop"
              "code.desktop"
            ];
          };

          "org/gnome/shell/extensions/user-theme" = {
            name = "Tahoe-Dark";
          };

          "org/gnome/desktop/interface" = {
            gtk-theme = "Tahoe-Dark";
            color-scheme = "prefer-dark";
          };

          "org/gnome/desktop/background" = {
            picture-uri = "file:///run/current-system/sw/share/backgrounds/Tahoe/Tahoe-5k-dark.jpg";
            picture-uri-dark = "file:///run/current-system/sw/share/backgrounds/Tahoe/Tahoe-5k-dark.jpg";
          };

          "org/gnome/desktop/screensaver" = {
            picture-uri = "file:///run/current-system/sw/share/backgrounds/Tahoe/Tahoe-5k-dark.jpg";
          };

          "org/gnome/shell/extensions/dash-to-dock" = {
            autohide = true;
            dock-fixed = true;
            intellihide = true;
            dock-position = "BOTTOM";
            extend-height = false;
            dash-max-icon-size = lib.gvariant.mkInt32 48;
            background-opacity = 0.5;
            custom-theme-shrink = true;
            running-indicator-style = "DOTS";
          };

          "org/gnome/shell/extensions/blur-my-shell" = {
            settings-version = lib.gvariant.mkInt32 2;
          };

          "org/gnome/shell/extensions/blur-my-shell/panel" = {
            blur = true;
            static-blur = true;
            style-panel = lib.gvariant.mkInt32 0;
            override-background = true;
            override-background-dynamism = true;
          };

          "org/gnome/shell/extensions/blur-my-shell/applications" = {
            blur-apps = true;
            enable-apps = true;
          };

          "org/gnome/shell/extensions/blur-my-shell/overview" = {
            blur = true;
            style-components = lib.gvariant.mkInt32 2;
            background-blur = true;
          };

          "org/gnome/shell/extensions/blur-my-shell/dash-to-dock" = {
            blur = true;
            static-blur = false;
          };

          "org/gnome/shell/extensions/hidetopbar" = {
            hide-topbar = true;
            hide-in-overview = true;
          };

          "org/gnome/shell/extensions/impatience" = {
            speed-factor = 0.75;
          };

          "org/gnome/shell/extensions/gnome-ui-tune" = {
            overview-firefox-pip = true;
            hide-window-titlebar-ssd = true;
            hide-overview-search-providers = true;
          };
        };
      }
    ];
  };
}
