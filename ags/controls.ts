import { Astal, Gtk, Gdk } from "astal/gtk3"
import GLib from "gi://GLib"
import Cairo from "cairo"
import { audio, batteryDevice, bluetooth, layout, switchLayout } from "./state"
import { selectedModule, observe } from "./motion"
import { notificationHistory, notificationPending, notificationsPaused, notificationsReady, markNotificationsRead, setNotificationsPaused } from "./notifications"

export function launch(command: string) { GLib.spawn_command_line_async(command) }

export function watch(widget: Gtk.Widget, source: any, signal: string, update: () => void) {
  if (!source) return
  const id = source.connect(signal, update)
  widget.connect("destroy", () => source.disconnect(id))
}

export function column(title: string) {
  const box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 12 })
  box.get_style_context().add_class("panel-content")
  const heading = new Gtk.Label({ label: title, xalign: 0 })
  heading.get_style_context().add_class("panel-title")
  box.add(heading)
  return box
}

export function action(label: string, callback: () => void) {
  const button = new Gtk.Button({ label, halign: Gtk.Align.FILL })
  button.get_style_context().add_class("panel-action")
  button.connect("clicked", callback)
  return button
}

type PanelBuilder = (close: () => void) => Gtk.Widget
let activePanel: {
  window: Astal.Window
  toggle: (button: Gtk.Button, build: PanelBuilder) => Astal.Window | null
  close: () => void
} | null = null

export function dismissPanel() {
  if (!activePanel) return false
  activePanel.close()
  return true
}

export function togglePanel(button: Gtk.Button, build: PanelBuilder) {
  if (activePanel) return activePanel.toggle(button, build)
  const parent = button.get_toplevel() as Gtk.Window
  const popup = new Astal.Window({
    application: parent.get_application(),
    namespace: "island-popup",
    layer: Astal.Layer.OVERLAY,
    anchor: Astal.WindowAnchor.TOP | Astal.WindowAnchor.BOTTOM | Astal.WindowAnchor.LEFT | Astal.WindowAnchor.RIGHT,
    exclusivity: Astal.Exclusivity.IGNORE,
    keymode: Astal.Keymode.ON_DEMAND,
    margin_top: 0,
  })
  const visual = popup.get_screen().get_rgba_visual()
  if (visual) popup.set_visual(visual)
  if (parent.get_window()) popup.gdkmonitor = parent.get_display().get_monitor_at_window(parent.get_window())
  popup.get_style_context().add_class("island-overlay")
  popup.connect("size-allocate", (_self, allocation) => {
    const region = new Cairo.Region()
    region.unionRectangle({ x: 0, y: 0, width: allocation.width, height: allocation.height })
    // Los botones siguen recibiendo el cursor; el resto de la superficie cierra el panel.
    region.subtractRectangle({
      x: Math.round((allocation.width - parent.get_allocated_width()) / 2), y: 0,
      width: parent.get_allocated_width(), height: parent.get_allocated_height(),
    })
    popup.input_shape_combine_region(region)
  })
  const duration = Gtk.Settings.get_default()?.gtk_enable_animations ? 200 : 0
  const reveal = new Gtk.Revealer({
    transition_type: Gtk.RevealerTransitionType.SLIDE_DOWN,
    transition_duration: duration,
    halign: Gtk.Align.CENTER, valign: Gtk.Align.START, margin_top: 36,
  })
  const stack = new Gtk.Stack({
    hhomogeneous: false, vhomogeneous: false, interpolate_size: true,
    transition_type: Gtk.StackTransitionType.CROSSFADE, transition_duration: duration,
  })
  const prune = () => {
    if (stack.transition_running) return
    for (const child of stack.get_children()) if (child !== stack.visible_child) child.destroy()
  }
  stack.connect("notify::transition-running", prune)
  let activeButton: Gtk.Button | null = null
  const close = () => {
    if (!reveal.reveal_child) return
    surface.sensitive = false
    activeButton?.get_style_context().remove_class("module-open")
    selectedModule.set(null)
    reveal.reveal_child = false
  }
  reveal.connect("notify::child-revealed", () => {
    if (!reveal.reveal_child && !reveal.child_revealed) popup.destroy()
  })
  const surface = new Gtk.EventBox()
  surface.get_style_context().add_class("island-panel")
  surface.add(stack)
  surface.connect("button-press-event", () => true)
  reveal.add(surface)
  const backdrop = new Gtk.EventBox({ visible_window: false })
  backdrop.add(reveal)
  backdrop.connect("button-press-event", () => { close(); return true })
  popup.add(backdrop)
  popup.connect("key-press-event", (_self, event) => {
    if (event.get_keyval()[1] !== Gdk.KEY_Escape) return false
    close()
    return true
  })
  const toggle = (nextButton: Gtk.Button, nextBuild: PanelBuilder) => {
    if (activeButton === nextButton && reveal.reveal_child) { close(); return null }
    if (activeButton !== nextButton) {
      activeButton?.get_style_context().remove_class("module-open")
      const content = nextBuild(close)
      stack.add(content)
      content.show_all()
      stack.visible_child = content
      activeButton = nextButton
    }
    surface.sensitive = true
    activeButton.get_style_context().add_class("module-open")
    selectedModule.set(activeButton)
    popup.show_all()
    reveal.reveal_child = true
    prune()
    return popup
  }
  activePanel = { window: popup, toggle, close }
  const parentDestroyed = parent.connect("destroy", () => popup.destroy())
  popup.connect("destroy", () => {
    parent.disconnect(parentDestroyed)
    activeButton?.get_style_context().remove_class("module-open")
    selectedModule.set(null)
    if (activePanel?.window === popup) activePanel = null
  })
  return toggle(button, build)
}

function notificationView() {
  const box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 10 })
  const status = new Gtk.Label({ xalign: 0 })
  status.get_style_context().add_class("muted")
  const rows = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 8 })
  const scroll = new Gtk.ScrolledWindow({ max_content_height: 260, propagate_natural_height: true, hscrollbar_policy: Gtk.PolicyType.NEVER })
  scroll.add(rows)
  const updateStatus = () => {
    status.label = `${notificationHistory.get().length} en historial · ${notificationPending.get()} pendientes`
  }
  observe(status, notificationPending, updateStatus)
  const render = () => {
    rows.get_children().forEach(child => child.destroy())
    const notices = notificationHistory.get()
    if (!notices.length) {
      const empty = new Gtk.Label({ label: notificationsReady.get() ? "No hay notificaciones" : "Notificaciones no disponibles", margin_top: 20, margin_bottom: 20 })
      empty.get_style_context().add_class("muted")
      rows.add(empty)
    }
    for (const notice of notices) {
      const card = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 4, tooltip_text: notice.summary })
      card.get_style_context().add_class("notification-card")
      if (notice.critical) card.get_style_context().add_class("critical")
      const app = new Gtk.Label({ label: notice.app, xalign: 0, ellipsize: 3, max_width_chars: 36 })
      app.get_style_context().add_class("notification-app")
      const title = new Gtk.Label({ label: notice.summary, xalign: 0, wrap: true, wrap_mode: 2, ellipsize: 3, lines: 2, max_width_chars: 38 })
      title.get_style_context().add_class("notification-title")
      card.add(app)
      card.add(title)
      if (notice.body) card.add(new Gtk.Label({ label: notice.body, xalign: 0, wrap: true, wrap_mode: 2, ellipsize: 3, lines: 3, max_width_chars: 38 }))
      rows.add(card)
    }
    rows.show_all()
    updateStatus()
    if (box.get_mapped() && notificationsReady.get()) markNotificationsRead()
  }
  observe(box, notificationHistory, render)
  observe(box, notificationsReady, render)
  box.connect("map", markNotificationsRead)
  box.add(status)
  box.add(scroll)
  return box
}

export function calendarPanel() {
  const box = column("Calendario y avisos")
  const date = new Gtk.Label({ label: GLib.DateTime.new_now_local().format("%A, %e %B") ?? "", xalign: 0 })
  date.get_style_context().add_class("muted")
  box.add(date)
  const calendar = new Gtk.Calendar({ show_heading: true, show_day_names: true })
  calendar.name = "island-calendar"
  const markToday = () => {
    const today = GLib.DateTime.new_now_local()
    const [year, month] = calendar.get_date()
    calendar.clear_marks()
    if (year === today.get_year() && month === today.get_month() - 1) calendar.mark_day(today.get_day_of_month())
  }
  const today = () => {
    const now = GLib.DateTime.new_now_local()
    calendar.select_month(now.get_month() - 1, now.get_year())
    calendar.select_day(now.get_day_of_month())
    markToday()
  }
  calendar.connect("month-changed", markToday)
  today()
  const dates = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 12 })
  dates.add(calendar)
  dates.add(action("Hoy", today))
  const pages = new Gtk.Stack({ hhomogeneous: true, vhomogeneous: false, interpolate_size: true, transition_type: Gtk.StackTransitionType.CROSSFADE, transition_duration: 180 })
  pages.add_titled(dates, "calendar", "Calendario")
  pages.add_titled(notificationView(), "notifications", "Notificaciones")
  const tabs = new Gtk.StackSwitcher({ stack: pages, halign: Gtk.Align.CENTER })
  tabs.get_style_context().add_class("panel-tabs")
  box.add(tabs)
  box.add(pages)
  const quiet = new Gtk.CheckButton({ label: "No molestar", sensitive: false })
  let syncing = false
  let alive = true
  quiet.connect("destroy", () => { alive = false })
  const syncQuiet = () => {
    syncing = true
    quiet.active = notificationsPaused.get()
    quiet.sensitive = notificationsReady.get()
    syncing = false
  }
  observe(quiet, notificationsPaused, syncQuiet)
  observe(quiet, notificationsReady, syncQuiet)
  quiet.connect("toggled", () => {
    if (!syncing) setNotificationsPaused(quiet.active).catch(error => { console.error(error); if (alive) syncQuiet() })
  })
  box.add(quiet)
  return box
}

export function audioPanel(close: () => void) {
  const box = column("Sonido")
  const device = new Gtk.Label({ xalign: 0, ellipsize: 3, max_width_chars: 30 })
  device.get_style_context().add_class("muted")
  const slider = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 1, 0.01)
  slider.draw_value = false
  const mute = action("Silenciar", () => {
    const speaker = audio.default_speaker
    if (speaker) speaker.mute = !speaker.mute
  })
  let syncing = false
  let trackedSpeaker: any = null
  let speakerSignal = 0
  const update = () => {
    const speaker = audio.default_speaker
    syncing = true
    slider.set_value(speaker?.volume ?? 0)
    slider.sensitive = Boolean(speaker)
    mute.sensitive = Boolean(speaker)
    mute.label = speaker?.mute ? "Activar sonido" : "Silenciar"
    device.label = speaker?.description ?? "Sin salida de audio"
    syncing = false
  }
  const track = () => {
    if (speakerSignal && trackedSpeaker) trackedSpeaker.disconnect(speakerSignal)
    trackedSpeaker = audio.default_speaker
    speakerSignal = trackedSpeaker?.connect("notify", update) ?? 0
    update()
  }
  watch(box, audio, "notify::default-speaker", track)
  box.connect("destroy", () => { if (speakerSignal && trackedSpeaker) trackedSpeaker.disconnect(speakerSignal) })
  slider.connect("value-changed", () => {
    if (!syncing && audio.default_speaker) audio.default_speaker.volume = slider.get_value()
  })
  box.add(device)
  box.add(slider)
  box.add(mute)
  box.add(action("Ajustes de sonido", () => { close(); launch("pavucontrol") }))
  track()
  return box
}

export function bluetoothPanel(close: () => void) {
  const box = column("Bluetooth")
  const enabled = new Gtk.Switch({ halign: Gtk.Align.END, active: bluetooth.is_powered })
  enabled.connect("notify::active", () => {
    if (enabled.active !== bluetooth.is_powered) bluetooth.is_powered = enabled.active
  })
  watch(box, bluetooth, "notify::is-powered", () => { enabled.active = bluetooth.is_powered })
  box.add(enabled)
  const rows = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 4 })
  const refresh = () => {
    rows.get_children().forEach(child => child.destroy())
    const devices = bluetooth.is_powered ? bluetooth.get_devices() : []
    if (!devices.length) rows.add(new Gtk.Label({ label: bluetooth.is_powered ? "No hay dispositivos guardados" : "Bluetooth desactivado" }))
    for (const device of devices) {
      const row = action("", () => {
        if (!device.paired) { close(); launch("blueman-manager") }
        else if (device.connected) device.disconnect_device()
        else device.connect_device()
      })
      const label = row.get_child() as Gtk.Label
      label.ellipsize = 3
      label.max_width_chars = 34
      const update = () => {
        row.label = `${device.alias || device.name || device.address}  ${device.connecting ? "Conectando…" : device.connected ? "Conectado" : ""}`
        row.sensitive = !device.connecting
      }
      watch(row, device, "notify", update)
      update()
      rows.add(row)
    }
    rows.show_all()
  }
  for (const signal of ["notify::is-powered", "device-added", "device-removed"]) watch(box, bluetooth, signal, refresh)
  refresh()
  const scroll = new Gtk.ScrolledWindow({ max_content_height: 240, propagate_natural_height: true, hscrollbar_policy: Gtk.PolicyType.NEVER })
  scroll.add(rows)
  box.add(scroll)
  box.add(action("Vincular dispositivos…", () => { close(); launch("blueman-manager") }))
  return box
}

export function batteryPanel() {
  const box = column("Batería")
  const percent = new Gtk.Label()
  percent.get_style_context().add_class("panel-value")
  const state = new Gtk.Label()
  state.get_style_context().add_class("muted")
  const update = () => {
    percent.label = batteryDevice?.is_present ? `${Math.round(batteryDevice.percentage * 100)}%` : "Sin batería"
    state.label = batteryDevice?.charging ? "Conectada a la corriente" : "Usando batería"
  }
  watch(box, batteryDevice, "notify", update)
  update()
  box.add(percent)
  box.add(state)
  return box
}

export function keyboardPanel(close: () => void) {
  const box = column("Teclado")
  for (const [index, label, code] of [[0, "English (US)", "en"], [1, "Español (Latinoamérica)", "es"]] as const) {
    const button = action(`${layout.get() === code ? "✓  " : ""}${label}`, () => { switchLayout(index); close() })
    box.add(button)
  }
  return box
}

export function powerPanel(close: () => void) {
  const box = column("Sesión")
  box.add(action("Bloquear", () => { close(); launch("hyprlock") }))
  box.add(action("Suspender", () => { close(); launch("systemctl suspend") }))
  let armed: Gtk.Button | null = null
  for (const [label, command] of [["Cerrar sesión", "hyprctl dispatch 'hl.dsp.exit()'"], ["Reiniciar", "systemctl reboot"], ["Apagar", "systemctl poweroff"]]) {
    const button = action(label, () => {
      if (armed === button) { close(); launch(command); return }
      if (armed) { armed.label = armed.name; armed.get_style_context().remove_class("destructive") }
      armed = button
      button.label = `Confirmar: ${label}`
      button.get_style_context().add_class("destructive")
    })
    button.name = label
    box.add(button)
  }
  return box
}
