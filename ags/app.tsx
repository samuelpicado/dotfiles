import App from "astal/gtk3/app"
import { Astal, Gtk, Gdk } from "astal/gtk3"
import { wifiPanel } from "./wifi"
import { clock, volumeIcon, volumeLevel, wifiIcon, wifiDescription, btIcon, bluetooth, battery, fullscreen, layout, batteryDevice, changeVolume } from "./state"
import { togglePanel, dismissPanel, calendarPanel, audioPanel, bluetoothPanel, batteryPanel, keyboardPanel, powerPanel, watch } from "./controls"
import { activeIndicator, smoothLabel, observe } from "./motion"
import { notificationPending, notificationsPaused } from "./notifications"

function dateContent() {
  const overlay = new Gtk.Overlay()
  const label = new Gtk.Label()
  observe(label, clock, () => { label.label = clock.get() })
  overlay.add(label)
  const dot = new Gtk.Box({ width_request: 5, height_request: 5 })
  dot.get_style_context().add_class("notification-dot")
  const reveal = new Gtk.Revealer({ transition_type: Gtk.RevealerTransitionType.CROSSFADE, transition_duration: 180, halign: Gtk.Align.END, valign: Gtk.Align.START, margin_top: 3, margin_end: 3 })
  reveal.add(dot)
  observe(reveal, notificationPending, () => { reveal.reveal_child = notificationPending.get() > 0 })
  overlay.add_overlay(reveal)
  overlay.set_overlay_pass_through(reveal, true)
  overlay.show_all()
  return overlay
}

App.start({
  css: `
    @define-color island_text #eef0f4;
    @define-color island_muted #969ca8;
    @define-color island_accent #9fc4ff;
    window { background: transparent; }
    .island-pill {
      background-color: #000000;
      color: @island_text;
      border: 1px solid rgba(210, 222, 245, 0.19);
      border-radius: 9999px;
      padding: 0 9px;
      margin-top: 2px;
      min-height: 18px;
      font-family: "JetBrainsMono Nerd Font";
      font-size: 11px;
    }
    .island-btn {
      background: transparent;
      background-image: none;
      color: @island_text;
      border: none;
      border-radius: 9999px;
      box-shadow: none;
      padding: 0 5px;
      min-width: 0;
      transition: background-color 180ms ease-in-out, color 180ms ease-in-out, box-shadow 180ms ease-in-out;
    }
    .island-btn:hover {
      background-color: rgba(210, 222, 245, 0.12);
      color: #ffffff;
    }
    .island-btn:active {
      background-color: rgba(159, 196, 255, 0.19);
      color: @island_accent;
      box-shadow: inset 0 0 0 1px rgba(159, 196, 255, 0.12);
      transition-duration: 70ms;
    }
    .island-btn.module-open, .island-btn.module-open:hover { color: @island_accent; background: transparent; box-shadow: none; }
    .island-btn:focus { outline: 1px solid @island_accent; }
    .island-btn.date-btn { padding: 0; min-height: 0; }
    .network-menu { background: transparent; padding: 0; box-shadow: none; }
    .network-menu > menuitem { padding: 8px 12px; border-radius: 9px; }
    .island-btn.battery-low { color: #f3bc9b; }
    .island-btn.battery-charging, .island-btn.connection-busy { color: @island_accent; }
    .island-btn.connection-off { color: @island_muted; }
    .notification-dot { background: @island_accent; border-radius: 9999px; }
    .island-panel, menu {
      background: rgba(20, 21, 26, 0.97);
      color: @island_text;
      border: 1px solid rgba(210, 222, 245, 0.16);
      border-radius: 16px;
      box-shadow: 0 6px 18px rgba(0, 0, 0, 0.18);
      font-family: Sans;
      font-size: 13px;
    }
    .panel-content { padding: 16px; min-width: 280px; }
    .panel-title { font-size: 15px; font-weight: 600; }
    .panel-value { font-size: 30px; font-weight: 600; }
    .muted { color: @island_muted; }
    .panel-tabs button { background: rgba(210, 222, 245, 0.05); color: @island_muted; border: none; border-radius: 9px; padding: 7px 10px; box-shadow: none; transition: background-color 180ms ease-in-out, color 180ms ease-in-out; }
    .panel-tabs button:checked { background: rgba(159, 196, 255, 0.16); color: @island_text; }
    .panel-tabs button:hover { color: @island_text; }
    .notification-card { background: rgba(210, 222, 245, 0.04); border: 1px solid rgba(210, 222, 245, 0.08); border-radius: 11px; padding: 10px; }
    .notification-card.critical { border-color: rgba(243, 166, 161, 0.55); }
    .notification-title { font-weight: 600; }
    .notification-app { color: @island_muted; font-size: 11px; }
    menu {
      padding: 6px;
      -gtk-outline-top-left-radius: 16px;
      -gtk-outline-top-right-radius: 16px;
      -gtk-outline-bottom-left-radius: 16px;
      -gtk-outline-bottom-right-radius: 16px;
    }
    menuitem, .panel-action {
      background: transparent;
      background-image: none;
      color: @island_text;
      border: none;
      border-radius: 9px;
      box-shadow: none;
      padding: 8px 12px;
      transition: background-color 180ms ease-in-out, color 180ms ease-in-out;
    }
    menuitem { min-width: 180px; }
    menuitem:hover, .network-menu > menuitem.network-hover:not(:disabled), .panel-action:hover {
      background-color: rgba(159, 196, 255, 0.13);
      color: #ffffff;
    }
    menuitem:active, .panel-action:active { background-color: rgba(159, 196, 255, 0.22); }
    .panel-action.destructive { color: #f3a6a1; background-color: rgba(243, 166, 161, 0.1); }
    calendar {
      font-size: 17px;
      background: transparent;
      color: @island_text;
      border: none;
      padding: 8px;
      border-radius: 12px;
    }
    calendar.header { color: @island_text; background: transparent; border: none; }
    calendar.highlight { color: @island_muted; }
    calendar.day-number { padding: 0 8px; }
    calendar:selected {
      background-color: @island_accent;
      color: #101114;
      border-radius: 6px;
    }
    calendar:indeterminate { color: #606673; }
    scale trough { min-height: 5px; background: rgba(210, 222, 245, 0.13); border: none; border-radius: 9999px; }
    scale highlight { background: @island_accent; border: none; border-radius: 9999px; }
    scale slider { min-width: 14px; min-height: 14px; background: #eef0f4; border: none; box-shadow: none; }
    switch { border: none; background: #383c46; }
    switch:checked { background: #719ce0; }
    switch slider { background: #eef0f4; border: none; box-shadow: none; }
    checkbutton { color: @island_muted; }
  `,
  main() {
    return <window
      name="island"
      namespace="island"
      layer={Astal.Layer.OVERLAY}
      exclusivity={Astal.Exclusivity.EXCLUSIVE}
      keymode={Astal.Keymode.NONE}
      anchor={Astal.WindowAnchor.TOP}
      application={App}
      visible
      setup={self => {
        const visual = self.get_screen().get_rgba_visual()
        if (visual) self.set_visual(visual)
        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
        self.connect("button-press-event", (_self, event) =>
          Gtk.get_event_widget(event) === self && dismissPanel())
      }}
    >
      <box halign={Gtk.Align.CENTER} valign={Gtk.Align.START}>
        <revealer
          revealChild={fullscreen().as(value => !value)}
          transitionType={Gtk.RevealerTransitionType.SLIDE_DOWN}
          transitionDuration={Gtk.Settings.get_default()?.gtk_enable_animations ? 200 : 0}
          valign={Gtk.Align.START}
        >
          <box className="island-pill" halign={Gtk.Align.CENTER} spacing={0} setup={activeIndicator}>
            <box widthRequest={94} spacing={0}>
            <button
              className="island-btn"
              widthRequest={24}
              tooltipText="Redes y conexiones"
              setup={self => {
                const update = () => {
                  self.tooltip_text = wifiDescription.get()
                  self.get_accessible().set_name(`Redes: ${self.tooltip_text}`)
                  const context = self.get_style_context()
                  if (["", ""].includes(wifiIcon.get())) context.add_class("connection-off")
                  else context.remove_class("connection-off")
                  if (wifiIcon.get() === "") context.add_class("connection-busy")
                  else context.remove_class("connection-busy")
                }
                observe(self, wifiDescription, update)
                observe(self, wifiIcon, update)
              }}
              onClicked={self => togglePanel(self, wifiPanel)}
            >{smoothLabel(wifiIcon, 10)}</button>
            <button
              className="island-btn"
              tooltipText="Bluetooth"
              widthRequest={24}
              onClicked={self => togglePanel(self, bluetoothPanel)}
              setup={self => {
                const update = () => {
                  self.tooltip_text = bluetooth.is_powered ? "Bluetooth activado" : "Bluetooth desactivado"
                  self.get_accessible().set_name(self.tooltip_text)
                  const context = self.get_style_context()
                  if (bluetooth.is_powered) context.remove_class("connection-off")
                  else context.add_class("connection-off")
                }
                watch(self, bluetooth, "notify::is-powered", update)
                update()
              }}
            >{smoothLabel(btIcon, 10)}</button>
            <button
              className="island-btn"
              tooltipText="Sonido · desplaza para ajustar"
              widthRequest={46}
              onClicked={self => togglePanel(self, audioPanel)}
              setup={self => {
                observe(self, volumeLevel, () => {
                  self.tooltip_text = `Sonido: ${volumeLevel.get() === "M" ? "silenciado" : `${volumeLevel.get()}%`} · desplaza para ajustar`
                  self.get_accessible().set_name(self.tooltip_text)
                })
                self.add_events(Gdk.EventMask.SMOOTH_SCROLL_MASK | Gdk.EventMask.SCROLL_MASK)
                self.connect("scroll-event", (_self, event: Gdk.EventScroll) => {
                  const direction = event.get_scroll_direction()[1]
                  if (direction === Gdk.ScrollDirection.UP) changeVolume(5)
                  else if (direction === Gdk.ScrollDirection.DOWN) changeVolume(-5)
                  else if (direction === Gdk.ScrollDirection.SMOOTH) {
                    const [, , delta] = event.get_scroll_deltas()
                    if (delta) changeVolume(delta > 0 ? -2 : 2)
                  }
                  return true
                })
              }}
            ><box spacing={3}>{smoothLabel(volumeIcon, 9)}{smoothLabel(volumeLevel, 22, 1)}</box></button>
            </box>
            <button
              className="island-btn date-btn"
              tooltipText="Calendario y notificaciones"
              widthRequest={141}
              onClicked={self => togglePanel(self, calendarPanel)}
              setup={self => {
                const update = () => {
                  self.tooltip_text = `Calendario · ${notificationPending.get()} notificaciones pendientes${notificationsPaused.get() ? " · No molestar" : ""}`
                  self.get_accessible().set_name(self.tooltip_text)
                }
                observe(self, notificationPending, update)
                observe(self, notificationsPaused, update)
              }}
            >{dateContent()}</button>
            <box widthRequest={94} spacing={0}>
            <button
              className="island-btn"
              tooltipText="Batería y energía"
              widthRequest={46}
              onClicked={self => togglePanel(self, batteryPanel)}
              setup={self => {
                const update = () => {
                  const context = self.get_style_context()
                  if (batteryDevice?.is_present && !batteryDevice.charging && batteryDevice.percentage <= 0.2)
                    context.add_class("battery-low")
                  else context.remove_class("battery-low")
                  if (batteryDevice?.charging) context.add_class("battery-charging")
                  else context.remove_class("battery-charging")
                  self.tooltip_text = `Batería: ${battery.get()} · ${batteryDevice?.charging ? "conectada a la corriente" : "usando batería"}`
                  self.get_accessible().set_name(self.tooltip_text)
                }
                watch(self, batteryDevice, "notify", update)
                update()
              }}
            >{smoothLabel(battery, 30, 1)}</button>
            <button
              className="island-btn"
              tooltipText="Distribución del teclado"
              widthRequest={24}
              onClicked={self => togglePanel(self, keyboardPanel)}
              setup={self => observe(self, layout, () => { self.get_accessible().set_name(`Distribución del teclado: ${layout.get()}`) })}
            >{smoothLabel(layout, 14)}</button>
            <button
              className="island-btn"
              tooltipText="Sesión y energía"
              widthRequest={24}
              onClicked={self => togglePanel(self, powerPanel)}
              setup={self => self.get_accessible().set_name("Sesión y energía")}
            ><label label="" /></button>
            </box>
          </box>
        </revealer>
      </box>
    </window>
  },
})
