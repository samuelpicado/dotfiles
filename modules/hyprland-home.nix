{ config, lib, pkgs, ... }:

let
  current-wallpaper = "${config.home.homeDirectory}/.cache/current-wallpaper";
  wallpaper = pkgs.fetchurl {
    url = "https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=3840";
    name = "wallpaper.jpg";
    hash = "sha256-Jenp9iP9BmCRvrlPeEKAwLHVbSdwxDalJRCmeT1L55Q=";
  };
  wallpaper-ocean = pkgs.fetchurl {
    url = "https://images.unsplash.com/photo-1505144808419-1957a94ca61e?w=3840";
    name = "wallpaper-ocean.jpg";
    hash = "sha256-lEyCISeq06boUAuYYOdBOdCdypNYXntk+Z8F3BPvfqo=";
  };
  wallpaper-lavender = pkgs.fetchurl {
    url = "https://images.unsplash.com/photo-1499002238440-d264edd596ec?w=3840";
    name = "wallpaper-lavender.jpg";
    hash = "sha256-D3wybsHoJCeuMEF80nHkvCqWq9Dss1hUK4vMhmxuhXk=";
  };
  terminal-opener = pkgs.writeShellScriptBin "exo-open" ''
    set -euo pipefail
    working_dir="$PWD"
    launch=""
    while [ "$#" -gt 0 ]; do
      case "$1" in
        --launch)
          launch="''${2:-}"
          shift 2
          ;;
        --working-directory=*)
          working_dir="''${1#*=}"
          shift
          ;;
        --working-directory)
          working_dir="''${2:-$PWD}"
          shift 2
          ;;
        *)
          shift
          ;;
      esac
    done

    if [ "$launch" = "TerminalEmulator" ]; then
      exec ${pkgs.foot}/bin/foot --working-directory="$working_dir"
    fi

    exit 0
  '';
  wallpaper-cycle = pkgs.writeShellScript "wallpaper-cycle" ''
    set -eu
    wallpapers=(
      "${wallpaper}"
      "${wallpaper-ocean}"
      "${wallpaper-lavender}"
    )
    state_file="${config.home.homeDirectory}/.cache/wallpaper-index"
    mkdir -p "$(dirname "$state_file")" "$(dirname "${current-wallpaper}")"
    index=0
    if [ -s "$state_file" ]; then
      index=$(cat "$state_file")
    fi
    case "$index" in
      0|1|2) ;;
      *) index=0 ;;
    esac
    ln -sfn "''${wallpapers[$index]}" "${current-wallpaper}"
    printf '%s\n' "$(( (index + 1) % ''${#wallpapers[@]} ))" > "$state_file"
    systemctl --user restart swaybg
  '';
in

{
  services.dunst = {
    enable = true;
    settings = {
      global = {
        monitor = 0;
        follow = "mouse";
        width = 300;
        height = 300;
        origin = "top-right";
        offset = "10x50";
        scale = 0;
        notification_limit = 20;
        progress_bar = true;
        transparency = 10;
        corner_radius = 8;
        horizontal_padding = 12;
        frame_width = 1;
        frame_color = "#89b4fa";
        font = "JetBrainsMono Nerd Font 11";
        foreground = "#cdd6f4";
        background = "#1e1e2eCC";
        timeout = 10;
        separator_color = "frame";
        show_indicators = false;
        icon_position = "left";
        min_icon_size = 32;
        max_icon_size = 64;
      };
      urgency_low = {
        background = "#1e1e2eCC";
        foreground = "#cdd6f4";
        timeout = 5;
      };
      urgency_normal = {
        background = "#1e1e2eCC";
        foreground = "#cdd6f4";
        timeout = 8;
      };
      urgency_critical = {
        background = "#f38ba8CC";
        foreground = "#cdd6f4";
        timeout = 0;
      };
    };
  };

  programs.waybar = {
    enable = false;
    style = ''
      * {
        border: none;
        border-radius: 0;
        font-family: "JetBrainsMono Nerd Font";
        font-size: 13px;
        min-height: 0;
      }

      window#waybar {
        background: rgba(24, 24, 37, 0.95);
        color: #cdd6f4;
        border-bottom: 1px solid rgba(137, 180, 250, 0.2);
      }

      #workspaces button {
        padding: 0 6px;
        background: transparent;
        color: #585b70;
        border-bottom: 2px solid transparent;
      }

      #workspaces button.active {
        color: #89b4fa;
        border-bottom: 2px solid #89b4fa;
      }

      #workspaces button:hover {
        background: rgba(137, 180, 250, 0.15);
        text-shadow: none;
      }

      #clock, #pulseaudio, #network, #battery, #tray, #bluetooth {
        padding: 0 10px;
        color: #cdd6f4;
      }

      #clock { color: #89b4fa; }
      #pulseaudio { color: #a6e3a1; }
      #network { color: #89dceb; }
      #battery { color: #cba6f7; }
      #tray { color: #cdd6f4; }
      #custom-notification { color: #f9e2af; }
      #custom-apps {
        padding: 0 12px;
        color: #89b4fa;
      }
      #custom-apps:hover {
        background: rgba(137, 180, 250, 0.15);
      }
    '';
    settings = [{
      layer = "top";
      position = "top";
      exclusive = false;
      height = 28;
      modules-left = [ "custom/apps" "hyprland/workspaces" ];
      modules-center = [];
      modules-right = [ "pulseaudio" "network" "bluetooth" "battery" "custom/settings" "tray" ];
      "custom/apps" = {
        format = " Apps";
        on-click = "nwg-drawer";
        tooltip = false;
      };
      "custom/settings" = {
        format = "";
        on-click = "pkill nwg-bar 2>/dev/null; nwg-bar";
        tooltip = true;
        tooltip-format = "System Settings (SUPER+,)";
      };
      clock = {
        format = "{:%H:%M  %a %d/%b}";
        tooltip-format = "{:%Y-%m-%d | %H:%M}";
        interval = 30;
      };
      pulseaudio = {
        format = "{icon} {volume}%";
        format-muted = " Muted";
        format-icons = { default = [ "" "" ]; };
        on-click = "pavucontrol";
        on-scroll-up = "wpctl set-volume @DEFAULT_AUDIO_SINK@ 2%+";
        on-scroll-down = "wpctl set-volume @DEFAULT_AUDIO_SINK@ 2%-";
      };
      network = {
        format-wifi = "  {essid}";
        format-ethernet = "  {ifname}";
        format-disconnected = "  Disconnected";
        tooltip-format = "{ipaddr}";
        interval = 30;
      };
      bluetooth = {
        format = "";
        format-disabled = "";
        format-connected = " {num_connections}";
        tooltip-format = "{controller_alias}\t{controller_address}\n\n{device_enumerate}";
        on-click = "blueman-manager";
      };
      battery = {
        bat = "BAT0";
        format = "{icon} {capacity}%";
        format-charging = " {capacity}%";
        format-plugged = " {capacity}%";
        format-icons = [ "" "" "" "" "" ];
        interval = 60;
      };
      tray = {
        icon-size = 16;
        spacing = 6;
      };
    }];
  };

  programs.foot = {
    enable = true;
    settings = {
      main = {
        font = "JetBrainsMono Nerd Font:size=12";
        pad = "8x8";
        initial-color-theme = "dark";
        shell = "${pkgs.zsh}/bin/zsh";
      };
      # Catppuccin Mocha (igual que el themeFile de kitty).
      # foot >= 1.26 usa colors-dark / colors-light en vez de [colors].
      colors-dark = {
        alpha = 0.82;
        background = "1e1e2e";
        foreground = "cdd6f4";
        selection-foreground = "1e1e2e";
        selection-background = "585b70";
        # dos valores: texto  cursor (fondo)
        cursor = "1e1e2e f5e0dc";
        regular0 = "45475a";
        regular1 = "f38ba8";
        regular2 = "a6e3a1";
        regular3 = "f9e2af";
        regular4 = "89b4fa";
        regular5 = "f5c2e7";
        regular6 = "94e2d5";
        regular7 = "bac2de";
        bright0 = "585b70";
        bright1 = "f38ba8";
        bright2 = "a6e3a1";
        bright3 = "f9e2af";
        bright4 = "89b4fa";
        bright5 = "f5c2e7";
        bright6 = "94e2d5";
        bright7 = "a6adc8";
      };
      bell = {
        urgent = "no";
      };
      cursor = {
        style = "block";
        blink = "no";
      };
      # Sin decoraciones de ventana (equivalente a hide_window_decorations).
      # border-width vive en la seccion [csd] en foot >= 1.26.
      csd = {
        border-width = 0;
      };
      # Keybindings estilo kitty (foot no tiene pestanas: Control+Shift+t/n abren
      # una nueva instancia/ventana, como hacia kitty con new-window).
      # Formato foot >= 1.26: accion=combo1 combo2 ... (combo accion invertido).
      key-bindings = {
        "clipboard-copy" = "Control+Shift+c";
        "clipboard-paste" = "Control+Shift+v";
        "spawn-terminal" = "Control+Shift+n Control+Shift+t";
      };
    };
  };

  gtk = {
    enable = true;
    theme = {
      name = "Adwaita-dark";
      package = pkgs.gnome-themes-extra;
    };
    iconTheme = {
      name = "Papirus-Dark";
      package = pkgs.papirus-icon-theme;
    };
    cursorTheme = {
      name = "Adwaita";
      package = pkgs.adwaita-icon-theme;
    };
    font = {
      name = "JetBrainsMono Nerd Font";
      size = 11;
    };
    gtk3.extraConfig = {
      gtk-application-prefer-dark-theme = true;
      gtk-decoration-layout = "menu:";
    };
    gtk4.extraConfig = {
      gtk-application-prefer-dark-theme = true;
    };
  };

  home.packages = with pkgs; [
    foot
    terminal-opener
    fuzzel
    swaybg
    papirus-icon-theme

    thunar
    thunar-archive-plugin
    thunar-volman
    gnome-calculator
    file-roller
    imv
    mpv
    nwg-drawer
    nwg-bar
    blueman
    enchant
    hunspell
    hunspellDicts.en_US
    hunspellDicts.es-any
  ];

  services.hypridle = {
    enable = true;
    settings = {
      general = {
        after_sleep_cmd = "hyprlock";
        before_sleep_cmd = "hyprlock";
        lock_cmd = "hyprlock";
      };
      listener = [
        { timeout = 300; on-timeout = "hyprlock"; }
        { timeout = 600; on-timeout = "hyprctl dispatch 'hl.dsp.dpms({action = \"off\"})'"; on-resume = "hyprctl dispatch 'hl.dsp.dpms({action = \"on\"})'"; }
      ];
    };
  };

  dconf.settings = {
    "org/gnome/shell/extensions/user-theme" = {
      name = "Adwaita-dark";
    };
  };

  systemd.user.services.swaybg = {
    Unit = {
      Description = "Wallpaper daemon";
      After = [ "graphical-session.target" ];
      PartOf = [ "graphical-session.target" ];
    };
    Service = {
       ExecStart = "${pkgs.swaybg}/bin/swaybg -i ${current-wallpaper} -m fill";
      Restart = "on-failure";
      RestartSec = 1;
    };
    Install = {
      WantedBy = [ "graphical-session.target" ];
    };
  };

  systemd.user.services.wallpaper-cycle = {
    Unit = {
      Description = "Rotate desktop wallpaper";
      After = [ "graphical-session.target" ];
      PartOf = [ "graphical-session.target" ];
    };
    Service = {
      Type = "oneshot";
      ExecStart = "${wallpaper-cycle}";
    };
  };

  systemd.user.timers.wallpaper-cycle = {
    Unit = {
      Description = "Rotate desktop wallpaper every four hours";
      PartOf = [ "graphical-session.target" ];
    };
    Timer = {
      OnBootSec = "1min";
      OnUnitActiveSec = "4h";
      Persistent = true;
    };
    Install = {
      WantedBy = [ "graphical-session.target" ];
    };
  };

  xdg.configFile."ags/app.tsx".source = ../ags/app.tsx;
  xdg.configFile."ags/tsconfig.json".source = ../ags/tsconfig.json;

  xdg.configFile."hypr/hyprlock.conf".text = ''
    general {
        hide_cursor = true
        grace = 0
        no_fade_in = false
    }

    background {
        monitor =
        path = ${current-wallpaper}
        color = rgba(17, 17, 27, 1.0)
        blur_passes = 2
        blur_size = 4
        contrast = 0.9
        brightness = 0.75
    }

    label {
        monitor =
        text = cmd[update:1000] ${pkgs.coreutils}/bin/date '+%H:%M'
        color = rgba(205, 214, 244, 0.96)
        font_size = 64
        font_family = JetBrainsMono Nerd Font
        position = 0, 145
        halign = center
        valign = center
    }

    label {
        monitor =
        text = cmd[update:60000] ${pkgs.coreutils}/bin/date '+%A, %d %B'
        color = rgba(205, 214, 244, 0.72)
        font_size = 16
        font_family = JetBrainsMono Nerd Font
        position = 0, 78
        halign = center
        valign = center
    }

    label {
        monitor =
        text = cmd[update:60000] ${pkgs.bash}/bin/bash -c 'capacity=$(${pkgs.coreutils}/bin/cat /sys/class/power_supply/BAT*/capacity 2>/dev/null | ${pkgs.coreutils}/bin/head -1); status=$(${pkgs.coreutils}/bin/cat /sys/class/power_supply/BAT*/status 2>/dev/null | ${pkgs.coreutils}/bin/head -1); [ -n "$capacity" ] && printf "%s%%  %s" "$capacity" "$status"'
        color = rgba(205, 214, 244, 0.62)
        font_size = 13
        font_family = JetBrainsMono Nerd Font
        position = 0, -42
        halign = center
        valign = center
    }

    input-field {
        monitor =
        size = 320, 54
        outline_thickness = 1
        outer_color = rgba(0, 0, 0, 0)
        inner_color = rgba(0, 0, 0, 0.28)
        font_color = rgba(205, 214, 244, 0.95)
        fade_on_empty = false
        dots_size = 0.22
        dots_spacing = 0.28
        dots_center = true
        placeholder_text = <i>Enter password</i>
        check_color = rgba(166, 227, 161, 0.9)
        fail_color = rgba(243, 139, 168, 0.9)
        capslock_color = rgba(249, 226, 175, 0.9)
        fail_text = <i>Authentication failed</i>
        position = 0, -125
        halign = center
        valign = center
    }
  '';

  xdg.configFile."fuzzel/fuzzel.ini".text = ''
    [main]
    font=JetBrainsMono Nerd Font:size=12
    prompt=>
    terminal=foot
    namespace=fuzzel
    match-mode=fuzzy
    filter-desktop=no

    width=40
    lines=10
    horizontal-pad=12
    vertical-pad=12
    inner-pad=8

    [colors]
    background=1e1e2ed9
    text=ffffffff
    placeholder=808080ff
    input=ffffffff

    [border]
    width=0
    radius=8
  '';

  xdg = {
    mime.enable = true;
    userDirs = {
      enable = true;
      createDirectories = true;
      download = "$HOME/Downloads";
      documents = "$HOME/Documents";
      desktop = "$HOME/Desktop";
      pictures = "$HOME/Pictures";
      videos = "$HOME/Videos";
      music = "$HOME/Music";
      templates = "$HOME/Templates";
      publicShare = "$HOME/Public";
    };
    desktopEntries = {
      foot-server-hidden = {
        name = "Foot Server";
        noDisplay = true;
      };
      footclient-hidden = {
        name = "Foot Client";
        noDisplay = true;
      };
      thunar-bulk-rename-hidden = {
        name = "Bulk Rename";
        noDisplay = true;
      };
    };
  };

  xdg.configFile."xfce4/helpers.rc".text = ''
    TerminalEmulator=foot
  '';
}
