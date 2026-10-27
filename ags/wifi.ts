import Gtk from "gi://Gtk?version=3.0"
import Gio from "gi://Gio"
import GObject from "gi://GObject"
import Pango from "gi://Pango"
import Tray from "gi://AstalTray"
import { action, column, launch, watch } from "./controls"

const tray = Tray.get_default()

// GTK 3 suma el indicador a la altura al distribuir un menú vertical, incluso con contenido horizontal.
const NetworkMenu = GObject.registerClass(class NetworkMenu extends Gtk.MenuBar {
  toggleHeight() {
    return this.get_children().filter(child => child.visible)
      .reduce((height, child) => height + (child as Gtk.MenuItem).toggle_size_request(0), 0)
  }
  vfunc_get_preferred_height() {
    return super.vfunc_get_preferred_height().map(height => height + this.toggleHeight())
  }
  vfunc_get_preferred_height_for_width(width: number) {
    return super.vfunc_get_preferred_height_for_width(width).map(height => height + this.toggleHeight())
  }
})

export function wifiPanel(close: () => void) {
  const box = column("Wi-Fi")
  const actions = new Gio.SimpleActionGroup()
  const eduroam = new Gio.SimpleAction({ name: "eduroam" })
  eduroam.connect("activate", () => { close(); launch("geteduroam-gui") })
  actions.add_action(eduroam)
  box.insert_action_group("wifi", actions)
  let menu: Gtk.Widget | null = null
  const refresh = () => {
    menu?.destroy()
    const item = tray.get_items().find(item => item.id === "nm-applet")
    if (item?.menu_model && item.action_group) {
      item.about_to_show()
      box.insert_action_group("dbusmenu", item.action_group)
      const model = new Gio.Menu()
      model.append_section(null, item.menu_model)
      model.append("Configurar eduroam…", "wifi.eduroam")
      const connections = new NetworkMenu()
      connections.pack_direction = Gtk.PackDirection.TTB
      connections.child_pack_direction = Gtk.PackDirection.LTR
      const fitLabel = (_menu: Gtk.MenuBar, row: Gtk.MenuItem) => {
        row.connect("enter-notify-event", () => {
          if (row.is_sensitive()) row.get_style_context().add_class("network-hover")
          return false
        })
        row.connect("leave-notify-event", () => {
          row.get_style_context().remove_class("network-hover")
          return false
        })
        const label = row.get_child()
        if (!(label instanceof Gtk.Label)) return
        label.max_width_chars = 32
        label.ellipsize = Pango.EllipsizeMode.END
        const tooltip = () => { row.tooltip_text = label.get_text() }
        watch(row, label, "notify::label", tooltip)
        tooltip()
      }
      connections.connect("insert", fitLabel)
      connections.bind_model(model, null, true)
      connections.get_style_context().add_class("network-menu")
      connections.connect("selection-done", close)
      menu = connections
    } else {
      menu = action("Abrir conexiones…", () => { close(); launch("nm-connection-editor") })
    }
    box.add(menu)
    menu.show_all()
  }
  watch(box, tray, "notify::items", refresh)
  refresh()
  return box
}
