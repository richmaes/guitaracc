#!/usr/bin/env python3
import serial
import time

port = '/dev/tty.usbmodem0010501494421'
ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
time.sleep(0.5)

def send_cmd(cmd):
    print(f"\n>>> {cmd}")
    ser.write((cmd + '\r\n').encode())
    time.sleep(0.7)
    if ser.in_waiting:
        resp = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        print(resp)

# Test various commands
send_cmd('config show')
send_cmd('config midi_ch 3')
send_cmd('config cc x 20')
send_cmd('config show')

ser.close()
print("\nDone!")
