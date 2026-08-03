import App from "astal/gtk3/app"
import { Astal, Gtk, Gdk } from "astal/gtk3"
import Variable from "astal/variable"
import GLib from "gi://GLib?version=2.0"
import Gio from "gi://Gio?version=2.0"
import { execAsync } from "astal/process"

const TOP = Astal.WindowAnchor.TOP
const OVERLAY = Astal.Layer.OVERLAY
const IGNORE = Astal.Exclusivity.IGNORE

function open(cmd: string) {
  GLib.spawn_command_line_async(cmd)
}

const clock = Variable("").poll(1000, () =>
  GLib.DateTime.new_now_local().format("%H:%M  %a %d/%b") ?? ""
)

const volume = Variable("").poll(500, async () => {
  const out = await execAsync(["bash", "-c", "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -oP '\\d+\\.\\d+' | head -1"]).catch(() => "0")
  const muted = await execAsync(["bash", "-c", "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -c MUTED"]).catch(() => "0")
  const pct = Math.round(Math.min(parseFloat(out), 1.0) * 100) || 0
  return muted === "1" ? " M" : ` ${pct}`
})

const wifi = Variable("").poll(60000, async () => {
  const state = await execAsync(["bash", "-c", "nmcli -t -f TYPE,STATE device | grep -c '^wifi:connected$'"]).catch(() => "0")
  return state === "1" ? "" : ""
})

const bt = Variable("").poll(30000, async () => {
  const connected = await execAsync(["bash", "-c", "bluetoothctl devices Connected | wc -l"]).catch(() => "0")
  return parseInt(connected) > 0 ? "" : ""
})

const battery = Variable("").poll(60000, async () => {
  const pct = await execAsync(["bash", "-c", "cat /sys/class/power_supply/BAT*/capacity 2>/dev/null | head -1"]).catch(() => "")
  return pct ? `${pct}%` : ""
})

const activeWs = Variable(1).poll(1000, async () => {
  const out = await execAsync(["hyprctl", "activeworkspace", "-j"]).catch(() => '{"id":1}')
  return JSON.parse(out).id
})
const workspaces = Variable<number[]>([]).poll(5000, async () => {
  const out = await execAsync(["hyprctl", "workspaces", "-j"]).catch(() => "[]")
  return JSON.parse(out).map((w: any) => w.id).filter((id: number) => id > 0).sort()
})
const fullscreen = Variable(false).poll(500, async () => {
  const out = await execAsync(["hyprctl", "activeworkspace", "-j"]).catch(() => '{"hasfullscreen":false}')
  return JSON.parse(out).hasfullscreen === true
})

const layout = Variable("").poll(2000, async () => {
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

function switchWs(id: number) {
  activeWs.set(id)
  open(`bash -c 'hyprctl dispatch "hl.dsp.focus({workspace = ${id}})"'`)
}

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

execAsync(["hyprctl", "workspaces", "-j"])
  .then(out => {
    const ids = JSON.parse(out).map((w: any) => w.id).filter((id: number) => id > 0).sort()
    workspaces.set(ids)
  })
  .catch(() => {})

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
          if (line.startsWith("workspace>>")) {
            activeWs.set(parseInt(line.slice("workspace>>".length)))
          } else if (line.startsWith("fullscreen>>")) {
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
      background-color: rgba(24, 24, 37, 0.92);
      color: #cdd6f4;
      border: 1px solid rgba(137, 180, 250, 0.2);
      border-radius: 9999px;
      padding: 0 6px;
      margin-top: 6px;
      min-height: 28px;
      font-family: "JetBrainsMono Nerd Font";
      font-size: 13px;
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
    .ws-btn {
      background: transparent;
      border: none;
      padding: 0 5px;
      color: #585b70;
      font-weight: bold;
      font-size: 12px;
      min-width: 0;
    }
    .ws-btn:hover {
      color: #89b4fa;
    }
    .ws-btn.active {
      color: #89b4fa;
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
    .background.popup {
      background-color: transparent;
      background-image: none;
      border: none;
      border-image: none;
      box-shadow: none;
      margin: 0;
      padding: 0;
    }
  `,
  main() {
    return <window
      name="island"
      namespace="island"
      layer={OVERLAY}
      exclusivity={IGNORE}
      keymode={Astal.Keymode.ON_DEMAND}
      anchor={TOP}
      application={App}
      visible
      setup={self => {
        const screen = self.get_screen()
        const visual = screen.get_rgba_visual()
        if (visual) self.set_visual(visual)

        const keyCtrl = new Gtk.EventControllerKey()
        keyCtrl.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        self.add_controller(keyCtrl)
        keyCtrl.connect("key-pressed", (_ctrl, keyval, _keycode, state) => {
          const modShift = !!(state & Gdk.ModifierType.SHIFT_MASK)
          const modAlt = !!(state & Gdk.ModifierType.MOD1_MASK)
          const isShift = keyval === Gdk.KEY_Shift_L || keyval === Gdk.KEY_Shift_R
          const isAlt = keyval === Gdk.KEY_Alt_L || keyval === Gdk.KEY_Alt_R
          if ((isAlt && modShift) || (isShift && modAlt)) {
            const idx = layout.get() === "en" ? 1 : 0
            switchLayout(idx)
            layout.set(idx === 0 ? "en" : "es")
            return Gdk.EVENT_STOP
          }
          return Gdk.EVENT_PROPAGATE
        })
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
          <box spacing={0}>
            {workspaces().as(wsList => wsList.map(n => (
              <button
                className={activeWs().as(ws => ws === n ? "ws-btn active" : "ws-btn")}
                onClicked={() => switchWs(n)}
              >
                <label label={String(n)} />
              </button>
            )))}
          </box>
          <label className="sep" label="|" />
          <button className="island-btn" onClicked={() => open("nm-connection-editor")}>
            <label label={wifi()} />
          </button>
          <button className="island-btn" onClicked={() => open("blueman-manager")}>
            <label label={bt()} />
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
