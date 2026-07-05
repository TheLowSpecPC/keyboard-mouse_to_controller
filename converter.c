#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "tusb.h"
#include "usb_descriptors.h"
#include "converter.h"

#define TU_BIT(n) (1UL << (n))
hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

// Helper function to check if a key is pressed in current report
bool check(uint8_t c, uint8_t arr[]){
    for(uint8_t i=0; i<6; i++)
    {
        if(arr[i] == c) return true;
    }
    return false;
}

// convert hid keyboard report to hid gamepad report
void kbd_to_con(hid_keyboard_report_t const *report) {

    bool w = check(HID_KEY_W, report->keycode);
    bool s = check(HID_KEY_S, report->keycode);
    bool a = check(HID_KEY_A, report->keycode);
    bool d = check(HID_KEY_D, report->keycode);

    // Left Stick
    gamepad.y = (w == s) ? 0 : (w ? -127 : 127);
    gamepad.x = (a == d) ? 0 : (a ? -127 : 127);

    // Buttons
    gamepad.buttons = check(HID_KEY_SPACE, report->keycode) ? gamepad.buttons | TU_BIT(0) : gamepad.buttons & ~TU_BIT(0); // A
    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTCTRL ? gamepad.buttons | TU_BIT(1) : gamepad.buttons & ~TU_BIT(1); //B
    gamepad.buttons = check(HID_KEY_R, report->keycode) ? gamepad.buttons | TU_BIT(2) : gamepad.buttons & ~TU_BIT(2); // X
    gamepad.buttons = check(HID_KEY_E, report->keycode) ? gamepad.buttons | TU_BIT(3) : gamepad.buttons & ~TU_BIT(3); // Y
    gamepad.buttons = check(HID_KEY_ESCAPE, report->keycode) ? gamepad.buttons | TU_BIT(9) : gamepad.buttons & ~TU_BIT(9); // Start
    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT ? gamepad.buttons | TU_BIT(10) : gamepad.buttons & ~TU_BIT(10); //THUMB L
    gamepad.buttons = check(HID_KEY_V, report->keycode) ? gamepad.buttons | TU_BIT(11) : gamepad.buttons & ~TU_BIT(11); // Thumb R
    gamepad.buttons = check(HID_KEY_1, report->keycode) ? gamepad.buttons | TU_BIT(12) : gamepad.buttons & ~TU_BIT(12); // D-up
    gamepad.buttons = check(HID_KEY_3, report->keycode) ? gamepad.buttons | TU_BIT(13) : gamepad.buttons & ~TU_BIT(13); // D-down
    gamepad.buttons = check(HID_KEY_2, report->keycode) ? gamepad.buttons | TU_BIT(14) : gamepad.buttons & ~TU_BIT(14); // D-left
    gamepad.buttons = check(HID_KEY_4, report->keycode) ? gamepad.buttons | TU_BIT(15) : gamepad.buttons & ~TU_BIT(15); // D-right
    gamepad.buttons = check(HID_KEY_DELETE,  report->keycode) ? gamepad.buttons | TU_BIT(16) : gamepad.buttons & ~TU_BIT(16); // Home
}

// convert hid mouse report to hid gamepad report
void mouse_to_con(hid_mouse_report_t const *report) {

    int8_t x = report->x;
    int8_t y = report->y;

    // Right Stick X axis
    if(x > 0) gamepad.z = 127.3119 - 128.3597*exp(-0.04554843*x);
    else if(x < 0) gamepad.z = -(127.3119 - 128.3597*exp(0.04554843*x));
    else gamepad.z = 0;

    // Right Stick Y axis
    if(y > 0) gamepad.rx = 127.3119 - 128.3597*exp(-0.04554843*y);
    else if(y < 0) gamepad.rx = -(127.3119 - 128.3597*exp(0.04554843*y));
    else gamepad.rx = 0;

    // Buttons
    gamepad.buttons = report->buttons & MOUSE_BUTTON_LEFT ? gamepad.buttons | TU_BIT(7) : gamepad.buttons & ~TU_BIT(7); // RT
    gamepad.buttons = report->buttons & MOUSE_BUTTON_RIGHT ? gamepad.buttons | TU_BIT(6) : gamepad.buttons & ~TU_BIT(6); // LT
    gamepad.buttons = report->buttons & MOUSE_BUTTON_MIDDLE ? gamepad.buttons | TU_BIT(4) : gamepad.buttons & ~TU_BIT(4); // LB
    gamepad.buttons = report->buttons & MOUSE_BUTTON_FORWARD ? gamepad.buttons | TU_BIT(5) : gamepad.buttons & ~TU_BIT(5); // RB
    gamepad.buttons = report->buttons & MOUSE_BUTTON_BACKWARD ? gamepad.buttons | TU_BIT(12) : gamepad.buttons & ~TU_BIT(12); // D-up
}