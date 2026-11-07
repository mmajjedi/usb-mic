# Full build walkthrough

The complete step-by-step guide, from soldering the mic to plugging it into your phone. If you just want the short version, see the [README](../README.md).

- [0. What you're building](#0-what-youre-building)
- [1. Pick the right board](#1-pick-the-right-board)
- [2. Solder the INMP441](#2-solder-the-inmp441)
- [3. Wiring](#3-wiring)
- [4. Stage 1: test the mic (any ESP32, Arduino IDE)](#4-stage-1-test-the-mic-any-esp32-arduino-ide)
- [5. Stage 2: the real USB mic (ESP32-S3, ESP-IDF)](#5-stage-2-the-real-usb-mic-esp32-s3-esp-idf)
- [6. Tuning: how the filters came about](#6-tuning-how-the-filters-came-about)
- [7. Stage 3: make it a filming mic](#7-stage-3-make-it-a-filming-mic)
- [8. Troubleshooting](#8-troubleshooting)
- [9. Checklist](#9-checklist)

---

## 0. What you're building

INMP441 (I2S digital MEMS mic) → ESP32-S3 over I2S → the S3 shows up over USB as a standard **USB Audio Class (UAC)** microphone. No drivers, and no syncing audio in editing because the mic records straight into your phone or PC.

The build has 3 stages:

1. **Stage 1:** prove the mic and your soldering work. Any ESP32 is fine here.
2. **Stage 2:** flash the USB mic firmware. Needs an ESP32-S3.
3. **Stage 3:** enclosure, foam and filming.

## 1. Pick the right board

A plug-and-play USB mic needs a chip with **native USB**: the **ESP32-S3** (or S2). The **ESP32-C3** and the **classic ESP32** can't act as a USB audio device. They still work for Stage 1, which records a WAV through a Python script, but Stage 2 needs an S3.

| Board | Good for |
|---|---|
| ESP32-S3-DevKitC-1 (dual USB-C, e.g. N16R8) | Developing: flash and see logs through the **COM/UART** port while the **USB** port acts as the mic |
| ESP32-S3 Super Mini / Seeed XIAO ESP32-S3 | The final build: tiny, fits in a printed mic body. Single USB-C port; to reflash, hold BOOT and tap RESET |

Flash size and PSRAM (N8, N16, R2, R8) don't matter. The firmware is small.

## 2. Solder the INMP441

- The pins are **VDD, GND, L/R, WS, SCK, SD**.
- The mic is **bottom-ported**: the small hole through the PCB is where sound enters. Keep it clear, with no flux, solder splash, hot glue or alcohol pooling in it.
- Keep each pad short: 2–3 s at about 330 °C, and no hot air on the module. The MEMS membrane hates heat and liquids.
- After soldering, do a continuity check: VDD to GND must **not** beep.

## 3. Wiring

| INMP441 | ESP32-S3 / C3 | Classic ESP32 | Note |
|---|---|---|---|
| VDD | 3V3 (through the RC filter, see below) | 3V3 | **3.3 V only**, never 5 V |
| GND | GND | GND | |
| L/R | GND | GND | GND = left channel (the firmware reads LEFT) |
| SCK | GPIO4 | GPIO26 | bit clock |
| WS | GPIO5 | GPIO25 | word select / LRCLK |
| SD | GPIO6 | GPIO33 | data out of the mic |

- On the S3, avoid GPIO19/20 (that's the USB), 0, 45, 46, and 35–37 on boards with octal PSRAM.
- Keep the I2S wires short, under 15 cm. Long jumpers cause noise and clicks.
- The power filter (100 Ω + 100 µF) is optional for Stage 1 but makes a clear difference on the final mic. Diagram in [wiring.md](wiring.md).

## 4. Stage 1: test the mic (any ESP32, Arduino IDE)

Use Arduino core **esp32 3.x** (it has the new IDF 5 I2S driver). On a C3 or S3, set **Tools → USB CDC On Boot → Enabled**.

Sketch: [`tools/mic_test/mic_test.ino`](../tools/mic_test/mic_test.ino)

### 4.1 Check 1: Serial Plotter (`MODE_PLOT 1`)

1. Flash the sketch with `MODE_PLOT 1`.
2. Open the Serial Plotter at **921600**.
3. In a quiet room the line should be low and flat. When you talk or clap it should spike.

### 4.2 Debug: nothing on the plotter

Open the **Serial Monitor** (not the plotter) at 921600 and read what prints:

| What you see | Cause → fix |
|---|---|
| Nothing, not even "boot" | Wrong USB port. With USB CDC On Boot = Enabled, Serial is on the S3's **native USB** port, not COM. Plug into the other port and select it in the IDE |
| "boot", then an error on one of the 3 init lines | I2S setup failed, so the sketch blocks on the first read. Note the error |
| All 3 `ESP_OK`, raw always `0` | The mic isn't sending. Switch `I2S_STD_SLOT_LEFT` → `I2S_STD_SLOT_RIGHT`, measure 3.3 V on the module's VDD, check SD is on GPIO6 and L/R on GND |
| raw stuck at `-1` or one fixed value | SCK and WS swapped, or SD disconnected |
| raw changes but the plot looks flat | The raw value squashes the plot scale. Remove the raw print and talk again |

### 4.3 Check 2: listen to it (`MODE_PLOT 0`)

1. Set `MODE_PLOT 0` and re-flash.
2. Close the Serial Monitor and Plotter (they hold the port).
3. On the laptop: `pip install pyserial`, then run [`tools/record.py`](../tools/record.py) with your port:
   ```
   python record.py COM5
   ```
4. Talk for 10 s, then open `test.wav` in Audacity.
5. Too quiet → lower `GAIN_SHIFT` by 1. Distorted → raise it by 1.

If you only have a C3 or classic ESP32 for now, this is already a working mic: record on the laptop while you film.

## 5. Stage 2: the real USB mic (ESP32-S3, ESP-IDF)

The firmware in [`main/usb_mic.c`](../main/usb_mic.c) uses Espressif's [`usb_device_uac`](https://components.espressif.com/components/espressif/usb_device_uac) component, which turns the S3 into a USB Audio Class device.

### 5.1 Install ESP-IDF

Install **ESP-IDF 5.x**. The easiest way is VS Code + the ESP-IDF extension, or the Windows installer from Espressif. Open an **ESP-IDF terminal** (it sets `IDF_PATH` for you).

### 5.2 Get the project

```
git clone https://github.com/mmajjedi/usb-mic.git
cd usb-mic
```

Never put the project inside the ESP-IDF install folder (e.g. `C:\Espressif`), and always run `idf.py` from the project folder, not from the ESP-IDF folder.

### 5.3 Settings (already done for you)

`sdkconfig.defaults` already sets the USB audio options, so you don't need menuconfig:

| Setting | Value |
|---|---|
| Microphone channels | 1 (mono) |
| Speaker channels | 0 |
| Sample rate | 48000 (must match `SAMPLE_RATE` in `usb_mic.c`) |
| Resolution | 16-bit |
| macOS / iOS support | on |

If you want to look at or change them: `idf.py menuconfig` → **Component config → USB Device UAC**. Quickest is to press **/** and search `UAC`. If nothing shows up, run `idf.py reconfigure` first so the component gets downloaded into `managed_components`.

> Espressif's note: one build can't be compatible with Windows and Linux at the same time (Android counts as Linux). This repo is set up for **Windows + iPhone/macOS**. If you film on Android, rebuild with the matching option.

### 5.4 Build and flash

```
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

Replace `COM5` with your port. On a dual-port DevKit, flash through the **COM/UART** port. On a Super Mini / XIAO, hold **BOOT**, tap **RESET**, then flash.

You should see `USB mic ready` in the monitor.

### 5.5 Plug it in

- Plug the S3's **native USB** port (labelled "USB", GPIO19/20) into the computer or phone.
- **Windows:** Settings → Sound → Input → a new device **"ESP UAC Device"**. Record in Audacity at 48 kHz.
- **iPhone:** USB-C iPhones plug in directly. Lightning iPhones need Apple's Lightning to USB Camera Adapter. The camera app picks it up as the audio input.
- **Android:** a USB-C OTG cable. If the stock camera app ignores it, try Open Camera, which lets you pick the USB mic as the audio source.

## 6. Tuning: how the filters came about

The first version used a simple DC blocker and no power filtering. Recording the same sentence on the iPhone with the phone mic and the DIY mic showed three problems:

1. A constant hiss up to 16 kHz.
2. Too much low end: rumble and boom below 100 Hz.
3. Faint tones around 2–3 kHz, from electrical noise on the power line.

| | Phone mic | DIY, first version | DIY, final |
|---|---|---|---|
| Voice-to-noise | ~36 dB | ~33 dB | **~38 dB** |
| Noise floor | -68 dB | -53 dB | **-63 dB** |
| Low rumble (vs voice) | -18 dB | -4.5 dB | **-32 dB** |
| 2–3 kHz tones | none | present | **gone** |

The fixes:

**Fix 1: firmware high-pass filter.** The DC blocker was replaced by a 2nd-order high-pass at 90 Hz (already in `usb_mic.c`). It removes rumble from desks, printers and handling, plus any DC offset:

```c
// 2nd-order high-pass at 90 Hz, 48 kHz
static const float hb0 = 0.991702f, hb1 = -1.983404f, hb2 = 0.991702f;
static const float ha1 = -1.983335f, ha2 = 0.983477f;
...
float y = hb0 * x + hz1;
hz1 = hb1 * x - ha1 * y + hz2;
hz2 = hb2 * x - ha2 * y;
```

**Fix 2: hiss.** Hiss is the mic's own noise multiplied by the digital gain. Put the mic 15–20 cm from your mouth so your voice is louder relative to it, and tune `GAIN_SHIFT` (13 is a good default).

**Fix 3: RC filter on the mic's power.** A 100 Ω resistor in series with VDD and a 100 µF capacitor from VDD to GND, right at the mic pins (see [wiring.md](wiring.md)). This removed the 2–3 kHz tones and brought the hiss down by about 10 dB.

**Remaining:** a faint 50 Hz mains hum showed up when the laptop charger was plugged in. That's a ground loop. Unplug the charger while recording.

**Optional, in editing:** light noise reduction in CapCut or Audacity takes it to about 42 dB voice-to-noise.

## 7. Stage 3: make it a filming mic

- **Enclosure:** a small 3D-printed body with the mic at the front, sound hole facing out, and the S3 behind it or at the cable end. Solder wires directly instead of using Dupont jumpers, which take too much room.
- **Foam:** a piece of open-cell foam (packaging or sponge foam) over the sound hole works as a windscreen and pop filter. It stops "p" and "b" pops and breath noise.
- **Soft mounting:** sit the module on foam or rubber instead of screwing it hard to the shell, to cut handling and desk noise.
- **Distance:** 15–30 cm from your mouth. MEMS mics pick up sound from all directions, so closer means less room echo.

## 8. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| All zeros | L/R vs slot mismatch: switch to `I2S_STD_SLOT_RIGHT` or tie L/R to 3V3. Check the SD wire and that VDD = 3.3 V |
| Constant max value / garbage | SCK and WS swapped, or SD on the wrong pin |
| Works but very quiet | Lower `GAIN_SHIFT` (13 → 12 → 11) |
| Distorted / crunchy | `GAIN_SHIFT` too low (raise it) or mic too close |
| Clicks / dropouts | Long jumper wires; or `SAMPLE_RATE` in code ≠ the UAC sample rate in menuconfig |
| Hum / whine | Noisy USB power: add the RC filter (100 Ω + 100 µF), optionally a 100 nF ceramic too. Unplug chargers |
| S3 not seen as a mic | Plugged into the COM port instead of the native USB port, or a Windows/Linux build mismatch |
| Callback doesn't compile | The component's API changed: open `managed_components/espressif__usb_device_uac/include/usb_device_uac.h` and match the typedefs; the logic stays the same |
| Dead after soldering | Heat or flux damage to the MEMS. Swap the module |

## 9. Checklist

- [ ] INMP441 soldered, no VDD–GND bridge, sound hole clean
- [ ] Wired per the table, RC filter on VDD
- [ ] Stage 1: plotter reacts to your voice
- [ ] Stage 1: `test.wav` sounds clean, gain tuned
- [ ] Stage 2: shows up as a USB mic on the laptop
- [ ] Stage 2: works on the phone you film with
- [ ] Stage 3: enclosure + foam, test recording side by side with your phone mic
