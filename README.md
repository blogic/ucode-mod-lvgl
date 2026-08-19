# ucode-mode-lvgl

An LVGL binding for [ucode](https://github.com/jow-/ucode), so a user interface
for a small DRM display can be written in ucode rather than in C. It builds one
file, lv.so, which ucode loads as the module lv.

It carries the widgets, the DRM backend and the evdev input, and nothing that
belongs to any particular panel: type and artwork are loaded at run time from
files the application ships, with lv.font_load() and lv.image_load().

## What it gives ucode

* Widgets: object, label, bar, chart, arc, line, tileview, image, qrcode. Each
  answers a handle carrying the methods in widget_fns[].
* A DRM display and an evdev pointer, both opened by path.
* lv.font_load(path) for an LVGL binary font, and lv.image_load(path, alpha)
  for a PNG as RGB565 or as an A8 coverage mask. Both answer an index and
  remember the paths they have already been given.
* lv.timer_handler() to drive from a uloop timer, and lv.refresh(), which
  draws and then waits for the panel to take the frame.
* lv.screenshot(path), so a page can be looked at without standing in front
  of it.

Every function a caller can reach carries a kernel-doc header in the source.

## Packaging

The OpenWrt package is ucode-mod-lvgl in
[feed-blogic](https://github.com/blogic/feed-blogic). It fetches LVGL, applies
the DRM patch, and builds this repository inside it.
