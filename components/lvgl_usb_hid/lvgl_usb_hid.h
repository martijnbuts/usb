#pragma once

// USB mouse and keyboard as LVGL input. See __init__.py.
#include "esphome/core/component.h"
#include "esphome/components/hid/hid_input.h"

#include <lvgl.h>

#include <string>

namespace esphome::lvgl_usb_hid {

class LvglUsbHid : public Component {
 public:
  explicit LvglUsbHid(hid::HIDInput *input) : input_(input) {}
  void set_pointer(bool on) { this->pointer_ = on; }
  void set_keyboard(bool on) { this->keyboard_ = on; }
  void set_hide_after(uint32_t ms) { this->hide_after_ = ms; }

  void setup() override;
  void dump_config() override;
  // After LVGL: the display and the screens exist by then.
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  // Mouse: motion moves the cursor, the left button presses, the wheel scrolls what is under it.
  void setup_pointer_();
  void move_(int16_t dx, int16_t dy, int16_t wheel, int16_t pan, uint8_t buttons);

  // Keyboard: Tab/Shift+Tab and the arrows move the focus, Enter presses, Esc is back,
  // typed text goes to a focused text area.
  void setup_keyboard_();
  void key_(uint16_t page, uint16_t usage, bool pressed);
  void text_(const std::string &text);
  void push_(uint32_t key, bool pressed);
  void fill_group_();
  void refresh_group_();
  static void add_tree_(lv_group_t *group, lv_obj_t *obj);

  hid::HIDInput *input_;
  bool pointer_{true};
  bool keyboard_{true};
  uint32_t hide_after_{0};

  lv_indev_t *pointer_drv_{nullptr};
  lv_obj_t *cursor_{nullptr};
  int32_t x_{0};
  int32_t y_{0};
  bool pressed_{false};
  uint32_t last_motion_{0};

  // Keys arrive on the main loop (usb_hid uses defer) and LVGL reads them there too: a plain
  // queue without locks, so a quick press and release between two reads is kept.
  struct KeyEvent {
    uint32_t key;
    bool pressed;
  };
  static constexpr uint8_t QUEUE_SIZE = 32;
  KeyEvent queue_[QUEUE_SIZE]{};
  uint8_t head_{0};
  uint8_t tail_{0};
  uint32_t last_key_{0};
  bool held_{false};
  lv_indev_t *key_drv_{nullptr};
  lv_group_t *group_{nullptr};
  lv_obj_t *filled_screen_{nullptr};
  uint32_t filled_top_count_{0};
};

}  // namespace esphome::lvgl_usb_hid
