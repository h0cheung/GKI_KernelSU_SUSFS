/* kp40_sim.c - Betop KP40 Simulator /Debugger */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <signal.h>

#define VID 0x20bc
#define PID 0x515b

/* Endpoints based on capture */
#define EP_IN 0x81
#define EP_OUT 0x02
#define EP_HID_OUT 0x04

volatile int keep_running = 1;
libusb_device_handle *dev_handle = NULL;

void handle_signal(int sig) {
    keep_running = 0;
}

/* Helper to send control msg */
void send_ctrl(uint8_t bmReq, uint8_t bReq, uint16_t wVal, uint16_t wIdx, int len) {
    unsigned char data[1024];
    int r = libusb_control_transfer(dev_handle, bmReq, bReq, wVal, wIdx, data, len, 100);
    // if(r < 0) printf("Ctrl Send Failed: %s\n", libusb_error_name(r));
    // else printf("Ctrl Send OK (%d bytes)\n", r);
}

/* Helper to send Interrupt OUT */
void send_int(uint8_t ep, uint8_t *data, int len) {
    int transferred;
    int r = libusb_interrupt_transfer(dev_handle, ep, data, len, &transferred, 100);
    // if(r < 0) printf("Int OUT (EP %02x) Failed: %s\n", ep, libusb_error_name(r));
    // else printf("Int OUT (EP %02x) OK\n", ep);
}

void *heartbeat_thread_func(void *arg) {
    unsigned char led_pkt[] = {0x01, 0x03, 0x02};
    int count = 0;

    while(keep_running) {
        /* Heartbeat: Send LED packet every ~50ms */
        // Windows sends these packets frequently. Maybe it acts as a heartbeat.
        send_int(EP_OUT, led_pkt, sizeof(led_pkt));
        
        usleep(50000); // 50ms
        count++;
    }
    return NULL;
}

int main() {
    libusb_context *ctx = NULL;
    int r;
    unsigned char data[64];
    int transferred;
    pthread_t hb_thread;

    signal(SIGINT, handle_signal);

    r = libusb_init(&ctx);
    if(r < 0) {
        fprintf(stderr, "Init Error: %d\n", r);
        return 1;
    }

    printf("Waiting for device %04x:%04x...\n", VID, PID);
    
    // Wait loop
    while (keep_running) {
        dev_handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if(dev_handle) break;
        
        // Check if device is present but permission denied
        // Note: libusb_open_device_with_vid_pid doesn't return error code for permission
        // but we can guess if we loop too long.
        
        usleep(100000); // 100ms
    }

    if (!keep_running) {
        libusb_exit(ctx);
        return 0;
    }
    
    printf("Device found.\n");

    /* 1. Detach Kernel Drivers (Both Interfaces) */
    if(libusb_kernel_driver_active(dev_handle, 0)) {
        r = libusb_detach_kernel_driver(dev_handle, 0);
        if(r == 0) printf("Detached IF 0\n");
        else printf("Error detaching IF 0: %s\n", libusb_error_name(r));
    }
    
    // Interface 1 (HID) is likely claimed by usbhid
    r = libusb_kernel_driver_active(dev_handle, 1);
    if(r == 1) {
        r = libusb_detach_kernel_driver(dev_handle, 1);
        if(r == 0) printf("Detached IF 1\n");
        else printf("Error detaching IF 1: %s\n", libusb_error_name(r));
    }

    libusb_claim_interface(dev_handle, 0);
    libusb_claim_interface(dev_handle, 1);

    /* 2. Initialization Sequence (Correct Order + Extras) */
    printf("Starting Initialization...\n");

    /* D. Set Idle (HID standard) to Interface 1 */
    int rr = libusb_control_transfer(dev_handle, 0x21, 0x0a, 0, 1, NULL, 0, 100);
    // if(rr < 0) printf("Set Idle Failed: %s\n", libusb_error_name(rr));
    // else printf("Set Idle Sent\n");

    /* E. Blind MS Feature Descriptor Request (Commonly 0x20) */
    unsigned char ms_data[64];
    rr = libusb_control_transfer(dev_handle, 0xc0, 0x20, 0, 4, ms_data, 64, 100);
    // if(rr < 0) printf("MS Desc (0x20) Failed: %s\n", libusb_error_name(rr));
    // else printf("MS Desc (0x20) Sent\n");

    /* A. Interrupt Packets (LED & Mode) */
    unsigned char pkt1[] = {0x01, 0x03, 0x02};
    unsigned char pkt2[] = {0x02, 0x08, 0x03};
    send_int(EP_OUT, pkt1, 3);
    usleep(2000); // 2ms
    send_int(EP_OUT, pkt2, 3);
    
    /* B. Shanwan Control Messages */
    send_ctrl(0xc1, 0x01, 0x100, 0, 20);
    send_ctrl(0xc1, 0x01, 0x00, 0, 8);
    send_ctrl(0xc0, 0x01, 0x00, 0, 4);

    /* C. HID Init (From Frame 409) */
    unsigned char pkt3[] = {0x01, 0x00};
    usleep(10000);
    send_int(EP_HID_OUT, pkt3, 2);
    // printf("HID Init Sent\n");

    /* F. Get HID Report Descriptor (Interface 1) - Critical for some devices */
    unsigned char rep_desc[512];
    // bmReq=0x81 (Dev->Host, Standard, Interface)
    // bReq=0x06 (Get Descriptor)
    // wVal=0x2200 (Report Descriptor, Index 0)
    // wIdx=1 (Interface 1)
    rr = libusb_control_transfer(dev_handle, 0x81, 0x06, 0x2200, 1, rep_desc, 512, 100);
    if(rr < 0) printf("Get RepDesc Failed: %s\n", libusb_error_name(rr));
    else printf("Get RepDesc OK (%d bytes)\n", rr);

    printf("Init Sequence Helper Done.\n");

    /* 3. Start Heartbeat Thread */
    pthread_create(&hb_thread, NULL, heartbeat_thread_func, NULL);

    /* 4. Input Loop */
    printf("Reading Input (Ctrl+C to stop)...\n");
    while(keep_running) {
        r = libusb_interrupt_transfer(dev_handle, EP_IN, data, sizeof(data), &transferred, 100);
        if (r == 0 && transferred > 0) {
            printf("IN: ");
            for(int i=0; i<transferred; i++) printf("%02x ", data[i]);
            printf("\n");
        } else if (r != LIBUSB_ERROR_TIMEOUT) {
            if (r == LIBUSB_ERROR_NO_DEVICE) {
                printf("Device Disconnected!\n");
                break;
            }
            // printf("Read Error: %s\n", libusb_error_name(r));
        }
    }

    cancel_thread: 
    // pthread_cancel(hb_thread); // Android Bionic might not allow this easily, just detach or join
    // We rely on keep_running flag.
    pthread_join(hb_thread, NULL);
    
    libusb_close(dev_handle);
    libusb_exit(ctx);
    return 0;
}
