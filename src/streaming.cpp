#include "streaming.h"
#include "camera_pins.h"
#include <Preferences.h>
#include <FS.h>
#include <SPIFFS.h>

// Add CORS headers to response
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Max-Age", "86400");
}

// Helper to insert 'checked' attribute into checkbox input with given id
static void insertCheckedAttribute(String &html, const char* inputId, bool isChecked) {
  if (!isChecked) return;
  String idPattern = String("id='") + inputId + "'";
  int idPos = html.indexOf(idPattern);
  if (idPos < 0) return;
  int tagEnd = html.indexOf('>', idPos);
  if (tagEnd < 0) return;
  // Insert before '>' if not already present
  String before = html.substring(0, tagEnd);
  if (before.indexOf(" checked") >= 0) return;
  html = before + " checked" + html.substring(tagEnd);
}

// Helper to mark selected option inside a <select id='...'> block
static void markSelectedOption(String &html, const char* selectId, int selectedValue) {
  String idPattern = String("id='") + selectId + "'";
  int selectStart = html.indexOf(idPattern);
  if (selectStart < 0) return;
  int blockEnd = html.indexOf("</select>", selectStart);
  if (blockEnd < 0) return;
  String block = html.substring(selectStart, blockEnd);
  String valuePattern = String("value='") + String(selectedValue) + "'";
  int valPos = block.indexOf(valuePattern);
  if (valPos < 0) return;
  // Prevent adding multiple selected
  if (block.indexOf(" selected", valPos) >= 0) return;
  block.replace(valuePattern, valuePattern + " selected");
  html = html.substring(0, selectStart) + block + html.substring(blockEnd);
}

// === ФУНКЦИЯ ДЛЯ ПОЛУЧЕНИЯ ОПИСАНИЯ РАЗРЕШЕНИЯ ===
String getFrameSizeDescription() {
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) return "Unknown Resolution";
  
  framesize_t framesize = sensor->status.framesize;
  
  switch(framesize) {
    case FRAMESIZE_96X96:     return "96x96";
    case FRAMESIZE_QQVGA:     return "160x120";
    case FRAMESIZE_QCIF:      return "176x144";
    case FRAMESIZE_HQVGA:     return "240x176";
    case FRAMESIZE_240X240:   return "240x240";
    case FRAMESIZE_QVGA:      return "320x240";
    case FRAMESIZE_CIF:       return "400x296";
    case FRAMESIZE_HVGA:      return "480x320";
    case FRAMESIZE_VGA:       return "640x480";
    case FRAMESIZE_SVGA:      return "800x600";
    case FRAMESIZE_XGA:       return "1024x768";
    case FRAMESIZE_HD:        return "1280x720";
    case FRAMESIZE_SXGA:      return "1280x1024";
    case FRAMESIZE_UXGA:      return "1600x1200";
    case FRAMESIZE_FHD:       return "1920x1080";
    case FRAMESIZE_P_HD:      return "720x1280";
    case FRAMESIZE_P_3MP:     return "864x1536";
    case FRAMESIZE_QXGA:      return "2048x1536";
    case FRAMESIZE_QHD:       return "2560x1440";
    case FRAMESIZE_WQXGA:     return "2560x1600";
    case FRAMESIZE_P_FHD:     return "1080x1920";
    case FRAMESIZE_QSXGA:     return "2560x1920";
    default:                  return "Unknown Resolution";
  }
}

// External task handles
extern TaskHandle_t tCam;

// External variables for adaptive JPEG
extern volatile float currentFPS;

// === TCP STREAMING ONLY ===

enum ClientType {
    CLIENT_HTTP_TCP    // Browser via HTTP/MJPEG  
};

// === TCP CLIENT MANAGEMENT FUNCTIONS ===


#ifdef ADAPTIVE_JPEG_QUALITY
// === АДАПТИВНОЕ УПРАВЛЕНИЕ КАЧЕСТВОМ JPEG ===
int getAdaptiveJpegQuality() {
    int quality = MAX_JPEG_QUALITY; // Начинаем с максимального качества
    
    // Проверяем количество подключенных клиентов
    if (clientsConnected >= QUALITY_THRESHOLD_CLIENTS) {
        quality = MIN_JPEG_QUALITY;
    }
    
    // Проверяем свободную память
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < QUALITY_THRESHOLD_HEAP) {
        quality = MIN_JPEG_QUALITY;
    }
    
    // Проверяем загрузку CPU (приблизительно по FPS)
    if (currentFPS < 8.0 && clientsConnected > 1) {
        quality = MIN_JPEG_QUALITY;
    }
    
    return quality;
}

// Применить адаптивное качество к камере
void applyAdaptiveQuality() {
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        int newQuality = getAdaptiveJpegQuality();
        s->set_quality(s, newQuality);
    }
}
#endif

const char* HEADER = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=+++===123454321===+++\r\n";
const char* BOUNDARY = "\r\n--+++===123454321===+++\r\n";
const char* CTNTTYPE = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);
volatile uint32_t frameNumber;
volatile uint32_t clientsConnected = 0;  // Track number of connected clients

frameChunck_t* fstFrame = NULL;  // first frame
frameChunck_t* curFrame = NULL;  // current frame being captured by the camera

const char*  STREAMING_URL = "/mjpeg/1";

void mjpegCB(void* pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(WSINTERVAL);

  // Creating frame synchronization semaphore and initializing it
  frameSync = xSemaphoreCreateBinary();
  
  // TCP streaming only
  xSemaphoreGive( frameSync );

  // Initialize streaming clients queue
  streamingClients = xQueueCreate( MAX_CLIENTS, sizeof(WiFiClient*) );

  //  Creating RTOS task for grabbing frames from the camera with optimized settings
  xTaskCreatePinnedToCore(
      camCB,        // callback
      "cam",        // name
      CAMERA_STACK_SIZE, // optimized stack size for maximum performance
      NULL,         // parameters
      CAMERA_TASK_PRIORITY, // maximum priority for camera capture
      &tCam,        // RTOS task handle
      APP_CPU);     // dedicated core for camera operations

  // Register webserver handling routines with CORS middleware
  server.on("/", HTTP_GET, [](){
    addCORSHeaders();
    handleRoot();
  });
  server.on(STREAMING_URL, HTTP_GET, handleJPGSstream);
  server.on("/control", HTTP_GET, [](){
    addCORSHeaders();
    handleControl();
  });
  server.on("/status", HTTP_GET, [](){
    addCORSHeaders();
    handleStatus();
  });
  server.on("/reset", HTTP_GET, [](){
    addCORSHeaders();
    handleReset();
  });
  server.on("/reboot", HTTP_GET, [](){
    addCORSHeaders();
    handleReboot();
  });
  
  // Handle CORS preflight requests FIRST - before any other handlers
  server.on("/status", HTTP_OPTIONS, [](){
    addCORSHeaders();
    server.send(200, "text/plain", "");
  });
  server.on("/control", HTTP_OPTIONS, [](){
    addCORSHeaders();
    server.send(200, "text/plain", "");
  });
  server.on("/reset", HTTP_OPTIONS, [](){
    addCORSHeaders();
    server.send(200, "text/plain", "");
  });
  server.on("/reboot", HTTP_OPTIONS, [](){
    addCORSHeaders();
    server.send(200, "text/plain", "");
  });
  
  // Universal OPTIONS handler for any other paths
  server.on("*", HTTP_OPTIONS, [](){
    addCORSHeaders();
    server.send(200, "text/plain", "");
  });

  // Serve UI assets from SPIFFS
  server.on("/css/main.css", HTTP_GET, [](){
    File f = SPIFFS.open("/css/main.css", "r");
    if (!f) { server.send(404, "text/plain", "Not found"); return; }
    server.streamFile(f, "text/css");
    f.close();
  });
  server.on("/js/controls.js", HTTP_GET, [](){
    File f = SPIFFS.open("/js/controls.js", "r");
    if (!f) { server.send(404, "text/plain", "Not found"); return; }
    server.streamFile(f, "application/javascript");
    f.close();
  });
  server.on("/js/status.js", HTTP_GET, [](){
    File f = SPIFFS.open("/js/status.js", "r");
    if (!f) { server.send(404, "text/plain", "Not found"); return; }
    server.streamFile(f, "application/javascript");
    f.close();
  });
  
  // Global OPTIONS handler for any unhandled preflight requests
  server.onNotFound([](){
    if (server.method() == HTTP_OPTIONS) {
      addCORSHeaders();
      server.send(200, "text/plain", "");
    } else {
      handleNotFound();
    }
  });

  //  Starting webserver
  server.begin();

  Log.trace("mjpegCB: Starting streaming service\n");
  Log.verbose ("mjpegCB: free heap (start)  : %d\n", ESP.getFreeHeap());

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    server.handleClient();

    //  Minimal delay for better responsiveness - optimized for high FPS
    vTaskDelay(1);  // Just yield to other tasks
  }
}

// ==== Memory allocator that takes advantage of PSRAM if present =======================
char* allocatePSRAM(size_t aSize) {
  if ( psramFound() && ESP.getFreePsram() > aSize ) {
    return (char*) ps_malloc(aSize);
  }
  return NULL;
}

char* allocateMemory(char* aPtr, size_t aSize, bool fail, bool psramOnly) {

  //  Since current buffer is too smal, free it
  if (aPtr != NULL) {
    free(aPtr);
    aPtr = NULL;
  }

  char* ptr = NULL;

  if ( psramOnly ) {
    ptr = allocatePSRAM(aSize);
  }
  else {
    // If memory requested is more than 2/3 of the currently free heap, try PSRAM immediately
    if ( aSize > ESP.getFreeHeap() * 2 / 3 ) {
      ptr = allocatePSRAM(aSize);
    }
    else {
      //  Enough free heap - let's try allocating fast RAM as a buffer
      ptr = (char*) malloc(aSize);

      //  If allocation on the heap failed, let's give PSRAM one more chance:
      if ( ptr == NULL ) ptr = allocatePSRAM(aSize);
    }
  }

  if ( ptr == NULL && fail ) {
    Log.error("allocateMemory: failed to allocate %d bytes\n", aSize);
    ESP.restart();
  }

  return ptr;
}

// handleJPGSstream is implemented in streaming_multiclient_task.cpp

// ==== Handle camera control requests ===================================
void handleControl() {
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;
  
  String var = server.arg("var");
  String val = server.arg("val");
  int intVal = val.toInt();
  
  Log.notice("Camera control request: %s = %s (int: %d)\n", var.c_str(), val.c_str(), intVal);
  
  delay(10);
  
  Preferences prefs;
  bool prefsOpened = prefs.begin("cam", false);
  if (prefsOpened) {
    Log.notice("Camera control: NVS opened successfully for saving\n");
  } else {
    Log.error("Camera control: Failed to open NVS for saving\n");
  }

  if (var == "quality") { 
    res = s->set_quality(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("q", intVal);
      Log.notice("Camera control: Saved quality = %d to NVS\n", intVal);
    }
  }
  else if (var == "contrast") { 
    res = s->set_contrast(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("ct", intVal);
      Log.notice("Camera control: Saved contrast = %d to NVS\n", intVal);
    }
  }
  else if (var == "brightness") { 
    res = s->set_brightness(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("br", intVal);
      Log.notice("Camera control: Saved brightness = %d to NVS\n", intVal);
    }
  }
  else if (var == "saturation") { 
    res = s->set_saturation(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("sa", intVal);
      Log.notice("Camera control: Saved saturation = %d to NVS\n", intVal);
    }
  }
  else if (var == "gainceiling") { 
    res = s->set_gainceiling(s, (gainceiling_t)intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("gc", intVal);
      Log.notice("Camera control: Saved gainceiling = %d to NVS\n", intVal);
    } 
  }
  else if (var == "colorbar") { 
    res = s->set_colorbar(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("cb", intVal);
      Log.notice("Camera control: Saved colorbar = %d to NVS\n", intVal);
    } 
  }
  else if (var == "awb") { 
    res = s->set_whitebal(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("awb", intVal);
      Log.notice("Camera control: Saved awb = %d to NVS\n", intVal);
    } 
  }
  else if (var == "agc") { 
    res = s->set_gain_ctrl(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("agc", intVal);
      Log.notice("Camera control: Saved agc = %d to NVS\n", intVal);
    } 
  }
  else if (var == "aec") { 
    res = s->set_exposure_ctrl(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("aec", intVal);
      Log.notice("Camera control: Saved aec = %d to NVS\n", intVal);
    } 
  }
  else if (var == "hmirror") { 
    res = s->set_hmirror(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("hm", intVal);
      Log.notice("Camera control: Saved hmirror = %d to NVS\n", intVal);
    } 
  }
  else if (var == "vflip") { 
    res = s->set_vflip(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("vf", intVal);
      Log.notice("Camera control: Saved vflip = %d to NVS\n", intVal);
    } 
  }
  else if (var == "awb_gain") { 
    res = s->set_awb_gain(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("awbg", intVal);
      Log.notice("Camera control: Saved awb_gain = %d to NVS\n", intVal);
    } 
  }
  else if (var == "agc_gain") { 
    res = s->set_agc_gain(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("agcg", intVal);
      Log.notice("Camera control: Saved agc_gain = %d to NVS\n", intVal);
    } 
  }
  else if (var == "aec_value") { 
    res = s->set_aec_value(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("aecv", intVal);
      Log.notice("Camera control: Saved aec_value = %d to NVS\n", intVal);
    } 
  }
  else if (var == "aec2") { 
    res = s->set_aec2(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("aec2", intVal);
      Log.notice("Camera control: Saved aec2 = %d to NVS\n", intVal);
    } 
  }
  else if (var == "dcw") { 
    res = s->set_dcw(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("dcw", intVal);
      Log.notice("Camera control: Saved dcw = %d to NVS\n", intVal);
    } 
  }
  else if (var == "bpc") { 
    res = s->set_bpc(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("bpc", intVal);
      Log.notice("Camera control: Saved bpc = %d to NVS\n", intVal);
    } 
  }
  else if (var == "wpc") { 
    res = s->set_wpc(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("wpc", intVal);
      Log.notice("Camera control: Saved wpc = %d to NVS\n", intVal);
    } 
  }
  else if (var == "raw_gma") { 
    res = s->set_raw_gma(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("rg", intVal);
      Log.notice("Camera control: Saved raw_gma = %d to NVS\n", intVal);
    } 
  }
  else if (var == "lenc") { 
    res = s->set_lenc(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("lenc", intVal);
      Log.notice("Camera control: Saved lenc = %d to NVS\n", intVal);
    } 
  }
  else if (var == "special_effect") { 
    res = s->set_special_effect(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("se", intVal);
      Log.notice("Camera control: Saved special_effect = %d to NVS\n", intVal);
    } 
  }
  else if (var == "wb_mode") { 
    res = s->set_wb_mode(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("wb", intVal);
      Log.notice("Camera control: Saved wb_mode = %d to NVS\n", intVal);
    } 
  }
  else if (var == "ae_level") { 
    res = s->set_ae_level(s, intVal); 
    if (res == 0 && prefsOpened) {
      prefs.putInt("ael", intVal);
      Log.notice("Camera control: Saved ae_level = %d to NVS\n", intVal);
    } 
  }
  else {
    Log.error("Camera control: Unknown variable %s\n", var.c_str());
    server.send(400, "text/plain", "Unknown variable");
    if (prefsOpened) prefs.end();
    return;
  }

  if (prefsOpened) {
    prefs.end();
    Log.notice("Camera control: NVS closed and synced to flash\n");
  }

  if (res == 0) {
    Log.notice("Camera control success: %s = %s\n", var.c_str(), val.c_str());
    
// ОТКЛЮЧЕНО: адаптивное качество конфликтует с ручным управлением
//#ifdef ADAPTIVE_JPEG_QUALITY
//    // Применить адаптивное качество после изменения настроек
//    applyAdaptiveQuality();
//#endif
    
    server.send(200, "text/plain", "OK");
  } else {
    Log.error("Camera control failed: %s = %s (error: %d)\n", var.c_str(), val.c_str(), res);
    server.send(500, "text/plain", "Failed to set variable");
  }
}

// ==== Handle status requests ============================================
void handleStatus() {
  sensor_t *s = esp_camera_sensor_get();
  
  // Get actual resolution from current framesize setting
  sensor_t *sensor = esp_camera_sensor_get();
  framesize_t framesize = sensor->status.framesize;
  
  String width = "Unknown";
  String height = "Unknown";
  
  // Map framesize enum to actual dimensions
  switch(framesize) {
    case FRAMESIZE_96X96:     width = "96";   height = "96";   break;
    case FRAMESIZE_QQVGA:     width = "160";  height = "120";  break;
    case FRAMESIZE_QCIF:      width = "176";  height = "144";  break;
    case FRAMESIZE_HQVGA:     width = "240";  height = "176";  break;
    case FRAMESIZE_240X240:   width = "240";  height = "240";  break;
    case FRAMESIZE_QVGA:      width = "320";  height = "240";  break;
    case FRAMESIZE_CIF:       width = "400";  height = "296";  break;
    case FRAMESIZE_HVGA:      width = "480";  height = "320";  break;
    case FRAMESIZE_VGA:       width = "640";  height = "480";  break;
    case FRAMESIZE_SVGA:      width = "800";  height = "600";  break;
    case FRAMESIZE_XGA:       width = "1024"; height = "768";  break;
    case FRAMESIZE_HD:        width = "1280"; height = "720";  break;
    case FRAMESIZE_SXGA:      width = "1280"; height = "1024"; break;
    case FRAMESIZE_UXGA:      width = "1600"; height = "1200"; break;
    case FRAMESIZE_FHD:       width = "1920"; height = "1080"; break;
    case FRAMESIZE_P_HD:      width = "720";  height = "1280"; break;
    case FRAMESIZE_P_3MP:     width = "864";  height = "1536"; break;
    case FRAMESIZE_QXGA:      width = "2048"; height = "1536"; break;
    case FRAMESIZE_QHD:       width = "2560"; height = "1440"; break;
    case FRAMESIZE_WQXGA:     width = "2560"; height = "1600"; break;
    case FRAMESIZE_P_FHD:     width = "1080"; height = "1920"; break;
    case FRAMESIZE_QSXGA:     width = "2560"; height = "1920"; break;
    default:                  width = "Unknown"; height = "Unknown"; break;
  }
  
  // Calculate uptime
  unsigned long uptimeSeconds = millis() / 1000;
  
  String json = "{";
  json += "\"status\":\"OK\",";
  json += "\"cameraFPS\":\"" + String(cameraFPS, 1) + "\",";
  json += "\"frameSize\":\"" + String(currentFrameSize) + "\",";
  json += "\"clients\":\"" + String(clientsConnected) + "\",";
  json += "\"tcpOnly\":\"true\",";
  json += "\"heap\":\"" + String(ESP.getFreeHeap() / 1024) + "\",";
  json += "\"psram\":\"" + String(ESP.getFreePsram() / 1024) + "\",";
  json += "\"uptime\":\"" + String(uptimeSeconds) + "\",";
  json += "\"wifiRSSI\":\"" + String(WiFi.RSSI()) + "\",";
  json += "\"wifiChannel\":\"" + String(WiFi.channel()) + "\",";
  
  // Добавляем информацию о скорости WiFi
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    json += "\"wifiPHY\":\"" + String((ap_info.phy_11n) ? "802.11n" : (ap_info.phy_11g) ? "802.11g" : "802.11b") + "\",";
    json += "\"wifiBandwidth\":\"" + String((ap_info.second != WIFI_SECOND_CHAN_NONE) ? "40MHz" : "20MHz") + "\",";
    
    // Теоретическая максимальная скорость
    int maxSpeed = 0;
    if (ap_info.phy_11n && ap_info.second != WIFI_SECOND_CHAN_NONE) {
      maxSpeed = 150; // 802.11n 40MHz
    } else if (ap_info.phy_11n) {
      maxSpeed = 72;  // 802.11n 20MHz
    } else if (ap_info.phy_11g) {
      maxSpeed = 54;  // 802.11g
    } else {
      maxSpeed = 11;  // 802.11b
    }
    json += "\"wifiMaxSpeed\":\"" + String(maxSpeed) + " Mbps\",";
  } else {
    json += "\"wifiPHY\":\"Unknown\",";
    json += "\"wifiBandwidth\":\"Unknown\",";
    json += "\"wifiMaxSpeed\":\"Unknown\",";
  }
  
  json += "\"currentWidth\":\"" + width + "\",";
  json += "\"currentHeight\":\"" + height + "\",";
  json += "\"settingsLoaded\":\"true\"";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

// ==== Reset camera settings ============================================
void handleReset() {
  Log.notice("Camera reset requested\n");
  
  // Clear NVS settings
  Preferences prefs;
  if (prefs.begin("cam", false)) {
    prefs.clear();
    prefs.end();
    Log.notice("Camera reset: NVS settings cleared\n");
  }
  
  server.send(200, "text/plain", "Camera settings reset. Please reboot to apply defaults.");
}

// ==== Reboot ESP32 =====================================================
void handleReboot() {
  Log.notice("Reboot requested\n");
  server.send(200, "text/plain", "Rebooting...");
  delay(1000);
  ESP.restart();
}

// ==== Serve the main HTML page =========================================
void handleRoot() {
  // Read current settings from NVS
  Preferences prefs;
  int quality = 10, brightness = 0, contrast = 0, saturation = 0;
  int gainceiling = 0, colorbar = 0, awb = 1, agc = 1, aec = 1;
  int hmirror = 0, vflip = 0, awb_gain = 1, agc_gain = 0, aec_value = 300;
  int aec2 = 0, dcw = 1, bpc = 0, wpc = 1, raw_gma = 1, lenc = 1;
  int special_effect = 0, wb_mode = 0, ae_level = 0;
  
  if (prefs.begin("cam", true)) { // read-only
    quality = prefs.getInt("q", 10);
    brightness = prefs.getInt("br", 0);
    contrast = prefs.getInt("ct", 0);
    saturation = prefs.getInt("sa", 0);
    gainceiling = prefs.getInt("gc", 0);
    colorbar = prefs.getInt("cb", 0);
    awb = prefs.getInt("awb", 1);
    agc = prefs.getInt("agc", 1);
    aec = prefs.getInt("aec", 1);
    hmirror = prefs.getInt("hm", 0);
    vflip = prefs.getInt("vf", 0);
    awb_gain = prefs.getInt("awbg", 1);
    agc_gain = prefs.getInt("agcg", 0);
    aec_value = prefs.getInt("aecv", 300);
    aec2 = prefs.getInt("aec2", 0);
    dcw = prefs.getInt("dcw", 1);
    bpc = prefs.getInt("bpc", 0);
    wpc = prefs.getInt("wpc", 1);
    raw_gma = prefs.getInt("rg", 1);
    lenc = prefs.getInt("lenc", 1);
    special_effect = prefs.getInt("se", 0);
    wb_mode = prefs.getInt("wb", 0);
    ae_level = prefs.getInt("ael", 0);
    prefs.end();
  }
  
  String html = String();
  File f = SPIFFS.open("/index.html", "r");
  if (!f) { server.send(500, "text/plain", "index.html not found"); return; }
  html.reserve(f.size());
  while (f.available()) { html += (char)f.read(); }
  f.close();

  // Ranges and numeric values
  html.replace("{{QUALITY}}", String(quality));
  html.replace("{{BRIGHTNESS}}", String(brightness));
  html.replace("{{CONTRAST}}", String(contrast));
  html.replace("{{SATURATION}}", String(saturation));
  html.replace("{{GAINCEILING}}", String(gainceiling));
  html.replace("{{COLORBAR}}", String(colorbar)); // still present as data-value
  html.replace("{{AWB}}", String(awb));
  html.replace("{{AGC}}", String(agc));
  html.replace("{{AEC}}", String(aec));
  html.replace("{{HMIRROR}}", String(hmirror));
  html.replace("{{VFLIP}}", String(vflip));
  html.replace("{{AWB_GAIN}}", String(awb_gain));
  html.replace("{{AGC_GAIN}}", String(agc_gain));
  html.replace("{{AEC_VALUE}}", String(aec_value));
  html.replace("{{AEC2}}", String(aec2));
  html.replace("{{DCW}}", String(dcw));
  html.replace("{{BPC}}", String(bpc));
  html.replace("{{WPC}}", String(wpc));
  html.replace("{{RAW_GMA}}", String(raw_gma));
  html.replace("{{LENC}}", String(lenc));
  html.replace("{{SPECIAL_EFFECT}}", String(special_effect));
  html.replace("{{WB_MODE}}", String(wb_mode));
  html.replace("{{AE_LEVEL}}", String(ae_level));
  // TIMESTAMP for cache busting
  String timestamp = String(millis());
  html.replace("{{TIMESTAMP}}", timestamp);
  html.replace("{{JS_VERSION}}", timestamp);
  
  // Apply checked for checkboxes before page load
  insertCheckedAttribute(html, "colorbar", colorbar == 1);
  insertCheckedAttribute(html, "awb", awb == 1);
  insertCheckedAttribute(html, "agc", agc == 1);
  insertCheckedAttribute(html, "aec", aec == 1);
  insertCheckedAttribute(html, "hmirror", hmirror == 1);
  insertCheckedAttribute(html, "vflip", vflip == 1);
  insertCheckedAttribute(html, "awb_gain", awb_gain == 1);
  insertCheckedAttribute(html, "aec2", aec2 == 1);
  insertCheckedAttribute(html, "dcw", dcw == 1);
  insertCheckedAttribute(html, "bpc", bpc == 1);
  insertCheckedAttribute(html, "wpc", wpc == 1);
  insertCheckedAttribute(html, "raw_gma", raw_gma == 1);
  insertCheckedAttribute(html, "lenc", lenc == 1);

  // Apply selected option for selects before page load (works with single quotes)
  markSelectedOption(html, "gainceiling", gainceiling);
  markSelectedOption(html, "special_effect", special_effect);
  markSelectedOption(html, "wb_mode", wb_mode);

  
  // Prevent caching so the browser doesn't restore old form state
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, post-check=0, pre-check=0, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Last-Modified", "Thu, 01 Jan 1970 00:00:00 GMT");
  server.sendHeader("Vary", "*");
  server.sendHeader("ETag", "\"" + String(millis()) + "\"");
  server.send(200, "text/html", html);
}

// ==== Handle invalid URL requests ============================================
void handleNotFound() {
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text / plain", message);
}
