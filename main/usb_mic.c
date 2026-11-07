#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "usb_device_uac.h"
#include "esp_log.h"

#define PIN_SCK 4
#define PIN_WS  5
#define PIN_SD  6
#define SAMPLE_RATE 48000          // must match menuconfig
#define GAIN_SHIFT  13
#define MAX_SAMPLES 1024

static const char *TAG = "mic";
static i2s_chan_handle_t rx;
static int32_t raw[MAX_SAMPLES];
// 2nd-order high-pass at 90 Hz, 48 kHz (cuts rumble + DC)
static const float hb0 = 0.991702f, hb1 = -1.983404f, hb2 = 0.991702f;
static const float ha1 = -1.983335f, ha2 = 0.983477f;
static float hz1 = 0, hz2 = 0;
static volatile bool muted = false;

static void i2s_init(void) {
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &rx));
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = PIN_SCK, .ws = PIN_WS,
            .dout = I2S_GPIO_UNUSED, .din = PIN_SD,
        },
    };
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));
}

// host asks for mic data: fill buf with 16-bit mono PCM
static esp_err_t mic_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *ctx) {
    size_t want = len / sizeof(int16_t);
    if (want > MAX_SAMPLES) want = MAX_SAMPLES;
    size_t got = 0;
    i2s_channel_read(rx, raw, want * sizeof(int32_t), &got, portMAX_DELAY);
    size_t n = got / sizeof(int32_t);
    int16_t *out = (int16_t *)buf;
    for (size_t i = 0; i < n; i++) {
        float x = (float)(raw[i] >> GAIN_SHIFT);
        float y = hb0 * x + hz1;
        hz1 = hb1 * x - ha1 * y + hz2;
        hz2 = hb2 * x - ha2 * y;
        if (y > 32767) y = 32767;
        if (y < -32768) y = -32768;
        out[i] = muted ? 0 : (int16_t)y;
    }
    *bytes_read = n * sizeof(int16_t);
    return ESP_OK;
}

static esp_err_t spk_output_cb(uint8_t *buf, size_t len, void *ctx) { return ESP_OK; }
static void set_mute_cb(uint32_t mute, void *ctx) { muted = mute; }
static void set_volume_cb(uint32_t vol, void *ctx) { ESP_LOGI(TAG, "host volume %lu", vol); }

void app_main(void) {
    i2s_init();
    uac_device_config_t cfg = {
        .output_cb     = spk_output_cb,
        .input_cb      = mic_input_cb,
        .set_mute_cb   = set_mute_cb,
        .set_volume_cb = set_volume_cb,
        .cb_ctx        = NULL,
    };
    ESP_ERROR_CHECK(uac_device_init(&cfg));
    ESP_LOGI(TAG, "USB mic ready");
}