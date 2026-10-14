#include "lvgl_usb_hid.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::lvgl_usb_hid {

static const char *const TAG = "lvgl_usb_hid";

// HID usage tables, page 0x07 (keyboard)
static constexpr uint16_t PAGE_KEYBOARD = 0x07;
static constexpr uint16_t KEY_ENTER = 0x28;
static constexpr uint16_t KEY_ESCAPE = 0x29;
static constexpr uint16_t KEY_BACKSPACE = 0x2A;
static constexpr uint16_t KEY_TAB = 0x2B;
static constexpr uint16_t KEY_HOME = 0x4A;
static constexpr uint16_t KEY_DELETE = 0x4C;
static constexpr uint16_t KEY_END = 0x4D;
static constexpr uint16_t KEY_RIGHT = 0x4F;
static constexpr uint16_t KEY_LEFT = 0x50;
static constexpr uint16_t KEY_DOWN = 0x51;
static constexpr uint16_t KEY_UP = 0x52;
static constexpr uint16_t KEY_KP_ENTER = 0x58;
static constexpr uint8_t SHIFT_MASK = hid::MOD_LEFT_SHIFT | hid::MOD_RIGHT_SHIFT;

void LvglUsbHid::setup() {
  if (this->pointer_)
    this->setup_pointer_();
  if (this->keyboard_)
    this->setup_keyboard_();
}

void LvglUsbHid::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LVGL USB input:\n"
                "  Mouse: %s (hide cursor after %u ms)\n"
                "  Keyboard: %s",
                YESNO(this->pointer_), (unsigned) this->hide_after_, YESNO(this->keyboard_));
}

// ─── Mouse ────────────────────────────────────────────────────────────────────────────────────

void LvglUsbHid::setup_pointer_() {
  lv_display_t *disp = lv_display_get_default();
  this->x_ = lv_display_get_horizontal_resolution(disp) / 2;
  this->y_ = lv_display_get_vertical_resolution(disp) / 2;

  this->pointer_drv_ = lv_indev_create();
  lv_indev_set_type(this->pointer_drv_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(this->pointer_drv_, disp);
  lv_indev_set_user_data(this->pointer_drv_, this);
  lv_indev_set_read_cb(this->pointer_drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *self = static_cast<LvglUsbHid *>(lv_indev_get_user_data(d));
    data->point.x = self->x_;
    data->point.y = self->y_;
    data->state = self->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (self->hide_after_ != 0 && self->cursor_ != nullptr && !self->pressed_ &&
        millis() - self->last_motion_ > self->hide_after_ && !lv_obj_is_hidden(self->cursor_))
      lv_obj_set_hidden(self->cursor_, true);
  });

  // Default cursor: a white dot with a black border, on the system layer (above everything).
  this->cursor_ = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(this->cursor_);
  lv_obj_set_size(this->cursor_, 14, 14);
  lv_obj_set_style_radius(this->cursor_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(this->cursor_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(this->cursor_, lv_color_white(), 0);
  lv_obj_set_style_border_width(this->cursor_, 2, 0);
  lv_obj_set_style_border_color(this->cursor_, lv_color_black(), 0);
  lv_obj_set_clickable(this->cursor_, false);
  lv_indev_set_cursor(this->pointer_drv_, this->cursor_);
  lv_obj_set_hidden(this->cursor_, true);  // until the mouse first moves

  this->input_->add_on_pointer_callback([this](int16_t dx, int16_t dy, int16_t wheel, int16_t pan,
                                               uint8_t buttons) { this->move_(dx, dy, wheel, pan, buttons); });
}

void LvglUsbHid::move_(int16_t dx, int16_t dy, int16_t wheel, int16_t pan, uint8_t buttons) {
  this->last_motion_ = millis();
  if (this->cursor_ != nullptr && lv_obj_is_hidden(this->cursor_))
    lv_obj_set_hidden(this->cursor_, false);
  lv_display_t *disp = lv_display_get_default();
  this->x_ = std::clamp<int32_t>(this->x_ + dx, 0, lv_display_get_horizontal_resolution(disp) - 1);
  this->y_ = std::clamp<int32_t>(this->y_ + dy, 0, lv_display_get_vertical_resolution(disp) - 1);
  this->pressed_ = (buttons & 0x01) != 0;
  lv_display_trigger_activity(disp);  // counts as a touch for inactivity / night mode
  if (wheel == 0 && pan == 0)
    return;
  lv_point_t point{this->x_, this->y_};
  for (lv_obj_t *obj = lv_indev_search_obj(lv_screen_active(), &point); obj != nullptr;
       obj = lv_obj_get_parent(obj)) {
    if (lv_obj_is_scrollable(obj)) {
      // One wheel step scrolls about a line of text.
      static constexpr int32_t STEP = 40;
      lv_obj_scroll_by_bounded(obj, -pan * STEP, wheel * STEP, LV_ANIM_ON);
      break;
    }
  }
}

// ─── Keyboard ─────────────────────────────────────────────────────────────────────────────────

void LvglUsbHid::setup_keyboard_() {
  lv_display_t *disp = lv_display_get_default();
  this->group_ = lv_group_create();
  this->key_drv_ = lv_indev_create();
  lv_indev_set_type(this->key_drv_, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_display(this->key_drv_, disp);
  lv_indev_set_user_data(this->key_drv_, this);
  lv_indev_set_group(this->key_drv_, this->group_);
  lv_indev_set_read_cb(this->key_drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *self = static_cast<LvglUsbHid *>(lv_indev_get_user_data(d));
    if (self->head_ == self->tail_) {
      data->key = self->last_key_;
      data->state = self->held_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
      return;
    }
    const KeyEvent ev = self->queue_[self->head_];
    self->head_ = (self->head_ + 1) % QUEUE_SIZE;
    self->last_key_ = ev.key;
    self->held_ = ev.pressed;
    data->key = ev.key;
    data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = self->head_ != self->tail_;
  });

  // ESPHome's widgets are in no group: the clickable widgets (those with an action of their own)
  // go into the group, again whenever another screen or a popup appears.
  this->fill_group_();

  this->input_->add_on_key_event_callback(
      [this](uint16_t page, uint16_t usage, bool pressed) { this->key_(page, usage, pressed); });
  this->input_->add_on_text_callback([this](const std::string &text) { this->text_(text); });
}

void LvglUsbHid::add_tree_(lv_group_t *group, lv_obj_t *obj) {
  const uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; i++) {
    lv_obj_t *child = lv_obj_get_child(obj, i);
    // Only what does something: clickable and with an event handler (on_click, on_value, ...).
    if (lv_obj_is_clickable(child) && lv_obj_get_event_count(child) > 0 &&
        lv_obj_get_group(child) == nullptr)
      lv_group_add_obj(group, child);
    add_tree_(group, child);
  }
}

void LvglUsbHid::refresh_group_() {
  lv_obj_t *screen = lv_screen_active();
  const uint32_t top = lv_obj_get_child_count(lv_layer_top());
  if (screen != this->filled_screen_ || top != this->filled_top_count_)
    this->fill_group_();
}

void LvglUsbHid::fill_group_() {
  this->filled_screen_ = lv_screen_active();
  this->filled_top_count_ = lv_obj_get_child_count(lv_layer_top());
  lv_group_remove_all_objs(this->group_);
  add_tree_(this->group_, lv_screen_active());
  add_tree_(this->group_, lv_layer_top());
  ESP_LOGD(TAG, "%u widgets reachable with the keyboard", (unsigned) lv_group_get_obj_count(this->group_));
}

void LvglUsbHid::push_(uint32_t key, bool pressed) {
  const uint8_t next = (this->tail_ + 1) % QUEUE_SIZE;
  if (next == this->head_)
    return;  // full: this key is dropped
  this->queue_[this->tail_] = KeyEvent{key, pressed};
  this->tail_ = next;
}

void LvglUsbHid::key_(uint16_t page, uint16_t usage, bool pressed) {
  if (page != PAGE_KEYBOARD)
    return;
  lv_display_trigger_activity(lv_display_get_default());
  this->refresh_group_();
  uint32_t key;
  switch (usage) {
    case KEY_ENTER:
    case KEY_KP_ENTER:
      key = LV_KEY_ENTER;
      break;
    case KEY_ESCAPE:
      key = LV_KEY_ESC;
      break;
    case KEY_BACKSPACE:
      key = LV_KEY_BACKSPACE;
      break;
    case KEY_TAB:
      key = (this->input_->get_modifiers() & SHIFT_MASK) ? LV_KEY_PREV : LV_KEY_NEXT;
      break;
    case KEY_HOME:
      key = LV_KEY_HOME;
      break;
    case KEY_END:
      key = LV_KEY_END;
      break;
    case KEY_DELETE:
      key = LV_KEY_DEL;
      break;
    case KEY_RIGHT:
      key = LV_KEY_RIGHT;
      break;
    case KEY_LEFT:
      key = LV_KEY_LEFT;
      break;
    case KEY_DOWN:
      key = LV_KEY_DOWN;
      break;
    case KEY_UP:
      key = LV_KEY_UP;
      break;
    default:
      return;  // ordinary characters come through text_()
  }
  // The arrows move the focus, unless the focused widget uses them itself (a slider or text
  // area in edit mode: Enter toggles that mode).
  if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN || key == LV_KEY_LEFT || key == LV_KEY_UP) {
    if (!lv_group_get_editing(this->group_))
      key = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) ? LV_KEY_NEXT : LV_KEY_PREV;
  }
  this->push_(key, pressed);
}

void LvglUsbHid::text_(const std::string &text) {
  // Printable ASCII only; Enter, Backspace and Tab go through key_().
  this->refresh_group_();
  for (const char c : text) {
    if (c < 0x20 || c > 0x7E)
      continue;
    this->push_(static_cast<uint32_t>(c), true);
    this->push_(static_cast<uint32_t>(c), false);
  }
}

}  // namespace esphome::lvgl_usb_hid
