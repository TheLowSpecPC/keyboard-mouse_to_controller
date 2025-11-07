#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "tusb.h"

#define TU_BIT(n) (1UL << (n))
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

hid_gamepad_report_t kbd_to_con(hid_keyboard_report_t const *report) {
    hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

    for(uint8_t i=0; i<6; i++)
    {
        uint8_t keycode = report->keycode[i];
        if ( keycode )
        {
            if(keycode == HID_KEY_W) gamepad.y = -127;
            if(keycode == HID_KEY_S) gamepad.y = 127;
            if(keycode == HID_KEY_A) gamepad.x = -127;
            if(keycode == HID_KEY_D) gamepad.x = 127;
            if(keycode == HID_KEY_SPACE) gamepad.buttons |= TU_BIT(0); // A
            if(keycode == HID_KEY_R) gamepad.buttons |= TU_BIT(2); // X
        }
    }
    if(report->modifier & KEYBOARD_MODIFIER_LEFTCTRL) gamepad.buttons |= TU_BIT(1); //B
    if(report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT) gamepad.buttons |= TU_BIT(10); //THUMB L

    return gamepad;
}

hid_gamepad_report_t mouse_to_con(hid_mouse_report_t const *report) {
    hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

    if(report->x > 10) gamepad.z = 127;
    if(report->x < -10) gamepad.z = -127;
    if(report->y > 10) gamepad.rx = 127;
    if(report->y < -10) gamepad.rx = -127;

    if(report->buttons & MOUSE_BUTTON_LEFT) gamepad.buttons |= TU_BIT(7); // RT
    if(report->buttons & MOUSE_BUTTON_RIGHT) gamepad.buttons |= TU_BIT(6); // LT

    return gamepad;
}