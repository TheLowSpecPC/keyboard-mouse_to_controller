#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "tusb.h"
#include "usb_descriptors.h"
#include "converter.h"

void keyConfig(uint8_t final[38][2]);

// Calculate an offset at the end of the Pico's Flash memory
// PICO_FLASH_SIZE_BYTES is typically 2MB (2 * 1024 * 1024)
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define TU_BIT(n) (1UL << (n))
hid_gamepad_report_t gamepad = {0, 0, 0, 0, 0, 0, 0, 0};

absolute_time_t lastTime = 0;
int8_t lastX, lastY = 0;

absolute_time_t rapidTime = 0;
bool rapidFire = false, rapidFireEnable = false;

uint8_t Keys[38][2] = {0};
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


uint8_t n = 0;
bool pushButton(uint8_t now) {
    if(now && n == 0) {
        n = 1;
        return true; // Button was just pressed
    } else if(!now && n == 1) {
        return true; // Button was just released
    }else if(now && n == 1) {
        n = 0;
        return false; // Button is still pressed
    } else if(!now && n == 0) {
        return false; // Button is still released
    }
    return false;
}

// convert hid keyboard report to hid gamepad report
void kbd_to_con(hid_keyboard_report_t const *report) {
    bool up = (Keys[32][1] == 1) ? check(Keys[32][0], report->keycode) : (Keys[32][1] == 2) ? report->modifier & Keys[32][0] : false;
    bool down = (Keys[33][1] == 1) ? check(Keys[33][0], report->keycode) : (Keys[33][1] == 2) ? report->modifier & Keys[33][0] : false;
    bool left = (Keys[34][1] == 1) ? check(Keys[34][0], report->keycode) : (Keys[34][1] == 2) ? report->modifier & Keys[34][0] : false;
    bool right = (Keys[35][1] == 1) ? check(Keys[35][0], report->keycode) : (Keys[35][1] == 2) ? report->modifier & Keys[35][0] : false;

    // Left Stick
    gamepad.y = (up == down) ? 0 : (up ? -127 : 127);
    gamepad.x = (left == right) ? 0 : (left ? -127 : 127);

    bool state = false;
    for(int i = 0; i < 32; i++){
        if(Keys[i][1] == 1){
            gamepad.buttons = check(Keys[i][0], report->keycode) ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
        else if(Keys[i][1] == 2){
            gamepad.buttons = report->modifier & Keys[i][0] ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
    }

    for(int i = 36; i < 38; i++){
        if(Keys[36][1] == 1 || Keys[36][1] == 2 && Keys[36][0] == Keys[i][0] && rapidFireEnable){continue;} // Skip rapid fire button if enabled
        if(Keys[i][1] == 1){
            if(i == 36){rapidFire = check(Keys[i][0], report->keycode);}
            if(i == 37){rapidFireEnable = pushButton(check(Keys[i][0], report->keycode));}
        }
        else if(Keys[i][1] == 2){
            if(i == 36){rapidFire = report->modifier & Keys[i][0];}
            if(i == 37){rapidFireEnable = pushButton(report->modifier & Keys[i][0]);}
        }
    }
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
    for(int i = 0; i < 32; i++){
        if(Keys[36][1] == 3 && Keys[36][0] == Keys[i][0] && rapidFireEnable){continue;} // Skip rapid fire button if enabled
        if(Keys[i][1] == 3){
            gamepad.buttons = report->buttons & Keys[i][0] ? gamepad.buttons | TU_BIT(i) : gamepad.buttons & ~TU_BIT(i);
        }
    }

    if(Keys[36][1] == 3){rapidFire = report->buttons & Keys[36][0];}
    if(Keys[37][1] == 3){rapidFireEnable = pushButton(report->buttons & Keys[37][0]);}

}

void converter_task(){
    absolute_time_t now = get_absolute_time();
    uint32_t cps = Keys[38][1];

    // Reset the right stick to 0 if no movement
    if((now - lastTime) >= 50000){ //50ms
      lastTime = now; // Reset 

      if(gamepad.z == lastX && gamepad.rx == lastY){
        gamepad.z = 0;
        gamepad.rx = 0;
      }
      
      lastX = gamepad.z;
      lastY = gamepad.rx;
    }

    if(rapidFireEnable){
        gpio_put(25, 1);
        if(rapidFire){
            if((now - rapidTime) >= 1000000/cps){
                rapidTime = now;
                gamepad.buttons = gamepad.buttons ^ TU_BIT(Keys[38][0]);
            }
        }
        else{gamepad.buttons = gamepad.buttons & ~TU_BIT(Keys[38][0]);}
    }
    else{
        gpio_put(25, 0);
    }

    tud_hid_gamepad_report(REPORT_ID_GAMEPAD, gamepad.x, gamepad.y, gamepad.z, gamepad.rz, gamepad.rx, gamepad.ry, gamepad.hat, gamepad.buttons);
}

void keyConfig(uint8_t final[39][2]){
    uint8_t config[78]={0};
    size_t config_size = sizeof(config);

    const uint8_t* flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

    memcpy(config, flash_target_contents, config_size);

    for(int i = 0; i < config_size/2; i++){
        final[i][0] = config[i*2];
        final[i][1] = config[i*2 + 1];
    }
}

void saveConfig(uint8_t* config_arr) {
    // 1. Create a 256-byte buffer (minimum write size for Pico flash)
    uint8_t flash_data[FLASH_PAGE_SIZE] = {0}; 
    
    // 2. Pack your arrays into the buffer
    memcpy(flash_data, config_arr, 78);

    // 3. Multicore Safety: Pause Core 0 and disable interrupts
    multicore_lockout_start_blocking(); 
    uint32_t ints = save_and_disable_interrupts();

    // 4. Perform the Flash Erase and Write
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, flash_data, FLASH_PAGE_SIZE);

    // 5. Restore system execution
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
}