// Stage 1 mic test: checks the INMP441 + wiring before you build the USB firmware.
// Works on any ESP32 (classic, C3, S3) with Arduino core esp32 3.x.
// On C3 / S3: Tools -> USB CDC On Boot -> Enabled.
//
// MODE_PLOT 1: open the Serial Plotter at 921600 and talk, the line should jump.
// MODE_PLOT 0: close the Serial Monitor and run tools/record.py to save a 10 s WAV.

#include "driver/i2s_std.h"

// --- pins: classic ESP32 uses SCK 26 / WS 25 / SD 33 ---
#define PIN_SCK 4
#define PIN_WS  5
#define PIN_SD  6

#define SAMPLE_RATE 16000
#define MODE_PLOT   1    // 1 = level in Serial Plotter, 0 = raw recorder for record.py
#define GAIN_SHIFT  13   // 16 = no gain, 14 = +12 dB, 13 = +18 dB, 12 = +24 dB
#define BLOCK       256

i2s_chan_handle_t rx;
int32_t raw[BLOCK];
int16_t pcm[BLOCK];
float dc_x1 = 0, dc_y1 = 0;
bool recording = false;
int blocksLeft = 0;

int16_t convert(int32_t s) {
  float x = (float)(s >> GAIN_SHIFT);        // 24-bit sample sits in the top of the 32-bit slot
  float y = x - dc_x1 + 0.995f * dc_y1;       // DC blocker (removes offset)
  dc_x1 = x; dc_y1 = y;
  if (y > 32767) y = 32767;
  if (y < -32768) y = -32768;
  return (int16_t)y;
}

void setupI2S() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  Serial.printf("new_channel: %s\n", esp_err_to_name(i2s_new_channel(&cc, NULL, &rx)));
  i2s_std_config_t sc = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)PIN_SCK,
      .ws   = (gpio_num_t)PIN_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)PIN_SD,
      .invert_flags = {},
    },
  };
  sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   // L/R tied to GND
  Serial.printf("init_std: %s\n", esp_err_to_name(i2s_channel_init_std_mode(rx, &sc)));
  Serial.printf("enable: %s\n",   esp_err_to_name(i2s_channel_enable(rx)));
}

void setup() {
  Serial.begin(921600);
  delay(2000);
  Serial.println("boot");
  setupI2S();
}

void loop() {
  size_t got = 0;
  i2s_channel_read(rx, raw, sizeof(raw), &got, portMAX_DELAY);
  int n = got / 4;
  for (int i = 0; i < n; i++) pcm[i] = convert(raw[i]);

#if MODE_PLOT
  double acc = 0;
  for (int i = 0; i < n; i++) acc += (double)pcm[i] * pcm[i];
  Serial.print(sqrt(acc / n));               // talk -> the line should jump
  Serial.print(" ");
  Serial.println(raw[0]);                    // raw mic sample (debug; remove once it works)
#else
  if (recording) {
    Serial.write((uint8_t*)pcm, n * 2);
    if (--blocksLeft <= 0) recording = false;
  } else if (Serial.available() && Serial.read() == 'r') {
    recording = true;
    blocksLeft = SAMPLE_RATE * 10 / BLOCK;    // 10 s = 625 blocks
  }
#endif
}
