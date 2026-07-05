#include "tusb.h"

void kbd_to_con(hid_keyboard_report_t const *report);
void mouse_to_con(hid_mouse_report_t const *report);