import { Gtk } from "astal/gtk3"
import GLib from "gi://GLib"
import Variable from "astal/variable"

export const selectedModule = Variable<Gtk.Button | null>(null)

export function observe<T>(widget: Gtk.Widget, source: Variable<T>, update: () => void) {
  const unsubscribe = source.subscribe(update)
  widget.connect("destroy", unsubscribe)
  update()
}

export function smoothLabel(source: Variable<string>, width: number, xalign = 0.5) {
  const stack = new Gtk.Stack({
    width_request: width, homogeneous: true,
    transition_type: Gtk.StackTransitionType.CROSSFADE, transition_duration: 140,
  })
  const labels = [0, 1].map(() => new Gtk.Label({ xalign, ellipsize: 3, single_line_mode: true }))
  labels.forEach(label => { stack.add(label); label.show() })
  let current = 0
  let text = source.get()
  labels[0].label = text
  stack.visible_child = labels[0]
  const unsubscribe = source.subscribe(value => {
    if (value === text) return
    text = value
    current = 1 - current
    labels[current].label = value
    stack.visible_child = labels[current]
  })
  stack.connect("destroy", unsubscribe)
  stack.show_all()
  return stack
}

export function activeIndicator(pill: Gtk.Box) {
  let current = [0, 0, 0, 0, 0]
  let from = current.slice(), target = current.slice(), started = 0, tick = 0
  const update = () => {
    const button = selectedModule.get()
    target = current.slice()
    if (button) {
      const [valid, x, y] = button.translate_coordinates(pill, 0, 0)
      if (!valid) return
      target = [x + 1, y + 1, button.get_allocated_width() - 2, button.get_allocated_height() - 2, 1]
      if (!tick && current[4] === 0) current = [...target.slice(0, 4), 0]
    } else target[4] = 0
    if (target.every((value, index) => Math.abs(value - current[index]) < 0.001)) return
    from = current.slice()
    started = GLib.get_monotonic_time()
    if (!Gtk.Settings.get_default()?.gtk_enable_animations || !pill.get_mapped()) {
      if (tick) { pill.remove_tick_callback(tick); tick = 0 }
      current = target.slice()
      pill.queue_draw()
      return
    }
    if (tick) return
    tick = pill.add_tick_callback((_self, clock) => {
      const progress = Math.max(0, Math.min(1, (clock.get_frame_time() - started) / 200000))
      const ease = 1 - (1 - progress) ** 3
      current = from.map((value, index) => value + (target[index] - value) * ease)
      pill.queue_draw()
      if (progress < 1) return true
      tick = 0
      return false
    })
  }
  pill.connect("draw", (_self, cr) => {
    const context = pill.get_style_context()
    const margin = context.get_margin(context.get_state())
    const borderWidth = pill.get_allocated_width() - margin.left - margin.right
    const borderHeight = pill.get_allocated_height() - margin.top - margin.bottom
    Gtk.render_background(context, cr, margin.left, margin.top, borderWidth, borderHeight)
    Gtk.render_frame(context, cr, margin.left, margin.top, borderWidth, borderHeight)
    const [x, y, width, height, opacity] = current
    if (opacity > 0 && width > 0 && height > 0) {
      const r = Math.min(height, width) / 2
      cr.newSubPath()
      cr.arc(x + width - r, y + r, r, -Math.PI / 2, 0)
      cr.arc(x + width - r, y + height - r, r, 0, Math.PI / 2)
      cr.arc(x + r, y + height - r, r, Math.PI / 2, Math.PI)
      cr.arc(x + r, y + r, r, Math.PI, Math.PI * 1.5)
      cr.closePath()
      cr.setSourceRGBA(159 / 255, 196 / 255, 1, 0.19 * opacity)
      cr.fillPreserve()
      cr.setSourceRGBA(159 / 255, 196 / 255, 1, 0.12 * opacity)
      cr.setLineWidth(1)
      cr.stroke()
    }
    pill.get_children().forEach(child => pill.propagate_draw(child, cr))
    return true
  })
  const unsubscribe = selectedModule.subscribe(update)
  pill.connect("size-allocate", () => { if (selectedModule.get() && !tick) update() })
  pill.connect("destroy", () => {
    unsubscribe()
    if (tick) pill.remove_tick_callback(tick)
  })
}
