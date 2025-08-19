/*

  This is a MJPEG streaming webserver implemented for AI-Thinker ESP32-CAM
  and ESP-EYE modules.
  This is tested to work with VLC and Blynk video widget and can support up to 10
  simultaneously connected streaming clients.
  Simultaneous streaming is implemented with FreeRTOS tools: queue and tasks.

  Inspired by and based on this Instructable: $9 RTSP Video Streamer Using the ESP32-CAM Board
  (https://www.instructables.com/id/9-RTSP-Video-Streamer-Using-the-ESP32-CAM-Board/)

*/

#include "definitions.h"
#include "references.h"
#include <Preferences.h>
#include <FS.h>
#include <SPIFFS.h>


#include "credentials.h"
#include "streaming.h"

const char *c_ssid = WIFI_SSID;
const char *c_pwd = WIFI_PWD;

// Camera models overview:
// https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/


#include "camera_pins.h"

WebServer server(80);

// ===== rtos task handles =========================
// Streaming is implemented with tasks:
TaskHandle_t tMjpeg;            // handles client connections to the webserver
TaskHandle_t tCam;              // handles getting picture frames from the camera and storing them locally
TaskHandle_t tStream;

uint8_t      noActiveClients;   // number of active clients

// frameSync semaphore is used to prevent streaming buffer as it is replaced with the next frame
SemaphoreHandle_t frameSync = NULL;


// ==== SETUP method ==================================================================
void setup() {

  // Setup Serial connection:
  Serial.begin(SERIAL_RATE);
  delay(500); // wait for a bit to let Serial connect

  setupLogging();

  Log.trace("\n\nMulti-client MJPEG Server\n");
  Log.trace("setup: total heap  : %d\n", ESP.getHeapSize());
  Log.trace("setup: free heap   : %d\n", ESP.getFreeHeap());
  Log.trace("setup: total psram : %d\n", ESP.getPsramSize());
  Log.trace("setup: free psram  : %d\n", ESP.getFreePsram());

  // Mount SPIFFS for serving UI assets
  if (!SPIFFS.begin(true)) {
    Log.error("setup: Failed to mount SPIFFS\n");
  } else {
    Log.notice("setup: SPIFFS mounted\n");
  }

  static camera_config_t camera_config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sscb_sda   = SIOD_GPIO_NUM,
    .pin_sscb_scl   = SIOC_GPIO_NUM,
    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,

    .xclk_freq_hz   = XCLK_FREQ,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAME_SIZE,  // Will be overridden below
    .jpeg_quality   = JPEG_QUALITY,
    .fb_count       = 3,  // Increased for triple buffering
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
    .sccb_i2c_port = -1  // Use default I2C
  };

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

#if defined(CAMERA_MODEL_AI_THINKER)
  // Initialize PWDN pin
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(100);
#endif

  // Using default framesize value (persistence requires NVS)
  Log.notice("Camera init: Using fixed framesize VGA\n");
  
  Log.notice("Camera init: Initializing with frame_size=%d (FRAME_SIZE=%d)\n", camera_config.frame_size, FRAME_SIZE);
  if (esp_camera_init(&camera_config) != ESP_OK) {
    Log.fatal("setup: Error initializing the camera\n");
    delay(10000);
    ESP.restart();
  }
  
  // Check actual resolution after initialization
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    Log.notice("Camera init: Actual frame size after init: %dx%d\n", fb->width, fb->height);
    
        Log.notice("Camera init: Actual frame size: %dx%d\n", fb->width, fb->height);
    
    esp_camera_fb_return(fb);
  } else {
    Log.error("Camera init: Failed to get frame buffer after init\n");
  }

#if defined (FLIP_VERTICALLY)
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, true);
#endif

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != NULL) {
    sensor->set_exposure_ctrl(sensor, 1);
    sensor->set_aec_value(sensor, 300);
    sensor->set_gain_ctrl(sensor, 1);
    sensor->set_agc_gain(sensor, 4);
    sensor->set_gainceiling(sensor, (gainceiling_t)6);

    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 0);
    sensor->set_saturation(sensor, 0);
    sensor->set_special_effect(sensor, 0);
    
    // White balance optimizations
    sensor->set_whitebal(sensor, 1);          // Enable white balance
    sensor->set_awb_gain(sensor, 1);          // Enable AWB gain
    sensor->set_wb_mode(sensor, 0);           // Auto white balance
    
    // Disable AEC2 for consistent timing
    sensor->set_aec2(sensor, 1);
    sensor->set_ae_level(sensor, 1);
    
    // Image processing optimizations
    sensor->set_bpc(sensor, 1);               // Enable bad pixel correction
    sensor->set_wpc(sensor, 1);               // Enable white pixel correction
    sensor->set_raw_gma(sensor, 1);           // Enable gamma correction
    sensor->set_lenc(sensor, 1);              // Enable lens correction
    
    // Orientation
    sensor->set_hmirror(sensor, 0);           // No horizontal mirror
    sensor->set_vflip(sensor, 0);             // No vertical flip
    sensor->set_dcw(sensor, 1);               // Enable downsize
    sensor->set_colorbar(sensor, 0);          // Disable test pattern
    
  } else {
    Log.warning("setup: Could not get camera sensor for default settings\n");
  }

  // Load persisted camera settings from NVS and apply (overrides the optimizations above if present)
  {
    Preferences prefs;
    if (prefs.begin("cam", false)) {
      sensor_t* s = esp_camera_sensor_get();
      if (s != NULL) {
        int v;

        // Framesize is now fixed at VGA in platformio.ini

        // Image quality
        v = prefs.getInt("q", -10000);   if (v != -10000) { s->set_quality(s, v); Log.notice("setup: Loaded quality = %d\n", v); }
        v = prefs.getInt("br", -10000);  if (v != -10000) { s->set_brightness(s, v); Log.notice("setup: Loaded brightness = %d\n", v); }
        v = prefs.getInt("ct", -10000);  if (v != -10000) { s->set_contrast(s, v); Log.notice("setup: Loaded contrast = %d\n", v); }
        v = prefs.getInt("sa", -10000);  if (v != -10000) { s->set_saturation(s, v); Log.notice("setup: Loaded saturation = %d\n", v); }
        v = prefs.getInt("gc", -10000);  if (v != -10000) { s->set_gainceiling(s, (gainceiling_t)v); Log.notice("setup: Loaded gainceiling = %d\n", v); }

        // Toggles
        v = prefs.getInt("cb", -10000);  if (v != -10000) s->set_colorbar(s, v);
        v = prefs.getInt("awb", -10000); if (v != -10000) s->set_whitebal(s, v);
        v = prefs.getInt("agc", -10000); if (v != -10000) s->set_gain_ctrl(s, v);
        v = prefs.getInt("aec", -10000); if (v != -10000) s->set_exposure_ctrl(s, v);
        v = prefs.getInt("hm", -10000);  if (v != -10000) s->set_hmirror(s, v);
        v = prefs.getInt("vf", -10000);  if (v != -10000) s->set_vflip(s, v);

        // Advanced values
        v = prefs.getInt("awbg", -10000); if (v != -10000) s->set_awb_gain(s, v);
        v = prefs.getInt("agcg", -10000); if (v != -10000) s->set_agc_gain(s, v);
        v = prefs.getInt("aecv", -10000); if (v != -10000) s->set_aec_value(s, v);
        v = prefs.getInt("aec2", -10000); if (v != -10000) s->set_aec2(s, v);
        v = prefs.getInt("dcw", -10000);  if (v != -10000) s->set_dcw(s, v);
        v = prefs.getInt("bpc", -10000);  if (v != -10000) s->set_bpc(s, v);
        v = prefs.getInt("wpc", -10000);  if (v != -10000) s->set_wpc(s, v);
        v = prefs.getInt("rg", -10000);   if (v != -10000) s->set_raw_gma(s, v);
        v = prefs.getInt("lenc", -10000); if (v != -10000) s->set_lenc(s, v);
        v = prefs.getInt("se", -10000);   if (v != -10000) s->set_special_effect(s, v);
        v = prefs.getInt("wb", -10000);   if (v != -10000) s->set_wb_mode(s, v);
        v = prefs.getInt("ael", -10000);  if (v != -10000) s->set_ae_level(s, v);
      }
      prefs.end();
    }
  }

  // Configure and connect to WiFi - TURBO OPTIMIZED
  // Start in STA-only mode; AP will be enabled only if STA fails
  WiFi.mode(WIFI_STA);
  // Ensure AP interface is fully stopped (in case it was left from previous boot)
  WiFi.softAPdisconnect(true);
  #ifdef ESP32
  WiFi.enableAP(false);
  #endif
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  
  // Maximum WiFi performance hacks
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);  // 40MHz channel
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power saving
  
  // TCP/IP stack settings for maximum speed
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);  // Minimal security = higher speed
  
  Log.notice("setup: Fast WiFi connection to SSID: '%s'\n", c_ssid);
  
  // Read cached WiFi info from NVS for faster connection
  Preferences wifiPrefs;
  wifiPrefs.begin("wifi_cache", false);
  int cachedChannel = wifiPrefs.getInt("channel", -1);
  String cachedBSSID = wifiPrefs.getString("bssid", "");
  wifiPrefs.end();
  
  WiFi.disconnect(true);
  delay(100); // Reduced delay
  
  bool connected = false;
  int attempts = 0;
  IPAddress ip;
  
  // Method 1: Fast connection using cached data (if available)
  if (cachedChannel != -1 && cachedBSSID.length() == 17) {
    Log.notice("setup: Trying cached connection (Channel: %d)\n", cachedChannel);
    
    // Convert BSSID string to bytes
    uint8_t bssid[6];
    sscanf(cachedBSSID.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
           &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
    
    WiFi.begin(c_ssid, c_pwd, cachedChannel, bssid);
    
    while (WiFi.status() != WL_CONNECTED && attempts < 3) { // Reduced attempts
      delay(500); // Reduced delay
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      ip = WiFi.localIP();
      Log.notice("setup: Fast cached connection successful: %p\n", ip);
    } else {
      Log.notice("setup: Cached connection failed, trying scan method\n");
      WiFi.disconnect(true);
      delay(100);
    }
  }
  
  // Method 2: Targeted scan (only if cached connection failed)
  if (!connected) {
    Log.notice("setup: Targeted scan for '%s'...\n", c_ssid);
    
    // Scan only for our target SSID (faster than full scan)
    int n = WiFi.scanNetworks(false, false, false, 300, 0, c_ssid);
    int bestChannel = -1;
    int bestRSSI = -100;
    uint8_t bestBSSID[6];
    
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == String(c_ssid) && WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        bestChannel = WiFi.channel(i);
        memcpy(bestBSSID, WiFi.BSSID(i), 6);
        Log.notice("setup: Found target (RSSI: %d, Ch: %d)\n", bestRSSI, bestChannel);
      }
    }
    
    if (bestChannel != -1) {
      WiFi.begin(c_ssid, c_pwd, bestChannel, bestBSSID);
      
      attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 3) {
        delay(500);
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        ip = WiFi.localIP();
        Log.notice("setup: Scan connection successful: %p\n", ip);
        
        // Cache successful connection info
        wifiPrefs.begin("wifi_cache", false);
        wifiPrefs.putInt("channel", bestChannel);
        char bssidStr[18];
        snprintf(bssidStr, sizeof(bssidStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 bestBSSID[0], bestBSSID[1], bestBSSID[2], 
                 bestBSSID[3], bestBSSID[4], bestBSSID[5]);
        wifiPrefs.putString("bssid", bssidStr);
        wifiPrefs.end();
        Log.notice("setup: Connection cached for next boot\n");
      }
    }
  }
  
  // Method 3: Standard fallback (last resort)
  if (!connected) {
    Log.warning("setup: Trying standard connection...\n");
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(c_ssid, c_pwd);
    
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 3) {
      delay(500);
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      ip = WiFi.localIP();
      Log.notice("setup: Standard connection successful: %p\n", ip);
    }
  }
  
  if (connected) {
    // Detailed WiFi connection diagnostics
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    
    Log.notice("setup: WiFi RSSI: %d dBm\n", WiFi.RSSI());
    Log.notice("setup: WiFi Channel: %d\n", WiFi.channel());
    Log.notice("setup: WiFi PHY Mode: %s\n", 
      (ap_info.phy_11n) ? "802.11n" : 
      (ap_info.phy_11g) ? "802.11g" : "802.11b");
    Log.notice("setup: WiFi Bandwidth: %s\n", 
      (ap_info.second == WIFI_SECOND_CHAN_ABOVE || ap_info.second == WIFI_SECOND_CHAN_BELOW) ? "40MHz" : "20MHz");
    
    // Get connection information
    uint8_t protocol_bitmap;
    esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap);
    
    Serial.printf("=== WiFi Connection Info ===\n");
    Serial.printf("IP: %s\n", ip.toString().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("Channel: %d\n", WiFi.channel());
    Serial.printf("PHY: %s\n", (ap_info.phy_11n) ? "802.11n" : (ap_info.phy_11g) ? "802.11g" : "802.11b");
    Serial.printf("Bandwidth: %s\n", (ap_info.second != WIFI_SECOND_CHAN_NONE) ? "40MHz" : "20MHz");
    Serial.printf("Stream Link: http://%s%s\n\n", ip.toString().c_str(), STREAMING_URL);
  } else {
    Log.warning("setup: All WiFi methods failed, starting AP mode\n");
    // Switch to AP-only mode and start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PWD);
    Log.notice("setup: AP IP address: %p\n", WiFi.softAPIP());
  }

  // Start main streaming RTOS task with optimized settings
  xTaskCreatePinnedToCore(
    mjpegCB,
    "mjpeg",
    NETWORK_STACK_SIZE,  // Optimized stack size
    NULL,
    NETWORK_TASK_PRIORITY,  // Optimized priority for network tasks
    &tMjpeg,
    PRO_CPU);  // Keep on PRO_CPU for WiFi/network tasks

  Log.trace("setup complete: free heap  : %d\n", ESP.getFreeHeap());
}

void loop() {
  vTaskDelete(NULL);
}
