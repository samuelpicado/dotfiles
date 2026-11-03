import Variable from "astal/variable"
import GLib from "gi://GLib"
import Network from "gi://AstalNetwork"
import Bluetooth from "gi://AstalBluetooth"
import Wp from "gi://AstalWp"
import Battery from "gi://AstalBattery"
import Hyprland from "gi://AstalHyprland"

export const network = Network.get_default()
export const bluetooth = Bluetooth.get_default()
export const audio = Wp.get_default().audio
export const batteryDevice = Battery.get_default()
export const hyprland = Hyprland.get_default()
export const clock = Variable("")
export const volumeIcon = Variable("")
export const volumeLevel = Variable("0")
export const wifiIcon = Variable("")
export const wifiDescription = Variable("Wi-Fi")
export const btIcon = Variable("")
export const battery = Variable("")
export const fullscreen = Variable(false)
export const layout = Variable("")

// El reloj solo despierta al cambiar el minuto; el resto usa señales nativas.
function tick() {
  const now = GLib.DateTime.new_now_local()
  clock.set(now.format("%H:%M  %a %d/%b") ?? "")
  GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 60 - now.get_second(), () => {
    tick()
    return GLib.SOURCE_REMOVE
  })
}
tick()

let speakerSignal = 0
let previousSpeaker = audio.default_speaker
function trackSpeaker() {
  if (speakerSignal && previousSpeaker) previousSpeaker.disconnect(speakerSignal)
  previousSpeaker = audio.default_speaker
  const update = () => {
    const speaker = audio.default_speaker
    volumeIcon.set(!speaker || speaker.mute ? "" : "")
    volumeLevel.set(!speaker ? "0" : speaker.mute ? "M" : String(Math.round(Math.max(0, Math.min(speaker.volume, 1)) * 100)))
  }
  speakerSignal = previousSpeaker?.connect("notify", update) ?? 0
  update()
}
audio.connect("notify::default-speaker", trackSpeaker)
trackSpeaker()

export function changeVolume(delta: number) {
  const speaker = audio.default_speaker
  if (speaker) speaker.volume = Math.max(0, Math.min(1, speaker.volume + delta / 100))
}

export function describeWifi(wifi: Network.Wifi | null) {
  if (!wifi) return ["", "Sin adaptador Wi-Fi"]
  if (!wifi.enabled) return ["", "Wi-Fi desactivado"]
  if (wifi.state >= Network.DeviceState.PREPARE && wifi.state <= Network.DeviceState.SECONDARIES)
    return ["", wifi.ssid ? `Conectando a ${wifi.ssid}…` : "Conectando a Wi-Fi…"]
  if (wifi.state === Network.DeviceState.ACTIVATED && wifi.ssid) return ["", wifi.ssid]
  return ["", "Wi-Fi sin conexión"]
}
let previousWifi = network.wifi
let wifiSignal = 0
function trackWifi() {
  if (wifiSignal && previousWifi) previousWifi.disconnect(wifiSignal)
  previousWifi = network.wifi
  const update = () => {
    const [icon, description] = describeWifi(previousWifi)
    wifiIcon.set(icon)
    wifiDescription.set(description)
  }
  wifiSignal = previousWifi?.connect("notify", update) ?? 0
  update()
}
network.connect("notify::wifi", trackWifi)
trackWifi()

const deviceSignals = new Map<any, number>()
function updateBluetooth() {
  for (const [device, id] of deviceSignals) device.disconnect(id)
  deviceSignals.clear()
  const update = () => btIcon.set(bluetooth.is_powered && bluetooth.get_devices().some(d => d.connected) ? "" : "")
  for (const device of bluetooth.get_devices())
    deviceSignals.set(device, device.connect("notify::connected", update))
  update()
}
bluetooth.connect("notify::is-powered", updateBluetooth)
bluetooth.connect("device-added", updateBluetooth)
bluetooth.connect("device-removed", updateBluetooth)
updateBluetooth()

const updateBattery = () => battery.set(batteryDevice?.is_present ? `${Math.round(batteryDevice.percentage * 100)}%` : "—")
batteryDevice?.connect("notify", updateBattery)
updateBattery()

function updateFullscreen() {
  try { fullscreen.set(Boolean(JSON.parse(hyprland.message("j/activeworkspace")).hasfullscreen)) }
  catch (error) { console.error("Estado de la pantalla:", error) }
}
hyprland.connect("event", (_self, event) => {
  if (["fullscreen", "workspace", "workspacev2", "focusedmon"].includes(event)) updateFullscreen()
})
updateFullscreen()

let keyboardName = ""
function updateLayout(name: string) {
  const normalized = name.toLowerCase()
  layout.set(/spanish|latam|latin/.test(normalized) ? "es" : /english|\bus\b/.test(normalized) ? "en" : normalized.slice(0, 2))
}
function readKeyboard() {
  try {
    const devices = JSON.parse(hyprland.message("j/devices"))
    const keyboard = devices.keyboards?.find((item: any) => item.main)
    keyboardName = keyboard?.name ?? ""
    updateLayout(keyboard?.active_keymap ?? "")
  } catch (error) { console.error("Distribución del teclado:", error) }
}
hyprland.connect("keyboard-layout", (_self, keyboard, name) => {
  if (keyboard === keyboardName) updateLayout(name)
})
hyprland.connect("config-reloaded", readKeyboard)
readKeyboard()

export function switchLayout(index: number) {
  return keyboardName ? hyprland.message(`switchxkblayout ${keyboardName} ${index}`) : ""
}
