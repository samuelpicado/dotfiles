{ config, lib, pkgs, inputs, ... }:

let
  hyprglass = pkgs.callPackage ../pkgs/hyprglass.nix { mkHyprlandPlugin = pkgs.hyprlandPlugins.mkHyprlandPlugin; };
  hyprspace = pkgs.callPackage ../pkgs/hyprspace.nix { };
  polkitAgent = "${pkgs.kdePackages.polkit-kde-agent-1}/libexec/polkit-kde-authentication-agent-1";
  ags-full = pkgs.ags.overrideAttrs (old: {
    buildInputs = (old.buildInputs or []) ++ [ pkgs.astal.network pkgs.astal.bluetooth pkgs.networkmanager ];
  });
  window-switcher = pkgs.writeShellScript "window-switcher" ''
    ${pkgs.hyprland}/bin/hyprctl clients -j \
      | ${pkgs.jq}/bin/jq -r '.[] | select(.title != "" and .mapped == true) | [.title, .address] | @tsv' \
      | ${pkgs.fuzzel}/bin/fuzzel -d --with-nth=1 --only-match --prompt="Window: " \
      | ${pkgs.coreutils}/bin/cut -f2 \
      | ${pkgs.gnused}/bin/sed 's/^/hl.dsp.focus({window="address:/; s/$/"})/' \
      | ${pkgs.findutils}/bin/xargs -r -d '\n' ${pkgs.hyprland}/bin/hyprctl dispatch
  '';

  hyprlandConf = pkgs.writeText "hyprland.lua" ''
    require("hyprland-gui")
    local mod = "SUPER"

    hl.monitor({
        output   = "",
        mode     = "preferred",
        position = "auto",
        scale    = "auto",
    })

    hl.config({
        input = {
            kb_layout  = "us,latam",
            kb_options = "",
            follow_mouse = 1,
            touchpad = {
                natural_scroll = true,
                tap_to_click   = true,
                drag_lock      = true,
                disable_while_typing = false,
            },
            sensitivity = 0,
        },
        gestures = {
            workspace_swipe_distance = 300,
            workspace_swipe_touch = true,
            workspace_swipe_touch_invert = false,
            workspace_swipe_min_speed_to_force = 30,
            workspace_swipe_cancel_ratio = 0.5,
            workspace_swipe_direction_lock = true,
            workspace_swipe_direction_lock_threshold = 10,
        },
        general = {
            layout = "dwindle",
            gaps_in  = 4,
            gaps_out = 8,
            border_size = 1,
            ["col.active_border"]   = "rgba(00000000)",
            ["col.inactive_border"] = "rgba(45475acc)",
        },
        decoration = {
            rounding = 4,
            blur = {
                enabled = true,
                size  = 4,
                passes = 1,
                new_optimizations = true,
            },
            shadow = {
                enabled     = true,
                range       = 8,
                render_power = 2,
            },
        },
        misc = {
            disable_hyprland_logo = true,
            disable_splash_rendering = true,
        },
        xwayland = {
            force_zero_scaling = true,
        },
        dwindle = {
            preserve_split = true,
            smart_split    = false,
            smart_resizing = true,
        },
    })

    -- Gestures (GNOME-like: 3-finger horizontal swipe for workspaces)
    hl.gesture({ fingers = 3, direction = "horizontal", action = "workspace" })
    hl.gesture({ fingers = 4, direction = "up",          action = function() hl.exec_cmd("${window-switcher}") end })
    hl.gesture({ fingers = 4, direction = "down",        action = function() hl.exec_cmd("foot") end })

    hl.curve("easeOutQuint",     { type = "bezier", points = { {0.23, 1}, {0.32, 1} } })
    hl.curve("easeInOutCubic",   { type = "bezier", points = { {0.65, 0}, {0.35, 1} } })
    hl.animation({ leaf = "windows",     enabled = true, speed = 3, bezier = "easeOutQuint",   style = "popin" })
    hl.animation({ leaf = "windowsOut",  enabled = true, speed = 3, bezier = "easeOutQuint",   style = "popin" })
    hl.animation({ leaf = "fade",        enabled = true, speed = 3, bezier = "easeOutQuint" })
    hl.animation({ leaf = "workspaces",  enabled = true, speed = 4, bezier = "easeInOutCubic", style = "slide" })
    hl.animation({ leaf = "border",      enabled = false })

    hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
    hl.env("XDG_SESSION_TYPE",    "wayland")
    hl.env("XDG_SESSION_DESKTOP", "Hyprland")
    hl.env("GTK_THEME",           "Adwaita-dark")
    hl.env("GDK_DPI_SCALE",       "1")
    hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")
    hl.env("HYPRLAND_NO_WARNINGS", "1")

    -- Hyprspace registers these options when its plugin is loaded. The guard
    -- keeps the first config parse valid and applies them on the plugin reload.
    if hl.plugin.overview then
        hl.config({
            plugin = {
                overview = {
                    disableGestures = false,
                    reverseSwipe = false,
                },
            },
        })
    end

    -- HyprGlass
    if hl.plugin.hyprglass then
        local hg = hl.plugin.hyprglass
        hg.config({
            enabled = false,
            default_theme = "dark",
            default_preset = "glass",
            glass_opacity = 0.85,
            blur_strength = 1.5,
            tint_color = 0x00000000,
            brightness = 0.85,
            refraction_strength = 0.3,
            fresnel_strength = 0.3,
            dark = { brightness = 0.78, contrast = 0.92 },
            light = { adaptive_boost = 0.4 },
            layers = { enabled = true },
        })
        hg.layer("fuzzel", { preset = "glass", mask_threshold = 0.05 })
    end

    hl.on("hyprland.start", function()
        hl.exec_cmd("hyprctl plugin load ${hyprglass}/lib/hyprglass.so")
        hl.exec_cmd("hyprctl plugin load ${hyprspace}/lib/libHyprspace.so")
        hl.exec_cmd("systemctl --user start wallpaper-cycle.timer")
        hl.exec_cmd("systemctl --user start wallpaper-cycle.service")
        hl.exec_cmd("hypridle")
        hl.exec_cmd("dunst")
        hl.exec_cmd("${polkitAgent}")
        hl.exec_cmd("nm-applet --indicator")
    hl.exec_cmd("bash -c 'while true; do ags run; sleep 5; done'")
    end)

    -- Window rules
    hl.window_rule({ match = { class = "^(foot|footclient)$" }, opacity = 0.77, tag = "+hyprglass_enabled" })
    hl.window_rule({ match = { class = "mpv" },       tag = "+hyprglass_disabled" })
    hl.window_rule({ match = { fullscreen = true },    opacity = "1.0 1.0 1.0 override", opaque = true, tag = "+hyprglass_disabled" })
    hl.window_rule({ match = { class = "^pavucontrol$" },          float = true, center = true, size = { 800, 600 } })
    hl.window_rule({ match = { title = "^Picture-in-Picture$" },   float = true })
    hl.window_rule({ match = { title = "^Volume Control$" },       float = true })
    hl.window_rule({ match = { class = "^nm-connection-editor$" }, float = true, center = true })
    hl.window_rule({ match = { class = "^org.gnome.Calculator$" }, float = true, center = true })
    hl.window_rule({ match = { class = "^blueman-manager$" },      float = true, center = true })
    hl.window_rule({ match = { class = "^xdg-desktop-portal" },    float = true, center = true })

    -- App launchers
    hl.bind(mod .. " + Q",               hl.dsp.exec_cmd("foot"))
    hl.bind(mod .. " + Space",           hl.dsp.exec_cmd("fuzzel"))
    hl.bind(mod .. " + R",               hl.dsp.exec_cmd("fuzzel"))
    hl.bind(mod .. " + E",               hl.dsp.exec_cmd("thunar"))
    hl.bind(mod .. " + W",               hl.dsp.exec_cmd("firefox"))
    hl.bind(mod .. " + SHIFT + W",       hl.dsp.exec_cmd("firefox --private-window"))
    hl.bind(mod .. " + Tab",             hl.dsp.exec_cmd("nwg-drawer"))
    hl.bind(mod .. " + D",               hl.dsp.exec_cmd("nwg-drawer"))
    hl.bind(mod .. " + L",               hl.dsp.exec_cmd("hyprlock"))
    hl.bind(mod .. " + SHIFT + S",       hl.dsp.exec_cmd("grim -g \"$(slurp)\" - | wl-copy && notify-send Screenshot 'Copied to clipboard'"))
    hl.bind(mod .. " + comma",           hl.dsp.exec_cmd("gnome-control-center"))
    hl.bind("XF86Search",               hl.dsp.exec_cmd("grim ~/Pictures/screenshot-$(date +%Y%m%d-%H%M%S).png && notify-send Screenshot 'Saved to ~/Pictures'"))
    hl.bind("ALT + F2",                  hl.dsp.exec_cmd("fuzzel"))

    -- Keyboard layout toggle (Shift+Alt)
    hl.bind("SHIFT + ALT_L", hl.dsp.exec_cmd("bash -c 'hyprctl switchxkblayout \"$(hyprctl devices -j | ${pkgs.jq}/bin/jq -r \".keyboards[]|select(.main).name\")\" next'"))

    -- GNOME-like overview (show windows)
    hl.bind(mod .. " + Grave",            hl.dsp.exec_cmd("${window-switcher}"))
    hl.bind(mod .. " + SHIFT + Grave",    hl.dsp.exec_cmd("${window-switcher}"))

    -- Window management
    hl.bind(mod .. " + C",               hl.dsp.window.close())
    hl.bind(mod .. " + F",               hl.dsp.window.fullscreen({ action = "toggle" }))
    hl.bind(mod .. " + V",               hl.dsp.window.float({ action = "toggle" }))
    hl.bind(mod .. " + P",               hl.dsp.window.pseudo())
    hl.bind("ALT + F4",                  hl.dsp.window.close())
    hl.bind(mod .. " + M",               hl.dsp.exit())
    hl.bind(mod .. " + SHIFT + Q",       hl.dsp.exec_cmd("pkill Hyprland"))

    -- Focus movement
    hl.bind(mod .. " + left",            hl.dsp.focus({ direction = "left" }))
    hl.bind(mod .. " + right",           hl.dsp.focus({ direction = "right" }))
    hl.bind(mod .. " + up",              hl.dsp.focus({ direction = "up" }))
    hl.bind(mod .. " + down",            hl.dsp.focus({ direction = "down" }))

    -- Window movement between tiled positions
    hl.bind(mod .. " + SHIFT + left",    hl.dsp.window.move({ direction = "left" }))
    hl.bind(mod .. " + SHIFT + right",   hl.dsp.window.move({ direction = "right" }))
    hl.bind(mod .. " + SHIFT + up",      hl.dsp.window.move({ direction = "up" }))
    hl.bind(mod .. " + SHIFT + down",    hl.dsp.window.move({ direction = "down" }))

    -- Workspace switching
    hl.bind(mod .. " + 1", hl.dsp.focus({ workspace = 1 }))
    hl.bind(mod .. " + 2", hl.dsp.focus({ workspace = 2 }))
    hl.bind(mod .. " + 3", hl.dsp.focus({ workspace = 3 }))
    hl.bind(mod .. " + 4", hl.dsp.focus({ workspace = 4 }))
    hl.bind(mod .. " + 5", hl.dsp.focus({ workspace = 5 }))
    hl.bind(mod .. " + 6", hl.dsp.focus({ workspace = 6 }))
    hl.bind(mod .. " + 7", hl.dsp.focus({ workspace = 7 }))
    hl.bind(mod .. " + 8", hl.dsp.focus({ workspace = 8 }))
    hl.bind(mod .. " + 9", hl.dsp.focus({ workspace = 9 }))
    hl.bind(mod .. " + 0", hl.dsp.focus({ workspace = 10 }))

    -- Move windows between workspaces
    hl.bind(mod .. " + SHIFT + 1", hl.dsp.window.move({ workspace = 1 }))
    hl.bind(mod .. " + SHIFT + 2", hl.dsp.window.move({ workspace = 2 }))
    hl.bind(mod .. " + SHIFT + 3", hl.dsp.window.move({ workspace = 3 }))
    hl.bind(mod .. " + SHIFT + 4", hl.dsp.window.move({ workspace = 4 }))
    hl.bind(mod .. " + SHIFT + 5", hl.dsp.window.move({ workspace = 5 }))
    hl.bind(mod .. " + SHIFT + 6", hl.dsp.window.move({ workspace = 6 }))
    hl.bind(mod .. " + SHIFT + 7", hl.dsp.window.move({ workspace = 7 }))
    hl.bind(mod .. " + SHIFT + 8", hl.dsp.window.move({ workspace = 8 }))
    hl.bind(mod .. " + SHIFT + 9", hl.dsp.window.move({ workspace = 9 }))
    hl.bind(mod .. " + SHIFT + 0", hl.dsp.window.move({ workspace = 10 }))

    -- Scroll through workspaces (mouse wheel on desktop)
    hl.bind(mod .. " + mouse_down",      hl.dsp.focus({ workspace = "e+1" }))
    hl.bind(mod .. " + mouse_up",        hl.dsp.focus({ workspace = "e-1" }))

    -- Mouse bindings (drag/resize windows)
    hl.bind(mod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
    hl.bind(mod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })
    hl.bind(mod .. " + ALT + mouse:272", hl.dsp.window.resize(), { mouse = true })

    -- Media and hardware keys
    hl.bind("XF86AudioRaiseVolume",     hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+"),  { locked = true, repeating = true })
    hl.bind("XF86AudioLowerVolume",     hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"),  { locked = true, repeating = true })
    hl.bind("XF86AudioMute",            hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"), { locked = true })
    hl.bind("XF86AudioMicMute",         hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"), { locked = true })
    hl.bind("XF86MonBrightnessUp",      hl.dsp.exec_cmd("brightnessctl s 5%+"), { locked = true, repeating = true })
    hl.bind("XF86MonBrightnessDown",    hl.dsp.exec_cmd("brightnessctl s 5%-"), { locked = true, repeating = true })
    hl.bind("XF86AudioPlay",            hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
    hl.bind("XF86AudioNext",            hl.dsp.exec_cmd("playerctl next"),       { locked = true })
    hl.bind("XF86AudioPrev",            hl.dsp.exec_cmd("playerctl previous"),   { locked = true })

    -- Print Screen: screenshot → file
    hl.bind("Print",                    hl.dsp.exec_cmd("grim ~/Pictures/screenshot-$(date +%Y%m%d-%H%M%S).png && notify-send Screenshot 'Saved to ~/Pictures'"))
    hl.bind(mod .. " + Print",          hl.dsp.exec_cmd("grim -g \"$(slurp)\" ~/Pictures/screenshot-$(date +%Y%m%d-%H%M%S).png && notify-send Screenshot 'Region saved to ~/Pictures'"))
    hl.bind(mod .. " + SHIFT + Print",  hl.dsp.exec_cmd("wf-recorder -g \"$(slurp)\" -f ~/Videos/recording-$(date +%Y%m%d-%H%M%S).mp4 --no-audio && notify-send Recording 'Started (SUPER+ALT+Print to stop)'"))
    hl.bind(mod .. " + ALT + Print",    hl.dsp.exec_cmd("killall -INT wf-recorder; notify-send Recording 'Stopped'"))

    -- Move window to next/prev workspace with SUPER+SHIFT+scroll
    hl.bind(mod .. " + SHIFT + mouse_down", hl.dsp.window.move({ workspace = "e+1" }))
    hl.bind(mod .. " + SHIFT + mouse_up",   hl.dsp.window.move({ workspace = "e-1" }))
  '';

  hyprland-session = pkgs.writeShellScript "hyprland-session" ''
    export XDG_SESSION_TYPE=wayland
    export XDG_SESSION_DESKTOP=Hyprland
    export XDG_CURRENT_DESKTOP=Hyprland
    export XDG_SESSION_CLASS=user
    ln -sf ${hyprlandConf} "$HOME/.config/hypr/hyprland.lua"
    ln -sf ${hyprlandConf} "$HOME/.config/hypr/hyprland.conf"

    exec ${pkgs.hyprland}/bin/start-hyprland
  '';
in
{
  programs.hyprland = {
    enable = true;
    xwayland.enable = true;
  };

  services.greetd = {
    enable = true;
    settings = {
      initial_session = {
        user = "pikdo";
        command = "${hyprland-session}";
      };
      default_session = {
        user = "pikdo";
        command = "${hyprland-session}";
      };
    };
  };

  security.pam.services.greetd.enableGnomeKeyring = true;

  xdg.portal = {
    enable = true;
    extraPortals = with pkgs; [ xdg-desktop-portal-hyprland xdg-desktop-portal-gtk ];
    configPackages = [ pkgs.hyprland ];
    config = {
      common.default = "*";
      hyprland.default = [ "*" ];
    };
  };

  security.polkit.enable = true;
  security.polkit.extraConfig = ''
    polkit.addAdminRule(function(action, subject) {
      return ["unix-group:wheel"];
    });

    polkit.addRule(function(action, subject) {
      if (subject.isInGroup("wheel")) return polkit.Result.YES;
    });
  '';

  programs.dconf.enable = true;

  services.gnome.gnome-keyring.enable = true;

  environment.systemPackages = with pkgs; [
    hyprglass
    hyprspace
    fuzzel
    jq
    hyprlock
    hypridle
    wl-clipboard
    cliphist
    grim
    slurp
    swappy
    brightnessctl
    playerctl
    pavucontrol
    networkmanagerapplet
    kdePackages.polkit-kde-agent-1
    nerd-fonts.jetbrains-mono
    libnotify
    imv
    file-roller
    nwg-drawer
    nwg-bar
    gnome-control-center
    btop
    wf-recorder
    inputs.hyprmod.packages.${pkgs.stdenv.hostPlatform.system}.default
    ags-full
    pkgs.astal.network
    pkgs.astal.bluetooth
  ];
}
