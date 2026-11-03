{ config, lib, pkgs, inputs, ... }:

let
  hyprglass = pkgs.callPackage ../pkgs/hyprglass.nix { mkHyprlandPlugin = pkgs.hyprlandPlugins.mkHyprlandPlugin; };
  cascade-overview = pkgs.callPackage ../pkgs/cascade-overview.nix { };
  polkitAgent = "${pkgs.kdePackages.polkit-kde-agent-1}/libexec/polkit-kde-authentication-agent-1";
  agsPackages = inputs.ags.packages.${pkgs.stdenv.hostPlatform.system};
  # AGS 2 needs the GJS bindings that Astal no longer exposes as a flake package.
  astalGjs = import "${inputs.astal}/lang/gjs" {
    inherit pkgs;
    self = inputs.astal;
  };
  ags-full = agsPackages.default.override {
    wrapGAppsHook = pkgs.wrapGAppsHook3;
    astal-gjs = "${astalGjs}/share/astal/gjs";
    extraPackages = [
      agsPackages.network agsPackages.bluetooth agsPackages.tray
      agsPackages.wireplumber agsPackages.battery agsPackages.hyprland
      pkgs.networkmanager
    ];
  };
  display-switcher = pkgs.writeShellScript "display-switcher" ''
    H=${pkgs.hyprland}/bin/hyprctl
    JQ=${pkgs.jq}/bin/jq

    INTERNAL=$($H monitors all -j | $JQ -r '[.[] | select(.name | startswith("eDP"))] | first | .name // empty')
    EXTERNAL=$($H monitors all -j | $JQ -r '[.[] | select(.name | startswith("eDP") | not)] | first | .name // empty')

    if [ -z "$EXTERNAL" ]; then
      ${pkgs.libnotify}/bin/notify-send 'Pantalla' 'No hay monitor externo conectado'
      exit 0
    fi

    scale_of() {
      $H monitors all -j | $JQ -r --arg m "$1" '[.[] | select(.name == $m)] | first | .scale // 1'
    }

    setmon() {
      $H eval "hl.monitor({ output = \"$1\", $2 })" >/dev/null
    }

    SI=$(scale_of "$INTERNAL"); SE=$(scale_of "$EXTERNAL")

    enable_mon() {
      setmon "$1" "mirror = \"none\", disabled = false, mode = \"preferred\", position = \"$2\", scale = $(scale_of "$1")"
      sleep 0.4
    }

    is_active() {
      $H monitors -j | $JQ -e --arg m "$1" '.[] | select(.name == $m)' >/dev/null
    }

    # Disable $1 only after confirming keeper $2 is rendering; if the disable
    # ever leaves zero active outputs, bring the keeper back immediately.
    safe_disable() {
      if ! is_active "$2"; then
        enable_mon "$2" "auto"
      fi
      setmon "$1" "disabled = true"
      sleep 0.3
      if [ "$($H monitors -j | $JQ 'length')" -eq 0 ]; then
        enable_mon "$2" "auto"
      fi
    }

    CHOICE=$(printf 'Extender pantalla\nProyectar pantalla\nSolo laptop\nSolo monitor externo' \
      | ${pkgs.fuzzel}/bin/fuzzel -d --with-nth=1 --only-match --prompt="Pantalla: ") || exit 0

    case "$CHOICE" in
      "Extender pantalla")
        $H eval 'hl.config({ cursor = { no_hardware_cursors = false } })' >/dev/null
        enable_mon "$INTERNAL" "auto"
        enable_mon "$EXTERNAL" "auto-right"
        ;;
      "Proyectar pantalla")
        $H eval 'hl.config({ cursor = { no_hardware_cursors = true } })' >/dev/null
        $H eval "hl.monitor({ output = \"$INTERNAL\", mode = \"preferred\", position = \"auto\", scale = $SI }); hl.monitor({ output = \"$EXTERNAL\", mirror = \"none\" }); hl.monitor({ output = \"$EXTERNAL\", disabled = false, mode = \"preferred\", position = \"auto\", scale = $SE, mirror = \"$INTERNAL\" })" >/dev/null
        ;;
      "Solo laptop")
        $H eval 'hl.config({ cursor = { no_hardware_cursors = false } })' >/dev/null
        enable_mon "$INTERNAL" "auto"
        safe_disable "$EXTERNAL" "$INTERNAL"
        ;;
      "Solo monitor externo")
        $H eval 'hl.config({ cursor = { no_hardware_cursors = false } })' >/dev/null
        enable_mon "$EXTERNAL" "auto"
        safe_disable "$INTERNAL" "$EXTERNAL"
        ;;
      *)
        exit 0
        ;;
    esac

    # Failsafe: never leave the user without any active output.
    if [ "$($H monitors -j | $JQ 'length')" -eq 0 ]; then
      enable_mon "$INTERNAL" "auto"
    fi

    # Re-sync wallpaper daemon: awww misses outputs that exist when it starts,
    # but registers hotplugged ones correctly. Bounce any monitor it does not
    # know about, then repaint the wallpaper everywhere.
    sleep 0.3
    AWWW=${pkgs.awww}/bin/awww
    ACTIVE=$($H monitors -j | $JQ -r '.[].name' | sort)
    KNOWN=$($AWWW query 2>/dev/null | awk -F': ' '/^: /{print $2}' | awk -F: '{print $1}' | sed 's/ *$//' | sort)
    MISSING=$(comm -23 <(echo "$ACTIVE") <(echo "$KNOWN") | grep -v '^$')

    for m in $MISSING; do
      MS=$(scale_of "$m")
      $H eval "hl.monitor({ output = \"$m\", disabled = true })" >/dev/null
      sleep 0.15
      $H eval "hl.monitor({ output = \"$m\", disabled = false, mode = \"preferred\", position = \"auto\", scale = $MS })" >/dev/null
      sleep 0.5
    done

    WP="$HOME/.cache/current-wallpaper"
    if [ -e "$WP" ]; then
      $AWWW img "$WP" --transition-type none >/dev/null 2>&1
    fi
    ${pkgs.libnotify}/bin/notify-send 'Pantalla' "$CHOICE"
  '';

  audio-switcher = pkgs.writeShellScript "audio-switcher" ''
    JQ=${pkgs.jq}/bin/jq
    WPCTL=${pkgs.wireplumber}/bin/wpctl
    CARD="alsa_card.pci-0000_00_1f.3-platform-skl_hda_dsp_generic"

    WPID=$($WPCTL status 2>/dev/null | grep '\[alsa\]' | head -1 | sed 's/[^0-9]*\([0-9]*\)\..*/\1/')
    [ -z "$WPID" ] && exit 0

    TMPF=$(mktemp /tmp/opencode/asw.XXXXXX)
    trap 'rm -f "$TMPF"' EXIT
    pw-dump > "$TMPF" 2>/dev/null

    # Output routes (ports) of the analog card, e.g. Speaker / Headphones
    MAP=$($JQ -r --arg c "$CARD" '[.[] | select(.info.props."device.name"? == $c)][0].info.params.EnumRoute[]? | select(.direction == "Output") | "\(.index)|\(.description)|\(.devices[0])"' "$TMPF")
    CUR=$($JQ -r --arg c "$CARD" '[.[] | select(.info.props."device.name"? == $c)][0].info.params.Route[]? | select(.direction == "Output") | .description // empty' "$TMPF")

    MENU=""
    while IFS='|' read -r IDX DESC DEV; do
      [ -z "$IDX" ] && continue
      MARK=""
      [ "$DESC" = "$CUR" ] && MARK=" [actual]"
      MENU+="''${DESC}''${MARK}\n"
    done <<EOF
$MAP
EOF

    CHOICE=$(printf "$MENU" | ${pkgs.fuzzel}/bin/fuzzel -d --with-nth=1 --only-match --prompt="Salida: ") || exit 0
    CHOICE="''${CHOICE% \[actual\]}"

    LINE=$(echo "$MAP" | grep -F "|$CHOICE|" | head -1)
    IDX=$(echo "$LINE" | cut -d'|' -f1)
    DEV=$(echo "$LINE" | cut -d'|' -f3)
    case "$IDX" in
      *[!0-9]*|"") ${pkgs.libnotify}/bin/notify-send 'Audio' 'Ruta no encontrada'; exit 1;;
    esac

    ${pkgs.pipewire}/bin/pw-cli s "$WPID" Route "{ index = $IDX, device = $DEV, save = true }" >/dev/null 2>&1
    ${pkgs.libnotify}/bin/notify-send 'Audio' "Salida: $CHOICE"
  '';

  display-rescue = pkgs.writeShellScript "display-rescue" ''
    H=${pkgs.hyprland}/bin/hyprctl
    JQ=${pkgs.jq}/bin/jq
    $H eval 'hl.config({ cursor = { no_hardware_cursors = false } })' >/dev/null
    for m in $($H monitors all -j | $JQ -r '.[].name'); do
      S=$($H monitors all -j | $JQ -r --arg m "$m" '[.[] | select(.name == $m)] | first | .scale // 1')
      $H eval "hl.monitor({ output = \"$m\", mirror = \"none\", disabled = false, mode = \"preferred\", position = \"auto\", scale = $S })" >/dev/null
      sleep 0.2
    done
  '';

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

    local function cascade(action, argument)
        if hl.plugin.cascade then
            hl.plugin.cascade[action](argument or "")
        end
    end

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
            workspace_swipe_distance = 200,
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
            -- Windows-style resize: floating windows can be resized by
            -- dragging any border or corner without a modifier key.
            resize_on_border = true,
            extend_border_grab_area = 8,
            hover_icon_on_border = true,
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
                range       = 4,
                render_power = 2,
            },
        },
        misc = {
            disable_hyprland_logo = true,
            disable_splash_rendering = true,
            animate_manual_resizes = false,
            animate_mouse_windowdragging = false,
        },
        render = {
            direct_scanout = true,
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

    -- Cascade captures three-finger navigation and four-finger overview gestures.

    hl.curve("easeOutQuint",     { type = "bezier", points = { {0.23, 1}, {0.32, 1} } })
    hl.curve("easeInOutCubic",   { type = "bezier", points = { {0.65, 0}, {0.35, 1} } })
    hl.animation({ leaf = "windows",     enabled = true, speed = 4, bezier = "easeOutQuint",   style = "popin" })
    hl.animation({ leaf = "windowsOut",  enabled = false })
    hl.animation({ leaf = "fade",        enabled = true, speed = 4, bezier = "easeOutQuint" })
    hl.animation({ leaf = "workspaces",  enabled = true, speed = 6, bezier = "easeOutQuint", style = "slide" })
    hl.animation({ leaf = "border",      enabled = false })
    hl.animation({ leaf = "layersIn",    enabled = true, speed = 3, bezier = "easeOutQuint", style = "fade" })
    hl.animation({ leaf = "layersOut",   enabled = true, speed = 2, bezier = "easeInOutCubic", style = "fade" })
    hl.layer_rule({ match = { namespace = "^notifications$" }, animation = "slide right" })

    hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
    hl.env("XDG_SESSION_TYPE",    "wayland")
    hl.env("XDG_SESSION_DESKTOP", "Hyprland")
    hl.env("GTK_THEME",           "Adwaita-dark")
    hl.env("GDK_DPI_SCALE",       "1")
    hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")
    hl.env("HYPRLAND_NO_WARNINGS", "1")


    -- Cascade registers these options when its plugin is loaded. The guard
    -- keeps the first config parse valid and applies them on the plugin reload.
    if hl.plugin.cascade then
        hl.config({
            plugin = {
                cascade = {
                    animate = true,
                    gestures = true,
                    reverseSwipe = false,
                    cardWidth = 480,
                    rowHeight = 340,
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
        hl.exec_cmd("systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_SESSION_DESKTOP")
        hl.exec_cmd("dbus-update-activation-environment --all")
        hl.exec_cmd("systemctl --user start hyprland-session-bridge.service")
        hl.exec_cmd("systemctl --user start xdg-desktop-portal-hyprland.service")
        -- HyprGlass registers its Lua API only after loading. Reload once both
        -- plugins are present so the guarded configuration above is applied.
        hl.exec_cmd("hyprctl plugin load ${hyprglass}/lib/hyprglass.so && hyprctl plugin load ${cascade-overview}/lib/libHyprspace.so && hyprctl reload")
        hl.exec_cmd("systemctl --user start awww.service")
        hl.exec_cmd("systemctl --user start wallpaper-cycle.timer")
        hl.exec_cmd("systemctl --user start wallpaper-cycle.service")
        hl.exec_cmd("hypridle")
        hl.exec_cmd("${polkitAgent}")
        hl.exec_cmd("systemctl --user start island.service")
    end)

    -- Window rules
    -- Keep Foot opt-in and pin its appearance to HyprGlass' built-in glass
    -- preset, rather than allowing the plugin default to select another look.
    -- Do not use a fullscreen `hyprglass_disabled` tag: Hyprland can retain
    -- that dynamic tag after leaving fullscreen, and HyprGlass gives it
    -- priority over `hyprglass_enabled`.
    hl.window_rule({ match = { class = "^(foot|footclient)$" }, opacity = 0.77, tag = "+hyprglass_enabled" })
    hl.window_rule({ match = { class = "^(foot|footclient)$" }, tag = "+hyprglass_preset_glass" })
    hl.window_rule({ match = { class = "mpv" },       tag = "+hyprglass_disabled" })
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
    hl.bind(mod .. " + Tab",             function() cascade("toggle") end)
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

    -- Display switcher (like Win+P). Plain F1 on this HP emits
    -- KEY_SWITCHVIDEOMODE (evdev 227 -> XKB code:235); Fn+F1 would emit F1.
    hl.bind("code:235",                  hl.dsp.exec_cmd("${display-switcher}"))
    hl.bind(mod .. " + SHIFT + P",       hl.dsp.exec_cmd("${display-switcher}"))
    -- Display rescue: re-enable every monitor (works with all screens off)
    hl.bind(mod .. " + SHIFT + M",       hl.dsp.exec_cmd("${display-rescue}"))
    -- Audio output switcher (Speaker / Headphones)
    hl.bind(mod .. " + SHIFT + A",       hl.dsp.exec_cmd("${audio-switcher}"))

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

    -- Desktop ordinals belong to the selected group; IDs are allocated by Cascade.
    for ordinal = 1, 10 do
        local key = ordinal % 10
        hl.bind(mod .. " + " .. key, function()
            cascade("command", "desktop-select " .. ordinal)
        end)
        hl.bind(mod .. " + SHIFT + " .. key, function()
            cascade("command", "window-to " .. ordinal)
        end)
    end

    for _, direction in ipairs({ "left", "right", "up", "down" }) do
        hl.bind(mod .. " + CTRL + " .. direction, function()
            cascade("navigate", direction)
        end)
        hl.bind(mod .. " + CTRL + SHIFT + " .. direction, function()
            cascade("navigate", "move-" .. direction)
        end)
    end

    hl.bind(mod .. " + mouse_down", function() cascade("navigate", "right") end)
    hl.bind(mod .. " + mouse_up",   function() cascade("navigate", "left") end)

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

    hl.bind(mod .. " + SHIFT + mouse_down", function() cascade("navigate", "move-right") end)
    hl.bind(mod .. " + SHIFT + mouse_up",   function() cascade("navigate", "move-left") end)
  '';

  hyprland-session = pkgs.writeShellScript "hyprland-session" ''
    export XDG_SESSION_TYPE=wayland
    export XDG_SESSION_DESKTOP=Hyprland
    export XDG_CURRENT_DESKTOP=Hyprland
    export XDG_SESSION_CLASS=user
    ln -sf ${hyprlandConf} "$HOME/.config/hypr/hyprland.lua"
    ln -sf ${hyprlandConf} "$HOME/.config/hypr/hyprland.conf"

    systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_SESSION_DESKTOP
    dbus-update-activation-environment --all

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

  # Rebuilding the session script must not terminate the running desktop.
  systemd.services.greetd.restartIfChanged = false;

  security.pam.services.greetd.enableGnomeKeyring = true;

  xdg.portal = {
    enable = true;
    extraPortals = with pkgs; [ xdg-desktop-portal-gtk ];
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
    cascade-overview
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
    awww
    inputs.hyprmod.packages.${pkgs.stdenv.hostPlatform.system}.default
    ags-full
    agsPackages.network
    agsPackages.bluetooth
    agsPackages.tray
  ];

  programs.thunar = {
    enable = true;
    plugins = with pkgs; [
      thunar-archive-plugin
      thunar-volman
    ];
  };

  systemd.user.services.island = {
    description = "Floating island";
    wantedBy = [ "graphical-session.target" ];
    after = [ "graphical-session.target" ];
    partOf = [ "graphical-session.target" ];
    path = with pkgs; [
      dunst pavucontrol blueman gnome-control-center hyprlock hyprland
      systemd networkmanagerapplet geteduroam
    ];
    serviceConfig = {
      ExecStart = "${ags-full}/bin/ags run";
      Restart = "on-failure";
      RestartSec = 2;
    };
  };

  systemd.user.services.hyprland-session-bridge = {
    description = "Activate graphical-session.target for Hyprland";
    serviceConfig = {
      Type = "oneshot";
      RemainAfterExit = true;
      ExecStart = "${pkgs.coreutils}/bin/true";
    };
    requires = [ "graphical-session.target" ];
    after = [ "graphical-session.target" ];
  };
}
