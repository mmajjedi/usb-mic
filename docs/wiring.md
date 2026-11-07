# Wiring

| INMP441 | ESP32-S3 | Notes |
|---|---|---|
| VDD | 3V3 through the RC filter below | never 5V |
| GND | GND | |
| L/R | GND | selects the left channel (mono) |
| SCK | GPIO4 | I2S bit clock |
| WS | GPIO5 | I2S word select |
| SD | GPIO6 | I2S data |

## Power filter (100 Ω + 100 µF)

```
S3 3V3 ──[ 100 Ω ]──┬── INMP441 VDD
                    │
                  100 µF (+)
                    │
GND ────────────────┴── INMP441 GND
```

- Solder both parts as close to the mic pins as you can.
- The capacitor stripe (short leg) goes to GND.
- The mic draws ~1.4 mA, so the resistor only drops ~0.14 V. The mic still gets ~3.16 V and runs fine from 1.8 to 3.6 V.
- Optional: add a 100 nF ceramic ("104") in parallel with the 100 µF to also catch very high-frequency noise.

## Soldering the INMP441

The module is bottom-ported: sound enters through the small hole in the PCB. Keep flux, solder and alcohol out of it, and use quick touches with no hot air.
