import App from "astal/gtk3/app"
import { Astal, Gtk, Gdk } from "astal/gtk3"
import Variable from "astal/variable"
import GLib from "gi://GLib?version=2.0"
import Gio from "gi://Gio?version=2.0"
import { execAsync } from "astal/process"
import Network from "gi://AstalNetwork"
import Bluetooth from "gi://AstalBluetooth"

const TOP = Astal.WindowAnchor.TOP
const OVERLAY = Astal.Layer.OVERLAY
const EXCLUSIVE = Astal.Exclusivity.EXCLUSIVE

const network = Network.get_default()
const bluetooth = Bluetooth.get_default()

function open(cmd: string) {
  GLib.spawn_command_line_async(cmd)
}

const clock = Variable("").poll(1000, () =>
  GLib.DateTime.new_now_local().format("%H:%M  %a %d/%b") ?? ""
)

const volume = Variable("").poll(2000, async () => {
  const out = await execAsync(["bash", "-c", "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -oP '\\d+\\.\\d+' | head -1"]).catch(() => "0")
  const muted = await execAsync(["bash", "-c", "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -c MUTED"]).catch(() => "0")
  const pct = Math.round(Math.min(parseFloat(out), 1.0) * 100) || 0
  return muted === "1" ? " M" : ` ${pct}`
})

const wifiIcon = Variable("").poll(3000, () => {
  if (!network.wifi) return ""
  if (!network.wifi.enabled) return ""
  return network.wifi.ssid ? "" : ""
})

const btIcon = Variable("").poll(3000, () => {
  if (!bluetooth.is_powered) return ""
  const connected = bluetooth.get_devices().some((d: any) => d.connected)
  return connected ? "" : ""
})

const battery = Variable("").poll(60000, async () => {
  const pct = await execAsync(["bash", "-c", "cat /sys/class/power_supply/BAT*/capacity 2>/dev/null | head -1"]).catch(() => "")
  return pct ? `${pct}%` : ""
})

const fullscreen = Variable(false).poll(2000, async () => {
  const out = await execAsync(["hyprctl", "activeworkspace", "-j"]).catch(() => '{"hasfullscreen":false}')
  return JSON.parse(out).hasfullscreen === true
})
const layout = Variable("").poll(5000, async () => {
  const out = await execAsync(["bash", "-c", "hyprctl devices -j"]).catch(() => "{}")
  try {
    const data = JSON.parse(out)
    const kb = data.keyboards?.find((k: any) => k.main)
    if (kb?.active_keymap) {
      const name = kb.active_keymap.toLowerCase()
      if (name.includes("english") || name.includes("us")) return "en"
      if (name.includes("spanish") || name.includes("latam") || name.includes("latin")) return "es"
      return name.slice(0, 2)
    }
  } catch {}
  return ""
})

function getKbName(): Promise<string> {
  return execAsync(["bash", "-c", "hyprctl devices -j"]).then(out => {
    const data = JSON.parse(out)
    const kb = data.keyboards?.find((k: any) => k.main)
    return kb?.name || ""
  }).catch(() => "")
}

function switchLayout(index: number) {
  getKbName().then(name => {
    if (name) {
      open(`bash -c 'hyprctl switchxkblayout "${name}" ${index}'`)
    }
  })
}

function changeVolume(delta: number) {
  execAsync(["bash", "-c", "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -oP '\\d+\\.\\d+' | head -1"]).then(out => {
    const current = parseFloat(out) || 0
    const target = Math.max(0, Math.min(1.0, current + delta / 100))
    execAsync(["bash", "-c", `wpctl set-volume @DEFAULT_AUDIO_SINK@ ${target}`])
  })
}

function sigBar(strength: number): string {
  if (strength > 80) return "▂▄▆█"
  if (strength > 60) return "▂▄▆ "
  if (strength > 40) return "▂▄  "
  if (strength > 20) return "▂   "
  return "    "
}

function secIcon(ap: any): string {
  return ap.requires_password ? "" : ""
}

function popupWindow(button: Gtk.Button, buildContent: (box: Gtk.Box, rebuild: () => void) => void) {
  let win: Gtk.Window | null = null

  button.connect("clicked", () => {
    if (win && win.get_visible()) {
      win.close()
      win.destroy()
      win = null
      return
    }

    win = new Gtk.Window({ type: Gtk.WindowType.POPUP })
    win.set_screen(button.get_screen())
    const visual = button.get_screen().get_rgba_visual()
    if (visual) win.set_visual(visual)
    win.set_app_paintable(true)
    win.get_style_context().add_class("popup-win")
    win.set_resizable(false)
    win.set_decorated(false)
    win.set_skip_taskbar_hint(true)
    win.set_skip_pager_hint(true)

    const box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 0 })
    box.get_style_context().add_class("popup-content")
    box.width_request = 270

    const rebuild = () => {
      box.get_children().forEach((c: any) => box.remove(c))
      buildContent(box, rebuild)
      box.show_all()
    }
    buildContent(box, rebuild)

    win.connect("key-press-event", (_w: any, e: any) => {
      if (e.keyval === Gdk.KEY_Escape) {
        win!.close(); win!.destroy(); win = null; return true
      }
      return false
    })

    GLib.timeout_add(GLib.PRIORITY_DEFAULT, 1500, () => {
      if (win && win.get_visible()) { rebuild(); return GLib.SOURCE_CONTINUE }
      return GLib.SOURCE_REMOVE
    })

    win.add(box)
    win.show_all()

    const screen = button.get_screen()
    const mon = screen.get_monitor_at_window(button.get_window())
    const geom = screen.get_monitor_geometry(mon)

    let [rx, ry] = [0, 0]
    if (button.get_window()) [rx, ry] = button.get_window().get_origin()
    const alloc = button.get_allocation()

    const x = rx + alloc.x
    const y = ry + alloc.y + alloc.height + 2

    const [_minW, natW] = win.get_preferred_width()
    const [_minH, natH] = win.get_preferred_height()
    const pw = Math.min(natW > 0 ? natW : 270, geom.width - 20)
    const maxH = geom.y + geom.height - y - 10
    const ph = Math.min(natH > 0 ? natH : 400, maxH)

    win.move(Math.round(x), Math.round(y))
    win.resize(Math.round(pw), Math.round(ph))
  })
}

function buildWifiContent(box: Gtk.Box, rebuild: () => void) {
  const wifi = network.wifi

  const hdrBox = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL, spacing: 8 })
  hdrBox.margin_start = 8; hdrBox.margin_end = 8; hdrBox.margin_top = 6; hdrBox.margin_bottom = 6
  const hdrLbl = new Gtk.Label({ label: "Wi-Fi", halign: Gtk.Align.START })
  hdrLbl.get_style_context().add_class("popover-title")
  hdrBox.pack_start(hdrLbl, true, true, 0)
  if (wifi) {
    const toggle = new Gtk.Switch()
    toggle.active = wifi.enabled
    toggle.connect("notify::active", () => { wifi.enabled = toggle.active })
    wifi.connect("notify::enabled", () => { toggle.active = wifi.enabled })
    hdrBox.pack_end(toggle, false, false, 0)
  }
  box.pack_start(hdrBox, false, false, 0)
  box.pack_start(new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL }), false, false, 0)

  if (!wifi) {
    const msg = new Gtk.Label({ label: "No Wi-Fi adapter" })
    msg.margin_start = 12; msg.margin_end = 12; msg.margin_top = 10; msg.margin_bottom = 10
    msg.get_style_context().add_class("dim-label")
    box.pack_start(msg, false, false, 0)
    return
  }
  if (!wifi.enabled) {
    const msg = new Gtk.Label({ label: "Wi-Fi is disabled" })
    msg.margin_start = 12; msg.margin_end = 12; msg.margin_top = 10; msg.margin_bottom = 10
    msg.get_style_context().add_class("dim-label")
    box.pack_start(msg, false, false, 0)
    return
  }

  const ssid = wifi.ssid
  if (ssid) {
    const connRow = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL, spacing: 8 })
    connRow.margin_start = 8; connRow.margin_end = 8; connRow.margin_top = 4; connRow.margin_bottom = 4
    const icon = new Gtk.Label({ label: ` ${sigBar(wifi.strength)}` })
    const name = new Gtk.Label({ label: ssid, halign: Gtk.Align.START })
    name.get_style_context().add_class("connected-label")
    name.set_ellipsize(3); name.set_max_width_chars(20)
    connRow.pack_start(icon, false, false, 0)
    connRow.pack_start(name, true, true, 0)
    const discBtn = new Gtk.Button({ label: "✕" })
    discBtn.get_style_context().add_class("mini-btn")
    discBtn.connect("clicked", () => { wifi.deactivate_connection() })
    connRow.pack_end(discBtn, false, false, 0)
    box.pack_start(connRow, false, false, 0)
    box.pack_start(new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL }), false, false, 0)
  }

  const scanRow = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL, spacing: 0 })
  scanRow.margin_start = 6; scanRow.margin_end = 6; scanRow.margin_top = 4; scanRow.margin_bottom = 4
  const scanLbl = new Gtk.Label({ label: "⟳  Scan", halign: Gtk.Align.START })
  const scanBtn = new Gtk.Button({ child: scanLbl })
  scanBtn.get_style_context().add_class("scan-btn")
  scanBtn.connect("clicked", () => {
    wifi.scan()
    scanLbl.label = "⟳  Scanning..."
    GLib.timeout_add(GLib.PRIORITY_DEFAULT, 3000, () => {
      scanLbl.label = "⟳  Scan"
      return GLib.SOURCE_REMOVE
    })
  })
  scanRow.pack_start(scanBtn, false, false, 0)
  box.pack_start(scanRow, false, false, 0)

  const aps = wifi.get_access_points()
  if (aps.length === 0) {
    const msg = new Gtk.Label({ label: "No networks found" })
    msg.margin_start = 12; msg.margin_end = 12; msg.margin_top = 10; msg.margin_bottom = 10
    msg.get_style_context().add_class("dim-label")
    box.pack_start(msg, false, false, 0)
  } else {
    const sorted = [...aps].sort((a: any, b: any) => b.strength - a.strength).slice(0, 10)
    const apList = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 0 })
    for (const ap of sorted) {
      const apSsid = ap.ssid || "<hidden>"
      const isActive = ssid === apSsid
      const row = new Gtk.Button()
      row.get_style_context().add_class("ap-row")
      const content = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL, spacing: 8 })
      content.margin_start = 10; content.margin_end = 10; content.margin_top = 4; content.margin_bottom = 4
      const info = new Gtk.Label({ label: `${sigBar(ap.strength)} ${secIcon(ap)}` })
      const apName = new Gtk.Label({ label: apSsid, halign: Gtk.Align.START })
      apName.set_ellipsize(3); apName.set_max_width_chars(18)
      const check = new Gtk.Label({ label: isActive ? "✓" : "" })
      check.get_style_context().add_class("active-mark")
      content.pack_start(info, false, false, 0)
      content.pack_start(apName, true, true, 0)
      content.pack_end(check, false, false, 0)
      row.add(content)
      if (isActive) row.sensitive = false
      else row.connect("clicked", () => {
        ap.activate(null, (_s: any, r: any) => { ap.activate_finish(r) })
      })
      apList.pack_start(row, false, false, 0)
    }
    const scroll = new Gtk.ScrolledWindow({})
    scroll.min_content_height = Math.min(sorted.length * 34, 240)
    scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
    scroll.add(apList)
    box.pack_start(scroll, true, true, 0)
  }
}

function buildBtContent(box: Gtk.Box, rebuild: () => void) {
  const hdrBox = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL, spacing: 8 })
  hdrBox.margin_start = 8; hdrBox.margin_end = 8; hdrBox.margin_top = 6; hdrBox.margin_bottom = 6
  const hdrLbl = new Gtk.Label({ label: "Bluetooth", halign: Gtk.Align.START })
  hdrLbl.get_style_context().add_class("popover-title")
  hdrBox.pack_start(hdrLbl, true, true, 0)

  const toggle = new Gtk.Switch()
  toggle.active = bluetooth.is_powered
  toggle.connect("notify::active", () => {
    if (toggle.active !== bluetooth.is_powered) bluetooth.toggle()
  })
  bluetooth.connect("notify::is-powered", () => { toggle.active = bluetooth.is_powered })
  hdrBox.pack_end(toggle, false, false, 0)
  box.pack_start(hdrBox, false, false, 0)
  box.pack_start(new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL }), false, false, 0)

  if (!bluetooth.is_powered) {
    const msg = new Gtk.Label({ label: "Bluetooth is off" })
    msg.margin_start = 12; msg.margin_end = 12; msg.margin_top = 10; msg.margin_bottom = 10
    msg.get_style_context().add_class("dim-label")
    box.pack_start(msg, false, false, 0)
    return
  }

  const devices = bluetooth.get_devices()
  if (devices.length === 0) {
    const msg = new Gtk.Label({ label: "No devices found" })
    msg.margin_start = 12; msg.margin_end = 12; msg.margin_top = 10; msg.margin_bottom = 10
    msg.get_style_context().add_class("dim-label")
    box.pack_start(msg, false, false, 0)
  } else {
    const devList = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 0 })
    for (const dev of devices) {
      const devName = dev.alias || dev.name || dev.address
      const batt = dev.battery_percentage
      const devIcon = dev.icon || ""
      let status = ""
      if (dev.connected) status = "  ✓"
      else if (dev.connecting) status = "  ⟳"
      let extra = ""
      if (batt >= 0) extra = `  ${batt}%`
      const label = `${devIcon}  ${devName}${extra}${status}`
      const row = new Gtk.Button({ label })
      row.get_style_context().add_class("dev-row")
      row.set_halign(Gtk.Align.FILL)
      row.connect("clicked", () => {
        if (dev.connected) dev.disconnect_device()
        else {
          if (!dev.paired) dev.pair()
          dev.connect_device()
        }
      })
      devList.pack_start(row, false, false, 0)
    }
    const scroll = new Gtk.ScrolledWindow({})
    scroll.min_content_height = Math.min(devices.length * 34, 240)
    scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
    scroll.add(devList)
    box.pack_start(scroll, true, true, 0)
  }
}

function listenToHyprland() {
  const sig = GLib.getenv("HYPRLAND_INSTANCE_SIGNATURE")
  if (!sig) return

  const sockPath = `/tmp/hypr/${sig}/.socket2.sock`

  try {
    const client = new Gio.SocketClient()
    const address = new Gio.UnixSocketAddress({ path: sockPath })
    const conn = client.connect(address, null)
    const stream = conn.get_input_stream()
    const sock = conn.get_socket()
    const fd = sock.get_fd()

    let buf = ""
    GLib.io_add_watch(fd, GLib.PRIORITY_DEFAULT, GLib.IOCondition.IN, () => {
      try {
        const [ok, data] = stream.read_bytes(4096, null)
        if (!ok || !data) return GLib.SOURCE_CONTINUE

        const raw = data.get_data()
        for (let i = 0; i < raw.length; i++) buf += String.fromCharCode(raw[i])

        const lines = buf.split("\n")
        buf = lines.pop() || ""

        for (const line of lines) {
          if (line.startsWith("fullscreen>>")) {
            fullscreen.set(line.slice("fullscreen>>".length) === "1")
          }
        }
      } catch {
        return GLib.SOURCE_REMOVE
      }
      return GLib.SOURCE_CONTINUE
    })
  } catch (e) {
    console.error("Hyprland socket:", e)
  }
}

listenToHyprland()

App.start({
  css: `
    window {
      background: transparent;
    }
    .island-pill {
      background-color: #000000;
      color: #cdd6f4;
      border: 1px solid rgba(137, 180, 250, 0.2);
      border-radius: 9999px;
      padding: 0;
      margin-top: 2px;
      min-height: 18px;
      font-family: "JetBrainsMono Nerd Font";
      font-size: 11px;
    }
    .sep {
      color: rgba(137, 180, 250, 0.15);
      font-size: 15px;
      margin: 0 2px;
    }
    .island-btn {
      background: transparent;
      border: none;
      padding: 0 6px;
      min-width: 0;
    }
    .island-btn:hover {
      background: rgba(137, 180, 250, 0.15);
      border-radius: 9999px;
    }
    menu {
      background: rgba(24, 24, 37, 0.95);
      border: 1px solid rgba(137, 180, 250, 0.3);
      border-radius: 12px;
      padding: 4px;
      -gtk-outline-top-left-radius: 12px;
      -gtk-outline-top-right-radius: 12px;
      -gtk-outline-bottom-left-radius: 12px;
      -gtk-outline-bottom-right-radius: 12px;
    }
    menuitem {
      background: transparent;
      border: none;
      color: #cdd6f4;
      padding: 6px 14px;
      border-radius: 8px;
      font-family: "JetBrainsMono Nerd Font";
      font-size: 13px;
      min-width: 180px;
    }
    menuitem:hover {
      background: rgba(137, 180, 250, 0.15);
      color: #89b4fa;
    }
    .popover-title {
      font-weight: bold;
      font-size: 14px;
      color: #cdd6f4;
    }
    .connected-label {
      color: #89b4fa;
      font-weight: bold;
    }
    .popup-win {
      background: transparent;
      background-image: none;
      border: none;
      box-shadow: none;
    }
    .popup-content {
      background: rgba(24, 24, 37, 0.97);
      border: 1px solid rgba(137, 180, 250, 0.25);
      border-radius: 14px;
      padding: 0;
    }
    .dim-label {
      color: #585b70;
    }
    .mini-btn {
      background: transparent;
      border: none;
      padding: 0 6px;
      min-width: 0;
      color: #f38ba8;
    }
    .mini-btn:hover {
      background: rgba(243, 139, 168, 0.15);
      border-radius: 9999px;
    }
    .scan-btn {
      background: rgba(137, 180, 250, 0.1);
      border: 1px solid rgba(137, 180, 250, 0.2);
      border-radius: 8px;
      padding: 4px 12px;
      color: #89b4fa;
    }
    .scan-btn:hover {
      background: rgba(137, 180, 250, 0.2);
    }
    .ap-row {
      background: transparent;
      border: none;
      border-radius: 0;
      padding: 0;
      min-width: 0;
    }
    .ap-row:hover {
      background: rgba(137, 180, 250, 0.1);
    }
    .dev-row {
      background: transparent;
      border: none;
      border-radius: 0;
      padding: 6px 12px;
      min-width: 0;
      color: #cdd6f4;
      font-family: "JetBrainsMono Nerd Font";
      font-size: 13px;
    }
    .dev-row:hover {
      background: rgba(137, 180, 250, 0.1);
    }
    .active-mark {
      color: #a6e3a1;
      font-weight: bold;
    }
  `,
  main() {
    return <window
      name="island"
      namespace="island"
      layer={OVERLAY}
      exclusivity={EXCLUSIVE}
      keymode={Astal.Keymode.NONE}
      anchor={TOP}
      application={App}
      visible
      setup={self => {
        const screen = self.get_screen()
        const visual = screen.get_rgba_visual()
        if (visual) self.set_visual(visual)
      }}
    >
      <box halign={Gtk.Align.CENTER} valign={Gtk.Align.START}>
        <revealer
          revealChild={fullscreen().as(fs => !fs)}
          transitionType={Gtk.RevealerTransitionType.SLIDE_DOWN}
          transitionDuration={250}
          valign={Gtk.Align.START}
        >
        <box className="island-pill" halign={Gtk.Align.CENTER} spacing={0}>
          <button
            className="island-btn"
            setup={self => { popupWindow(self, buildWifiContent) }}
          >
            <label label={wifiIcon()} />
          </button>
          <button
            className="island-btn"
            setup={self => { popupWindow(self, buildBtContent) }}
          >
            <label label={btIcon()} />
          </button>
          <label className="sep" label="|" />
          <button
            className="island-btn"
            onClicked={() => open("pavucontrol")}
            setup={self => {
              self.add_events(Gdk.EventMask.SMOOTH_SCROLL_MASK | Gdk.EventMask.SCROLL_MASK)
              self.connect("scroll-event", (_w, event: Gdk.EventScroll) => {
                const dir = event.direction
                if (dir === Gdk.ScrollDirection.UP) {
                  changeVolume(5)
                } else if (dir === Gdk.ScrollDirection.DOWN) {
                  changeVolume(-5)
                }
              })
            }}
          >
            <label label={volume()} />
          </button>
          <label className="sep" label="|" />
          <label label={clock()} />
          <label className="sep" label="|" />
          <button className="island-btn" onClicked={() => open("gnome-control-center power")}>
            <label label={battery()} />
          </button>
          <label className="sep" label="|" />
          <button
            className="island-btn"
            setup={self => {
              const menu = new Gtk.Menu()
              const enItem = new Gtk.MenuItem({ label: "EN  English (US)" })
              enItem.connect("activate", () => {
                switchLayout(0)
                layout.set("en")
              })
              menu.append(enItem)
              const esItem = new Gtk.MenuItem({ label: "ES  Español (Latinoamérica)" })
              esItem.connect("activate", () => {
                switchLayout(1)
                layout.set("es")
              })
              menu.append(esItem)
              menu.show_all()
              self.connect("clicked", () => {
                const south = Gdk.Gravity ? Gdk.Gravity.SOUTH : 8
                const north = Gdk.Gravity ? Gdk.Gravity.NORTH : 2
                menu.popup_at_widget(self, south, north, null)
              })
            }}
          >
            <label label={layout()} />
          </button>
          <label className="sep" label="|" />
          <button className="island-btn" onClicked={() => open("nwg-bar")}>
            <label label="" />
          </button>
        </box>
        </revealer>
      </box>
    </window>
  },
})
