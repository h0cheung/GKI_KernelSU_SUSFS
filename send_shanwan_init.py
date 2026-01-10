import usb.core
import usb.util
import time

# Betop KP40 VID/PID
VID = 0x20bc
PID = 0x515b

def send_init_packets():
    print(f"Looking for device {hex(VID)}:{hex(PID)}...")
    while True:
        try:
            # Find the device
            dev = usb.core.find(idVendor=VID, idProduct=PID)
            
            if dev is None:
                # Device not found, wait and retry
                time.sleep(0.05)
                continue
            
            # Device found, send the 3 Shanwan control messages
            # These can be sent even if the kernel driver is active
            
            # Msg 1: 0xc1, 0x01, 0x100, 0, len=20
            try:
                dev.ctrl_transfer(0xc1, 0x01, 0x100, 0, 20)
                # print("Sent Msg 1")
            except usb.core.USBError:
                pass # Ignore errors (e.g. if device disconnects mid-transfer)

            # Msg 2: 0xc1, 0x01, 0x0, 0, len=8
            try:
                dev.ctrl_transfer(0xc1, 0x01, 0x0, 0, 8)
                # print("Sent Msg 2")
            except usb.core.USBError:
                pass

            # Msg 3: 0xc0, 0x01, 0x0, 0, len=4
            try:
                dev.ctrl_transfer(0xc0, 0x01, 0x0, 0, 4)
                # print("Sent Msg 3")
            except usb.core.USBError:
                pass
                
            # Wait 50ms before next burst
            time.sleep(0.05)
            
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    try:
        send_init_packets()
    except KeyboardInterrupt:
        print("\nStopped.")
