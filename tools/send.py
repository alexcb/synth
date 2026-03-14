import serial
import time
import sys
import zlib

reboot = False
path = sys.argv[1]
if path == "reboot":
    reboot = True
else:
    data_to_send = open(path).read().encode('ascii')
    crc = zlib.crc32(data_to_send) & 0xffffffff
    print(f'patch size is {len(data_to_send)}; crc is {crc}')

# Configure the serial port settings
# Replace '/dev/ttyUSB0' with your actual port name
# Ensure the baud rate (e.g., 9600) matches the receiving device's settings
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

try:
    # Open the serial port with a timeout of 1 second
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Port {ser.name} opened successfully.")

    # Give the connection a moment to establish
    time.sleep(0.2)

    if reboot:
        ser.write('magic-reboot-string-omg'.encode('ascii'))
    else:
        ser.write('here-comes-a-new-patch'.encode('ascii'))
        ser.write(len(data_to_send).to_bytes(2, byteorder='little'))
        ser.write(crc.to_bytes(4, byteorder='little'))
        ser.write(data_to_send)

except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    print("Please check port name and permissions.")

finally:
    # Close the port
    if ser and ser.isOpen():
        ser.close()
        print("Port closed.")
