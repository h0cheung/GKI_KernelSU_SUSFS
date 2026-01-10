/* send_shanwan_init.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VID 0x20bc
#define PID 0x515b

void send_packets(libusb_device_handle *dev_handle) {
    unsigned char data[32];
    int r;

    // Msg 1: 0xc1, 0x01, 0x100, 0, len=20
    r = libusb_control_transfer(dev_handle, 0xc1, 0x01, 0x100, 0, data, 20, 100);
    // printf("Msg 1: %d\n", r);

    // Msg 2: 0xc1, 0x01, 0x0, 0, len=8
    r = libusb_control_transfer(dev_handle, 0xc1, 0x01, 0x0, 0, data, 8, 100);
    // printf("Msg 2: %d\n", r);

    // Msg 3: 0xc0, 0x01, 0x0, 0, len=4
    r = libusb_control_transfer(dev_handle, 0xc0, 0x01, 0x0, 0, data, 4, 100);
    // printf("Msg 3: %d\n", r);
}

int main() {
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle = NULL;
    int r;

    printf("Starting Shanwan Init Loop (VID=0x%04x PID=0x%04x)...\n", VID, PID);

    r = libusb_init(&ctx);
    if(r < 0) {
        fprintf(stderr, "Init Error: %d\n", r);
        return 1;
    }

    while (1) {
        // Try to open device
        dev_handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
        
        if(dev_handle == NULL) {
            // Device not found, wait 50ms and retry
            usleep(50000); 
            continue;
        }

        // Device found! Send packets
        send_packets(dev_handle);
        
        libusb_close(dev_handle);
        dev_handle = NULL;

        // Wait 50ms before next check/send
        usleep(50000);
    }

    libusb_exit(ctx);
    return 0;
}
