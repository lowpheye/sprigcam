// ESP32-CAM (AI-Thinker) -> WHY2025 badge video link, sender side.
//
// Unlike firmware/esp32cam/esp32cam.ino (the Sprig's UART-based sender), the
// WHY2025 badge's BadgeVMS OS has no UART device API -- only WiFi/sockets,
// I2C, LCD, keyboard, filesystem and a gas sensor. So this variant runs the
// ESP32-CAM as its own WiFi access point and serves the video feed over a
// plain TCP socket instead of wiring a serial pin.
//
// Board setup in Arduino IDE: board "AI Thinker ESP32-CAM", PSRAM enabled.
// Flash via the HW-381 baseboard (hold IO0 low / press its IO0 button while
// resetting to enter bootloader), then unplug the baseboard for normal use.

#include "esp_camera.h"
#include <WiFi.h>

// AI-Thinker ESP32-CAM camera pin map (standard across the community, e.g.
// Espressif's CameraWebServer example).
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- Link config: must match firmware/why2025_badge/esp32cam_view/main.c ---
static const char    *AP_SSID      = "SPRAMERA-CAM";
static const char    *AP_PASS      = "spramera1"; // WPA2 requires >= 8 chars
static const uint16_t SERVER_PORT  = 9191;
static const framesize_t CAM_FRAME_SIZE = FRAMESIZE_VGA; // 640x480 RGB565
static const uint32_t CAM_W = 640; // must match CAM_FRAME_SIZE above
static const uint32_t CAM_H = 480; // must match CAM_FRAME_SIZE above

WiFiServer server(SERVER_PORT);

void haltBlinking() {
  // No screen on this side to report errors on -- blink the onboard red LED
  // (GPIO33 on AI-Thinker boards) so an init failure is at least visible.
  pinMode(33, OUTPUT);
  while (true) {
    digitalWrite(33, !digitalRead(33));
    delay(200);
  }
}

void setup() {
  Serial.begin(115200);

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = CAM_FRAME_SIZE;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    haltBlinking();
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("SoftAP up, IP: ");
  Serial.println(WiFi.softAPIP()); // should be 192.168.4.1

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) {
    delay(10);
    return;
  }

  Serial.println("badge connected");

  struct __attribute__((packed)) {
    char magic[4];
    uint32_t width;
    uint32_t height;
  } hdr = {{'C', 'A', 'M', '1'}, CAM_W, CAM_H};

  client.write((const uint8_t *)&hdr, sizeof(hdr));

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;

    size_t len = fb->len;
    size_t written = client.write(fb->buf, len);
    esp_camera_fb_return(fb);

    if (written != len) {
      // badge fell behind or disconnected mid-write
      break;
    }
  }

  Serial.println("badge disconnected");
  client.stop();
}
