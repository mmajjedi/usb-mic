# Records 10 s from tools/mic_test (MODE_PLOT 0) into test.wav
# Usage:  pip install pyserial
#         python record.py COM5        (Windows)
#         python record.py /dev/ttyACM0 (Linux / macOS)
import serial, wave, sys, time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
SR, SECS = 16000, 10
N = SR * SECS * 2                      # 16-bit mono

s = serial.Serial(PORT, 921600, timeout=5)
time.sleep(2)
s.reset_input_buffer()
s.write(b"r")
print("Recording 10 s... talk now")

data = bytearray()
while len(data) < N:
    chunk = s.read(N - len(data))
    if not chunk:
        break
    data += chunk

with wave.open("test.wav", "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(data)
print(f"Saved test.wav ({len(data)} bytes)")
