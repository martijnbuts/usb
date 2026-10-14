# USB mouse and keyboard for LVGL on the ESP32-P4 (ESPHome)

A USB mouse and keyboard as input for an ESPHome LVGL screen on the ESP32-P4, on **upstream
ESPHome-dev** with ESP-IDF 6.1.

Tested on the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** (Type-A USB 2.0 OTG High-Speed port),
ESPHome 2026.11.0-dev, ESP-IDF 6.1.0, LVGL 9.6.

## What you get

- **Mouse:** a cursor (white dot) on top of everything. The left button taps, the scroll wheel
  scrolls whatever is under the cursor, and the cursor hides after `hide_after` without motion.
- **Keyboard:** Tab / Shift+Tab and the arrow keys move the focus over the widgets that have an
  action, Enter presses, Esc is back, and typed text (printable ASCII) goes into a focused text
  area. Arrows go to the widget itself while it is in edit mode (a slider after Enter).
- Mouse and keyboard activity counts as a touch for LVGL inactivity (screen savers, night mode).
- Several devices at once (a combo receiver, or a hub with `enable_hubs: true`).

## How it fits together

| Part | From | Does |
|---|---|---|
| `usb_host`, `usb_hid`, `hid` | [p1ngb4ck/esphome](https://github.com/p1ngb4ck/esphome) (branch `testing_dev`) | USB host with hubs, HID report parsing, keys, typed text (keyboard layouts), mouse motion |
| `lvgl_usb_hid` | this repo | turns that into LVGL input devices: a pointer with a cursor and a keypad with a focus group |

The fork connects HID to LVGL inside its own `lvgl` component (`hid_pointers` / `hid_keyboards`).
Using that would replace upstream's LVGL (version, PPA draw settings). `lvgl_usb_hid` does the
same on top of upstream's `lvgl`. The cursor code is adapted from the fork's
`LVHIDPointerListener`.

`lvgl_usb_hid` also carries a small compatibility fix: the fork's `usb_host` calls
`esp32.add_idf_component(override_path=...)` for its patched `espressif/usb` copy, a parameter
only the fork's `esp32` component has. It is translated to the regular `path=`.

## Usage

Copy [`example/usb_input.yaml`](example/usb_input.yaml) next to your config and include it as a
package. Your device needs `lvgl` and ESP-IDF 6.1:

```yaml
esp32:
  framework:
    type: esp-idf
    version: 6.1.0

packages:
  usb: !include usb_input.yaml
```

Options per entry:

```yaml
lvgl_usb_hid:
  - hid_id: usb_input_1   # a usb_hid id (any HIDInput)
    pointer: true         # mouse as LVGL pointer with a cursor (default true)
    keyboard: true        # keyboard as LVGL keypad (default true)
    hide_after: 10s       # hide the cursor after this long without motion (default 10s)
```

## Notes

- The keyboard focus is only visible if your LVGL theme or styles draw a focus state
  (`LV_STATE_FOCUS_KEY`).
- The keyboard group is filled with clickable widgets that have an event handler, in screen
  order, and refilled when another screen or a popup on the top layer appears.

## Credits

- `usb_host`, `usb_hid`, `hid` and the cursor approach: [p1ngb4ck/esphome](https://github.com/p1ngb4ck/esphome).
- USB port: Waveshare [ESP32-P4-WIFI6-Touch-LCD-7B wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B).
