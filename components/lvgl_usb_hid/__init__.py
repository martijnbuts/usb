"""USB mouse and keyboard as LVGL input (cursor, click, scroll, keys, typing).

Uses the hid component from the p1ngb4ck ESPHome fork (usb_hid delivers the keys, typed text
and mouse motion). The fork connects those to LVGL inside its own lvgl component
(hid_pointers / hid_keyboards); this component does the same on top of upstream ESPHome's
lvgl, so the rest of LVGL (version, PPA settings) stays untouched. The mouse cursor is adapted
from the fork's LVHIDPointerListener.

It also carries a compatibility fix: the fork's usb_host calls
esp32.add_idf_component(override_path=...) for its patched copy of espressif/usb, a parameter
only the fork's esp32 component has. It is translated here to the regular path= (a local
component of the same name takes precedence over the registry version).
"""

import inspect
import sys

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["lvgl"]
MULTI_CONF = True

CONF_HID_ID = "hid_id"
CONF_POINTER = "pointer"
CONF_KEYBOARD = "keyboard"
CONF_HIDE_AFTER = "hide_after"


_ORIGINAL_ADD_IDF_COMPONENT = esp32.add_idf_component


def _add_idf_component(*, override_path=None, **kwargs):
    if override_path is not None:
        # Local copy: no registry version, the directory itself is the component.
        kwargs.pop("ref", None)
        kwargs["path"] = override_path
    return _ORIGINAL_ADD_IDF_COMPONENT(**kwargs)


def _patch_add_idf_component(config):
    """Runs after every component is loaded (including the fork's usb_host), before to_code."""
    if "override_path" in inspect.signature(_ORIGINAL_ADD_IDF_COMPONENT).parameters:
        return config  # this ESPHome supports override_path itself
    esp32.add_idf_component = _add_idf_component
    # usb_host imports the function by name, so replace it there too. Do not import it here:
    # that could load upstream's usb_host instead of the fork's.
    if (usb_host := sys.modules.get("esphome.components.usb_host")) is not None:
        usb_host.add_idf_component = _add_idf_component
    return config


FINAL_VALIDATE_SCHEMA = _patch_add_idf_component

lvgl_usb_hid_ns = cg.esphome_ns.namespace("lvgl_usb_hid")
LvglUsbHid = lvgl_usb_hid_ns.class_("LvglUsbHid", cg.Component)


def _hid_input(value):
    from esphome.components.hid import HIDInput

    return cv.use_id(HIDInput)(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LvglUsbHid),
        cv.Required(CONF_HID_ID): _hid_input,
        cv.Optional(CONF_POINTER, default=True): cv.boolean,
        cv.Optional(CONF_KEYBOARD, default=True): cv.boolean,
        # The cursor hides when the mouse has not moved this long; the next motion shows it.
        cv.Optional(CONF_HIDE_AFTER, default="10s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hid = await cg.get_variable(config[CONF_HID_ID])
    var = cg.new_Pvariable(config[CONF_ID], hid)
    await cg.register_component(var, config)
    cg.add(var.set_pointer(config[CONF_POINTER]))
    cg.add(var.set_keyboard(config[CONF_KEYBOARD]))
    cg.add(var.set_hide_after(config[CONF_HIDE_AFTER].total_milliseconds))
