#include "usbd_custom_hid_if.h"

static int8_t CUSTOM_HID_Init_FS(void);
static int8_t CUSTOM_HID_DeInit_FS(void);
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t *data, uint8_t size);

USB_MIDI_ItfTypeDef USB_MIDI_dev =
{
  NULL,
  CUSTOM_HID_Init_FS,
  CUSTOM_HID_DeInit_FS,
  CUSTOM_HID_OutEvent_FS
};

extern USBD_HandleTypeDef hUsbDeviceFS;
extern void USB_MIDI_dout(MIDIMesg *);
extern void USB_MIDI_sysex(uint8_t *data, uint16_t size);

static int8_t CUSTOM_HID_Init_FS(void) { return (USBD_OK); }
static int8_t CUSTOM_HID_DeInit_FS(void) { return (USBD_OK); }

//Sizes of different commands (taken from page 16 of https://www.usb.org/sites/default/files/midi10.pdf)
static const uint8_t MIDI_sizes[16] = {
    0, 0, 2, 3,
    3, 1, 2, 3,
    3, 3, 3, 3,
    2, 2, 3, 1
};

static uint8_t sysex_buffer[512];
static uint16_t sysex_cursor;
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t *data, uint8_t size){
    while(((int8_t)size) > 0){
        uint8_t cmd_size = MIDI_sizes[data[0] & 0x0f];
        uint8_t cmd = data[0] & 0x0f;
        data++;
        size--;

        if(cmd_size == 0)continue; //Undefined cmd, skipping

        //Sysex start, end and data
        if(cmd >= 0x04 && cmd <= 0x7){
            if(data[0] == 0xf0){
                sysex_cursor = 0;
            }
            //Avoid segfault
            if(sysex_cursor > 500)break;
            memcpy(sysex_buffer + sysex_cursor, data, cmd_size);

            sysex_cursor += cmd_size;
            data += cmd_size;
            size -= cmd_size;

            if(cmd != 0x04)USB_MIDI_sysex(sysex_buffer, sysex_cursor);
            continue;
        }

        if(cmd < 0x8 || cmd > 0xe){
            data += cmd_size;
            size -= cmd_size;
            continue;
        }

        USB_MIDI_dout((MIDIMesg *)data);
        data += cmd_size;
        size -= cmd_size;
    }

    if(USB_MIDI_ReceivePacket(&hUsbDeviceFS) != (uint8_t)USBD_OK){
        return -1;
    }

    return (USBD_OK);
}
