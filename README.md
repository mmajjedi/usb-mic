# DIY USB Microphone (ESP32-S3 + INMP441)

A plug-and-play USB microphone for about 60 DT. Plug it into a PC or an iPhone and it shows up as a normal microphone. No drivers, no app, and no syncing audio in editing.

Built by Mahdi · Instagram [@majjcrafts](https://instagram.com/majjcrafts)

📖 **New to this? Follow the full step-by-step guide: [docs/walkthrough.md](docs/walkthrough.md)**

## How it works

1. The **INMP441** is a digital MEMS microphone. It sends audio over **I2S**, so there's no analog signal for noise to get into.
2. The **ESP32-S3** reads it at 48 kHz, applies the gain and a **90 Hz high-pass filter** to remove rumble, and converts it to 16-bit mono.
3. Espressif's `usb_device_uac` component makes the S3 a standard **USB Audio Class** device, so Windows, macOS, Linux, Android and USB-C iPhones detect it on their own.
4. A **100 Ω + 100 µF RC filter** on the mic's power line removes electrical noise coming from USB.

## Results

Same sentence, same distance, same room, recorded on an iPhone:

| | Phone mic | DIY, no filters | DIY, with both filters |
|---|---|---|---|
| Voice-to-noise | ~36 dB | ~33 dB | **~38 dB** |
| Background noise | -68 dB | -53 dB | **-63 dB** |
| Low rumble (vs voice) | -18 dB | -4.5 dB | **-32 dB** |

How each filter was found and what it fixed: [walkthrough, section 6](docs/walkthrough.md#6-tuning-how-the-filters-came-about).

## Parts

| Part | Notes |
|---|---|
| ESP32-S3 board | Must be an **S3** (or S2): it needs native USB. DevKitC-1 (dual USB-C) is easiest to develop on; Super Mini or XIAO ESP32-S3 for a compact build |
| INMP441 I2S microphone module | |
| 100 Ω resistor | Power filter |
| 100 µF electrolytic capacitor (6.3 V or more) | Power filter |
| Jumper wires / breadboard, USB-C cable | Keep the I2S wires under 15 cm |

An ESP32-C3 or classic ESP32 can still run the Stage 1 test sketch, but not the USB mic firmware.

## Wiring

<!-- Wiring diagram: save it as docs/wiring.png -->
![Wiring diagram](docs/wiring.png)

| INMP441 | ESP32-S3 |
|---|---|
| VDD | 3V3 through 100 Ω (100 µF from VDD to GND) |
| GND | GND |
| L/R | GND |
| SCK | GPIO4 |
| WS | GPIO5 |
| SD | GPIO6 |

Filter diagram and soldering tips: [docs/wiring.md](docs/wiring.md)

## Quick start

**1. Test the mic first (optional but recommended).** Flash [`tools/mic_test/mic_test.ino`](tools/mic_test/mic_test.ino) from the Arduino IDE (core esp32 3.x) and talk while watching the Serial Plotter. Details in [walkthrough, section 4](docs/walkthrough.md#4-stage-1-test-the-mic-any-esp32-arduino-ide).

**2. Build and flash the USB mic** with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) 5.x, from an ESP-IDF terminal:

```
git clone https://github.com/mmajjedi/usb-mic.git
cd usb-mic
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

The `usb_device_uac` component downloads automatically on the first build, and the USB audio settings come from `sdkconfig.defaults`. You should see `USB mic ready`.

**3. Plug in the native USB port** (labelled "USB" on dual-port boards, not "UART"/"COM"). It shows up as **"ESP UAC Device"**.

## Repo layout

```
main/usb_mic.c          USB mic firmware (I2S read, gain, 90 Hz high-pass, USB audio)
sdkconfig.defaults      USB audio settings: mono, 48 kHz, 16-bit, macOS/iOS support
tools/mic_test/         Stage 1 Arduino sketch to test the mic and wiring
tools/record.py         Records a 10 s WAV from the test sketch
docs/walkthrough.md     Full step-by-step build guide
docs/wiring.md          Wiring and power filter
```

## Tuning

In `main/usb_mic.c`:

- `GAIN_SHIFT`: lower = louder. 13 is a good start. If it's too quiet try 12; if it distorts try 14.
- `PIN_SCK`, `PIN_WS`, `PIN_SD`: change these if you wire to other GPIOs.

## Tips

- The INMP441 picks up sound from all directions, so keep it 15–20 cm from your mouth.
- A bit of open-cell foam over the sound hole cuts "p" and "b" pops.
- If you hear a faint 50 Hz hum, unplug the laptop charger. It's usually a ground loop.

Something not working? See [troubleshooting](docs/walkthrough.md#8-troubleshooting).

## License

MIT. Use it, remix it, and tag me if you build one.
