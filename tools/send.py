import serial
import time

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

    # The string to send
    string_to_send = "Hello, World!"

    # Encode the string to bytes (UTF-8 or ASCII are common encodings)
    # The `b` prefix can also be used for a simple byte string: ser.write(b'hello')
    bytes_to_send = string_to_send.encode('ascii')

    # Write the bytes to the serial port
    ser.write(bytes_to_send)
    print(f"Sent: {string_to_send!r}")

    # Optional: Read response (if the connected device sends one back)
    # line = ser.readline().decode('utf-8').strip()
    # if line:
    #     print(f"Received: {line!r}")

except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    print("Please check port name and permissions.")

finally:
    # Close the port
    if ser and ser.isOpen():
        ser.close()
        print("Port closed.")
