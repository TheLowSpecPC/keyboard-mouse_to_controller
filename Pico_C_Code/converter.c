#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/time.h"

#include "tusb.h"
#include "usb_descriptors.h"
#include "converter.h"
#include "kvstore.h"

void keyConfig(uint8_t final[36][2]);

#define TU_BIT(n) (1UL << (n))
hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

absolute_time_t lastTime = 0;
int8_t lastX, lastY = 0;

uint8_t Keys[36][2] = {0};
void init_converter(void) {
    keyConfig(Keys);
}

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
    /*for(int i = 0; i < 32; i++){
        if(Keys[i][1] == 1){
            gamepad.buttons = check(Keys[i][0], report->keycode) ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
        else if(Keys[i][1] == 2){
            gamepad.buttons = report->modifier & Keys[i][0] ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
    }

    bool up = (Keys[32][1] == 1) ? check(Keys[32][0], report->keycode) : (Keys[32][1] == 2) ? report->modifier & Keys[32][0] : false;
    bool down = (Keys[33][1] == 1) ? check(Keys[33][0], report->keycode) : (Keys[33][1] == 2) ? report->modifier & Keys[33][0] : false;
    bool left = (Keys[34][1] == 1) ? check(Keys[34][0], report->keycode) : (Keys[34][1] == 2) ? report->modifier & Keys[34][0] : false;
    bool right = (Keys[35][1] == 1) ? check(Keys[35][0], report->keycode) : (Keys[35][1] == 2) ? report->modifier & Keys[35][0] : false;*/

    bool up = check(HID_KEY_W, report->keycode);
    bool down = check(HID_KEY_S, report->keycode);
    bool left = check(HID_KEY_A, report->keycode);
    bool right = check(HID_KEY_D, report->keycode);

    // Left Stick
    gamepad.y = (up == down) ? 0 : (up ? -127 : 127);
    gamepad.x = (left == right) ? 0 : (left ? -127 : 127);

    // Buttons
    gamepad.buttons = check(HID_KEY_SPACE, report->keycode) ? gamepad.buttons | TU_BIT(0) : gamepad.buttons & ~TU_BIT(0); // A
    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTCTRL ? gamepad.buttons | TU_BIT(1) : gamepad.buttons & ~TU_BIT(1); //B
    gamepad.buttons = check(HID_KEY_R, report->keycode) ? gamepad.buttons | TU_BIT(2) : gamepad.buttons & ~TU_BIT(2); // X
    gamepad.buttons = check(HID_KEY_E, report->keycode) ? gamepad.buttons | TU_BIT(3) : gamepad.buttons & ~TU_BIT(3); // Y
    gamepad.buttons = check(HID_KEY_ESCAPE, report->keycode) ? gamepad.buttons | TU_BIT(9) : gamepad.buttons & ~TU_BIT(9); // Start
    gamepad.buttons = report->modifier & KEYBOARD_MODIFIER_LEFTSHIFT ? gamepad.buttons | TU_BIT(10) : gamepad.buttons & ~TU_BIT(10); //Thumb L
    gamepad.buttons = check(HID_KEY_V, report->keycode) ? gamepad.buttons | TU_BIT(11) : gamepad.buttons & ~TU_BIT(11); // Thumb R
    gamepad.buttons = check(HID_KEY_1, report->keycode) ? gamepad.buttons | TU_BIT(12) : gamepad.buttons & ~TU_BIT(12); // D-up
    gamepad.buttons = check(HID_KEY_3, report->keycode) ? gamepad.buttons | TU_BIT(13) : gamepad.buttons & ~TU_BIT(13); // D-down
    gamepad.buttons = check(HID_KEY_2, report->keycode) ? gamepad.buttons | TU_BIT(14) : gamepad.buttons & ~TU_BIT(14); // D-left
    gamepad.buttons = check(HID_KEY_4, report->keycode) ? gamepad.buttons | TU_BIT(15) : gamepad.buttons & ~TU_BIT(15); // D-right
    gamepad.buttons = check(HID_KEY_DELETE,  report->keycode) ? gamepad.buttons | TU_BIT(16) : gamepad.buttons & ~TU_BIT(16); // Home

    tud_cdc_write(Keys, sizeof(Keys)); 
    tud_cdc_write_flush();
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

    /*for(int i = 0; i < 32; i++){
        if(Keys[i][1] == 3){
            gamepad.buttons = report->buttons & Keys[i][0] ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
    }*/

}

void converter_task(){
    absolute_time_t now = get_absolute_time();

    if ((now - lastTime) >= 50000){ //50ms
      lastTime = now; // Reset 

      if(gamepad.z == lastX && gamepad.rx == lastY){
        gamepad.z = 0;
        gamepad.rx = 0;
      }
      
      lastX = gamepad.z;
      lastY = gamepad.rx;
    }

    tud_hid_gamepad_report(REPORT_ID_GAMEPAD, gamepad.x, gamepad.y, gamepad.z, gamepad.rz, gamepad.rx, gamepad.ry, gamepad.hat, gamepad.buttons);
}

void keyConfig(uint8_t final[36][2]){
    uint8_t ascii[36]={0}, modifier[36]={0}, mouse[36]={0};
    size_t ascii_size = sizeof(ascii), modifier_size = sizeof(modifier), mouse_size = sizeof(mouse);

    kvs_get("ascii", ascii, sizeof(ascii), &ascii_size);
    kvs_get("modifier", modifier, sizeof(modifier), &modifier_size);
    kvs_get("mouse", mouse, sizeof(mouse), &mouse_size);

    for(int i=0; i<36; i++){
        if(ascii[i] != 0){
            final[i][0] = ascii[i];
            final[i][1] = 1;
        }
        else if(modifier[i] != 0){
            final[i][0] = modifier[i];
            final[i][1] = 2;
        }
        else if(mouse[i] != 0){
            final[i][0] = mouse[i];
            final[i][1] = 3;
        }
        else{
            final[i][0] = 0;
            final[i][1] = 0;
        }
    }
}