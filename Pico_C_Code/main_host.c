#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

#include "pio_usb.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "converter.h"



//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+



static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

#define BUFFER_SIZE 80
static uint8_t rx_buffer[BUFFER_SIZE];
static uint32_t rx_index = 0;

/*------------- MAIN -------------*/

// core1: handle host events
void core1_main() {
  sleep_ms(10);

  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = {
    .pin_dp = PIO_USB_DP_PIN_DEFAULT,
    .pio_tx_num = PIO_USB_TX_DEFAULT,
    .sm_tx = PIO_SM_USB_TX_DEFAULT,
    .tx_ch = PIO_USB_DMA_TX_DEFAULT,
    .pio_rx_num = PIO_USB_RX_DEFAULT,
    .sm_rx = PIO_SM_USB_RX_DEFAULT,
    .sm_eop = PIO_SM_USB_EOP_DEFAULT,
    .alarm_pool = NULL, // Set to your alarm pool if you have one, otherwise NULL
    .debug_pin_rx = PIO_USB_DEBUG_PIN_NONE,
    .debug_pin_eop = PIO_USB_DEBUG_PIN_NONE,
    .skip_alarm_pool = false, // Add this line, typically false if using alarm_pool
    .pinout = PIO_USB_PINOUT_DPDM // Add this line, as per newer pio_usb.h
  };
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);

  // Choose two new, unused GPIO pins. D- will be (pin_dp_2 + 1).
  // For example, let's use GPIO 21 and 22.
  const uint pin_dp_2 = 2; // D+ for 2nd port on GPIO 2 (D- will be GPIO 3)

  // Add the second port
  pio_usb_host_add_port(pin_dp_2, PIO_USB_PINOUT_DPDM);

  init_converter(); // Initialize the converter (key mapping, etc.)

  // Initialize the default LED pin (GP25) as an output
  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  
  while (true) {
    tuh_task(); // tinyusb host task
    converter_task();

    if (tud_cdc_available()) {
      // Read new data into the buffer starting at our current index
      uint32_t count = tud_cdc_read(&rx_buffer[rx_index], sizeof(rx_buffer) - rx_index);
      rx_index += count;

      // Process the packet ONLY when we have the full 110 bytes
      // (36 ascii + 36 modifier + 36 mouse + 2 signature = 110)
      while (rx_index >= BUFFER_SIZE) {
        // Check for your end-of-data signature at the expected positions
        if (rx_buffer[0] == 0xFF && rx_buffer[1] == 0xAA) {
          gpio_put(25, 1); // Turn on LED on success

          // Shift elements left by 2 positions
          memmove(&rx_buffer[0], &rx_buffer[2], (BUFFER_SIZE - 2) * sizeof(uint8_t));
          memset(&rx_buffer[BUFFER_SIZE - 2], 0, 2 * sizeof(uint8_t)); // Clear the last 2 bytes

          // Save to memory
          saveConfig(rx_buffer);

          sleep_ms(100); // Give time for the message to be sent before rebooting

          // Reboot to apply changes
          watchdog_enable(1, true);
          while (1) {
            __wfi();
          }
        } 
        else {
          // If we have 110 bytes but the signature doesn't match, we got misaligned garbage data.
          // In a real-world scenario, you might search for 0xFF 0xAA and shift to resync, 
          // but clearing the buffer is the easiest fallback to reset state.
          rx_index = 0; 
          break; 
        }

        // Shift any leftover bytes (if you somehow received >110 bytes at once) to the front
        rx_index -= 110;
        if (rx_index > 0) {
          memmove(rx_buffer, &rx_buffer[110], rx_index);
        }
      }
    }
  }
}

//--------------------------------------------------------------------+
// Host HID
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  char tempbuf[256];
  int count = sprintf(tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", vid, pid, dev_addr, instance, protocol_str[itf_protocol]);

  tud_cdc_write(tempbuf, count);
  tud_cdc_write_flush();

  // Receive report from boot keyboard & mouse only
  // tuh_hid_report_received_cb() will be invoked when report is available
  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE)
  {
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
      tud_cdc_write_str("Error: cannot request report\r\n");
    }
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  char tempbuf[256];
  int count = sprintf(tempbuf, "[%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
  tud_cdc_write(tempbuf, count);
  tud_cdc_write_flush();
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}


// convert hid keycode to ascii and print via usb device CDC (ignore non-printable)
static void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report)
{
  (void) dev_addr;
  bool flush = false;

  for(uint8_t i=0; i<6; i++)
  {
    uint8_t keycode = report->keycode[i];
    if ( keycode )
    {
      bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
      uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];

      if (ch)
      {
        if (ch == '\n') tud_cdc_write("\r", 1);
        tud_cdc_write(&ch, 1);
        flush = true;
      }
    }
    // TODO example skips key released
  }

  if (flush) tud_cdc_write_flush();
}

// send mouse report to usb device CDC
static void process_mouse_report(uint8_t dev_addr, hid_mouse_report_t const * report)
{
  //------------- button state  -------------//
  //uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  char l = report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-';
  char m = report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-';
  char r = report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-';
  char f = report->buttons & MOUSE_BUTTON_FORWARD  ? 'F' : '-';
  char b = report->buttons & MOUSE_BUTTON_BACKWARD  ? 'B' : '-';

  char tempbuf[32];
  int count = sprintf(tempbuf, "[%u] %c%c%c%c%c %d %d %d\r\n", dev_addr, l, m, r, f, b, report->x, report->y, report->wheel);

  tud_cdc_write(tempbuf, count);
  tud_cdc_write_flush();
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) len;
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  // Print raw report bytes for debugging
  /*char raw_report_buf[len * 3 + 1]; // Each byte takes 2 hex chars + space, plus null terminator
  int offset = 0;
  for (uint16_t i = 0; i < len; i++) {
    offset += sprintf(raw_report_buf + offset, "%02X ", report[i]);
  }
  sprintf(raw_report_buf + offset, "\r\n");
  tud_cdc_write("Raw HID report: ", 16);
  tud_cdc_write(raw_report_buf, strlen(raw_report_buf));
  tud_cdc_write_flush();*/

  switch(itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      //process_kbd_report(dev_addr, (hid_keyboard_report_t const*) report );
      kbd_to_con((hid_keyboard_report_t const*) report); // Converts keyboard report to gamepad report
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      //process_mouse_report(dev_addr, (hid_mouse_report_t const*) report );
      mouse_to_con((hid_mouse_report_t const*) report); // Converts mouse report to gamepad report
    break;

    default: break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    tud_cdc_write_str("Error: cannot request report\r\n");
  }
}