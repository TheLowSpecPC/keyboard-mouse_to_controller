#include "tusb.h"

hid_gamepad_report_t kbd_to_con(hid_keyboard_report_t const *report);
hid_gamepad_report_t mouse_to_con(hid_mouse_report_t const *report);