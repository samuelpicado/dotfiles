import App from "astal/gtk3/app"
import { Astal, Gtk, Gdk } from "astal/gtk3"
import { wifiPanel } from "./wifi"
import { clock, volume, wifiIcon, btIcon, battery, fullscreen, layout, network, batteryDevice, changeVolume } from "./state"
import { togglePanel, dismissPanel, calendarPanel, audioPanel, bluetoothPanel, batteryPanel, keyboardPanel, powerPanel, watch } from "./controls"

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
    .island-btn:active, .island-btn.module-open {
      background-color: rgba(159, 196, 255, 0.19);
      color: @island_accent;
      box-shadow: inset 0 0 0 1px rgba(159, 196, 255, 0.12);
    }
    .island-btn:focus { outline: 1px solid @island_accent; }
    .island-btn.date-btn { padding: 0; min-height: 0; }
    .network-menu { background: transparent; padding: 0; box-shadow: none; }
    .network-menu > menuitem { padding: 8px 12px; border-radius: 9px; }
    .island-btn.battery-low { color: #f3bc9b; }
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
          <box className="island-pill" halign={Gtk.Align.CENTER} spacing={0}>
            <box widthRequest={94} spacing={0}>
            <button
              className="island-btn"
              widthRequest={24}
              tooltipText="Redes y conexiones"
              setup={self => {
                const update = () => { self.tooltip_text = network.wifi?.ssid || "Redes y conexiones" }
                watch(self, network.wifi, "notify::ssid", update)
                update()
              }}
              onClicked={self => togglePanel(self, wifiPanel)}
            ><label label={wifiIcon()} /></button>
            <button
              className="island-btn"
              tooltipText="Bluetooth"
              widthRequest={24}
              onClicked={self => togglePanel(self, bluetoothPanel)}
            ><label label={btIcon()} /></button>
            <button
              className="island-btn"
              tooltipText="Sonido · desplaza para ajustar"
              widthRequest={46}
              onClicked={self => togglePanel(self, audioPanel)}
              setup={self => {
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
            ><label label={volume()} /></button>
            </box>
            <button
              className="island-btn date-btn"
              tooltipText="Calendario y notificaciones"
              widthRequest={141}
              onClicked={self => togglePanel(self, calendarPanel)}
            ><label label={clock()} /></button>
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
                }
                watch(self, batteryDevice, "notify", update)
                update()
              }}
            ><label label={battery()} /></button>
            <button
              className="island-btn"
              tooltipText="Distribución del teclado"
              widthRequest={24}
              onClicked={self => togglePanel(self, keyboardPanel)}
            ><label label={layout()} /></button>
            <button
              className="island-btn"
              tooltipText="Sesión y energía"
              widthRequest={24}
              onClicked={self => togglePanel(self, powerPanel)}
            ><label label="" /></button>
            </box>
          </box>
        </revealer>
      </box>
    </window>
  },
})
