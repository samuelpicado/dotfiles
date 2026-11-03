import Gio from "gi://Gio"
import GLib from "gi://GLib"
import Pango from "gi://Pango"
import Variable from "astal/variable"

export type Notice = { key: string, app: string, summary: string, body: string, critical: boolean }
export const notificationHistory = Variable<Notice[]>([])
export const notificationPending = Variable(0)
export const notificationsPaused = Variable(false)
export const notificationsReady = Variable(false)

const directory = `${GLib.get_user_state_dir()}/ags`
const readFile = Gio.File.new_for_path(`${directory}/notifications-read.json`)
let seen = new Set<string>()
if (readFile.query_exists(null)) {
  try {
    const [, bytes] = readFile.load_contents(null)
    const keys = JSON.parse(new TextDecoder().decode(bytes))
    if (Array.isArray(keys)) seen = new Set(keys.filter(key => typeof key === "string").slice(-200))
  } catch (error) { console.error("Estado de avisos:", error) }
}

let proxy: Gio.DBusProxy | null = null
let scheduled = 0, loading = false, dirty = false
let lastHistory = ""
const property = (name: string) => proxy?.get_cached_property(name)?.unpack()
function recount() {
  notificationPending.set(notificationHistory.get().filter(notice => !seen.has(notice.key)).length
    + Number(property("displayedLength") ?? 0) + Number(property("waitingLength") ?? 0))
}
function call(method: string, parameters: GLib.Variant | null = null): Promise<GLib.Variant> {
  return new Promise((resolve, reject) => {
    if (!proxy?.g_name_owner) { reject(new Error("Dunst no está disponible")); return }
    proxy.call(method, parameters, Gio.DBusCallFlags.NONE, 1500, null, (source, result) => {
      try { resolve(source!.call_finish(result)) } catch (error) { reject(error) }
    })
  })
}
function plain(text: string) {
  try { return Pango.parse_markup(text, -1, "")[2] } catch { return text }
}
async function loadHistory() {
  if (loading) { dirty = true; return }
  if (!proxy?.g_name_owner) {
    lastHistory = ""
    notificationsReady.set(false)
    notificationHistory.set([])
    notificationPending.set(0)
    return
  }
  loading = true
  try {
    const [rows] = (await call("NotificationListHistory")).deep_unpack()
    const notices: Notice[] = rows.slice(0, 30).map((row: Record<string, GLib.Variant>) => ({
      key: `${row.id.unpack()}:${row.timestamp.unpack()}`,
      app: String(row.appname?.unpack() || "Notificación"),
      summary: plain(String(row.summary?.unpack() || "Sin título")),
      body: plain(String(row.body?.unpack() || "")),
      critical: row.urgency?.unpack() === "CRITICAL",
    }))
    const snapshot = JSON.stringify(notices)
    if (snapshot !== lastHistory) { lastHistory = snapshot; notificationHistory.set(notices) }
    notificationsReady.set(true)
    notificationsPaused.set(Boolean(property("paused")))
    recount()
  } catch (error) {
    notificationsReady.set(false)
    console.error("Historial de avisos:", error)
  } finally {
    loading = false
    if (dirty) { dirty = false; refresh() }
  }
}
function refresh() {
  if (scheduled) return
  scheduled = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
    scheduled = 0
    loadHistory()
    return GLib.SOURCE_REMOVE
  })
}
Gio.DBusProxy.new_for_bus(Gio.BusType.SESSION, Gio.DBusProxyFlags.DO_NOT_AUTO_START, null,
  "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.dunstproject.cmd0", null,
  (_source, result) => {
    try {
      proxy = Gio.DBusProxy.new_for_bus_finish(result)
      proxy.connect("g-properties-changed", () => {
        notificationsPaused.set(Boolean(property("paused")))
        recount()
        refresh()
      })
      proxy.connect("g-signal", refresh)
      proxy.connect("notify::g-name-owner", refresh)
      refresh()
    } catch (error) { console.error("Servicio de avisos:", error) }
  })

export function markNotificationsRead() {
  const keys = notificationHistory.get().map(notice => notice.key)
  if (keys.some(key => !seen.has(key))) {
    seen = new Set(keys)
    try {
      GLib.mkdir_with_parents(directory, 0o700)
      readFile.replace_contents(new TextEncoder().encode(JSON.stringify(keys)), null, false, Gio.FileCreateFlags.PRIVATE, null)
    } catch (error) { console.error("No se pudo guardar el estado de avisos:", error) }
  }
  recount()
}

export async function setNotificationsPaused(paused: boolean) {
  await call("org.freedesktop.DBus.Properties.Set", new GLib.Variant("(ssv)", [
    "org.dunstproject.cmd0", "paused", new GLib.Variant("b", paused),
  ]))
}
