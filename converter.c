#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "tusb.h"

#define TU_BIT(n) (1UL << (n))
hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

bool check(uint8_t c, uint8_t arr[]){
    for(uint8_t i=0; i<6; i++)
    {
        if(arr[i] == c) return true;
    }
    return false;
}

hid_gamepad_report_t kbd_to_con(hid_keyboard_report_t const *report) {

    bool w = check(HID_KEY_W, report->keycode);
    bool s = check(HID_KEY_S, report->keycode);
    bool a = check(HID_KEY_A, report->keycode);
    bool d = check(HID_KEY_D, report->keycode);

    gamepad.y = (w == s) ? 0 : (w ? -127 : 127);
    gamepad.x = (a == d) ? 0 : (a ? -127 : 127);

    gamepad.buttons = check(HID_KEY_SPACE, report->keycode) ? gamepad.buttons | TU_BIT(0) : gamepad.buttons & ~TU_BIT(0); // A
    gamepad.buttons = check(HID_KEY_R, report->keycode) ? gamepad.buttons | TU_BIT(2) : gamepad.buttons & ~TU_BIT(2); // X
    gamepad.buttons = check(HID_KEY_E, report->keycode) ? gamepad.buttons | TU_BIT(3) : gamepad.buttons & ~TU_BIT(3); // Y

    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTCTRL ? gamepad.buttons | TU_BIT(1) : gamepad.buttons & ~TU_BIT(1); //B
    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT ? gamepad.buttons | TU_BIT(10) : gamepad.buttons & ~TU_BIT(10); //THUMB L

    return gamepad;
}

hid_gamepad_report_t mouse_to_con(hid_mouse_report_t const *report) {

    int8_t x = report->x;
    int8_t y = report->y;

    // X axis
    if(report->x > 0) gamepad.z = 194.4927 + (0.02418395 - 194.4927)/(1 + pow((x/64.94899), 0.957923));
    else if(report->x < 0) gamepad.z = -(194.4927 + (0.02418395 - 194.4927)/(1 + pow((-1*x/64.94899), 0.957923)));
    else gamepad.z = 0;

    // Y axis
    if(report->y > 0) gamepad.rx = 194.4927 + (0.02418395 - 194.4927)/(1 + pow((y/64.94899), 0.957923));
    else if(report->y < 0) gamepad.rx = -(194.4927 + (0.02418395 - 194.4927)/(1 + pow((-1*y/64.94899), 0.957923)));
    else gamepad.rx = 0;

    gamepad.buttons = report->buttons & MOUSE_BUTTON_LEFT ? gamepad.buttons | TU_BIT(7) : gamepad.buttons & ~TU_BIT(7); // RT
    gamepad.buttons = report->buttons & MOUSE_BUTTON_RIGHT ? gamepad.buttons | TU_BIT(6) : gamepad.buttons & ~TU_BIT(6); // LT

    return gamepad;
}