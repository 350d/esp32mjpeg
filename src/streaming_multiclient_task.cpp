#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// Forward declarations
extern WebServer server;
extern SemaphoreHandle_t frameSync;
extern TaskHandle_t tCam;
extern uint8_t noActiveClients;
extern volatile uint32_t frameNumber;

// Constants
#define KILOBYTE    1024
#define APP_CPU     1
#define PRO_CPU     0

// Task priorities and stack sizes
#define CAMERA_TASK_PRIORITY      (tskIDLE_PRIORITY + 6)
#define STREAM_TASK_PRIORITY      (tskIDLE_PRIORITY + 4)
#define NETWORK_TASK_PRIORITY     (tskIDLE_PRIORITY + 5)
#define WEB_TASK_PRIORITY         (tskIDLE_PRIORITY + 2)

#define CAMERA_STACK_SIZE         (6 * KILOBYTE)
#define STREAM_STACK_SIZE         (5 * KILOBYTE)
#define NETWORK_STACK_SIZE        (6 * KILOBYTE)
#define WEB_STACK_SIZE            (4 * KILOBYTE)

// Memory allocation flags
#define FAIL_IF_OOM true
#define OK_IF_OOM   false
#define PSRAM_ONLY  true
#define ANY_MEMORY  false

// Stream info structure
typedef struct {
  uint32_t        frame;
  WiFiClient      *client;
  TaskHandle_t    task;
  char*           buffer;
  size_t          len;
} streamInfo_t;

// External variables
extern const char* HEADER;
extern const char* BOUNDARY;
extern const char* CTNTTYPE;
extern const int hdrLen;
extern const int bdrLen;
extern const int cntLen;
extern volatile uint32_t clientsConnected;

// Function declarations
char* allocateMemory(char* aPtr, size_t aSize, bool fail = FAIL_IF_OOM, bool psramOnly = ANY_MEMORY);
void streamCB(void * pvParameters);

// Constants for FPS and MAX_CLIENTS
#ifndef FPS
#define FPS 30
#endif

#ifndef MAX_CLIENTS  
#define MAX_CLIENTS 10
#endif

#if defined(CAMERA_MULTICLIENT_TASK)

volatile size_t   camSize;    // size of the current frame, byte
volatile char*    camBuf;      // pointer to the current frame
volatile float    currentFPS = 0.0;   // current delivery FPS for web interface  
volatile float    cameraFPS = 1.0;    // actual camera capture FPS (initialized to avoid 0)
volatile uint32_t currentFrameSize = 0;  // current frame size in KB for web interface
// currentFrameSizeIndex removed - framesize is now fixed at VGA in platformio.ini

// Streaming control variables
QueueHandle_t streamingClients;
volatile bool streamingFlag = false;

#if defined(BENCHMARK)
#define BENCHMARK_PRINT_INT 1000
#endif

// Simple FPS calculation without AverageFilter
uint32_t lastCaptureTime = 0;
uint32_t captureCount = 0;
uint32_t lastPrintCam = millis();

void camCB(void* pvParameters) {

  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);
  
  // Set maximum priority for this camera task
  vTaskPrioritySet(NULL, CAMERA_TASK_PRIORITY);

  //  Pointers to the 2 frames, their respective sizes and index of the current frame
  char* fbs[2] = { NULL, NULL };
  size_t fSize[2] = { 0, 0 };
  int ifb = 0;
  frameNumber = 0;

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    size_t s = 0;
    //  Grab a frame from the camera and query its size
    camera_fb_t* fb = NULL;

    // Always measure capture time for FPS calculation
    uint32_t benchmarkStart = micros();

    s = 0;
    fb = esp_camera_fb_get();
    if ( fb ) {
      s = fb->len;

      //  If frame size is more that we have previously allocated - request exact size for minimal memory usage
      if (s > fSize[ifb]) {
        fSize[ifb] = s;
        fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb], FAIL_IF_OOM, PSRAM_ONLY);  // Force PSRAM for large buffers
      }

      //  Copy current frame into local buffer
      char* b = (char *)fb->buf;
      memcpy(fbs[ifb], b, s);
      esp_camera_fb_return(fb);
    }
    else {
      Serial.printf("camCB: error capturing image for frame %d\n", frameNumber);
      vTaskDelay(1000);
    }

    // Simple FPS calculation - track capture time
    uint32_t captureTime = micros() - benchmarkStart;
    lastCaptureTime = captureTime;

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    //    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow frame copying while switching the current frame
    // ОПТИМИЗАЦИЯ: Неблокирующий захват для максимального FPS
    if ( xSemaphoreTake( frameSync, 0 ) ) {  // Не ждать вообще!
      camBuf = fbs[ifb];
      camSize = s;
      ifb++;
      ifb &= 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
      frameNumber++;
      //  Let anyone waiting for a frame know that the frame is ready
      xSemaphoreGive( frameSync );
    } else {
      // Если семафор занят - просто пропускаем кадр, но обновляем номер
      frameNumber++;
      // Log.trace("camCB: Skipping frame %d due to busy semaphore\n", frameNumber);
    }

    //  Let other (streaming) tasks run with zero delay for maximum FPS
    taskYIELD();  // Просто передать управление без задержки

    //  If streaming task has suspended itself (no active clients to stream to)
    //  there is no need to grab frames from the camera. We can save some juice
    //  by suspedning the tasks
    if ( noActiveClients == 0 ) {
      Serial.printf("mjpegCB: free heap           : %d\n", ESP.getFreeHeap());
      Serial.printf("mjpegCB: min free heap)      : %d\n", ESP.getMinFreeHeap());
      Serial.printf("mjpegCB: max alloc free heap : %d\n", ESP.getMaxAllocHeap());
      Serial.printf("mjpegCB: tCam stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(tCam));
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }

    // Always update cameraFPS for web interface (not just in BENCHMARK mode)
    captureCount++;
    if ( millis() - lastPrintCam > 1000 ) {  // Update every second
      uint32_t currentTime = millis();
      float timeInterval = (float)(currentTime - lastPrintCam) / 1000.0; // seconds
      cameraFPS = (float)captureCount / timeInterval;  // frames per second
      
#if defined(BENCHMARK)
      Serial.printf("mjpegCB: frame capture: %d us, real camera FPS: %.2f\n", lastCaptureTime, cameraFPS);
#endif
      
      lastPrintCam = currentTime;
      captureCount = 0;  // Reset counter
    }


  }
}


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  if ( noActiveClients >= MAX_CLIENTS ) return;
  Serial.printf("handleJPGSstream start: free heap  : %d\n", ESP.getFreeHeap());

  streamInfo_t* info = new streamInfo_t;
  if ( info == NULL ) {
    Serial.printf("handleJPGSstream: cannot allocate stream info - OOM\n");
    return;
  }

  WiFiClient* client = new WiFiClient();
  if ( client == NULL ) {
    Serial.printf("handleJPGSstream: cannot allocate WiFi client for streaming - OOM\n");
    free(info);
    return;
  }

  *client = server.client();
  
  // МАКСИМАЛЬНАЯ ОПТИМИЗАЦИЯ TCP для HD кадров
  client->setNoDelay(true);        // Отключить алгоритм Nagle для мгновенной отправки
  client->setTimeout(100);         // Увеличить таймаут для больших кадров
  
  // Увеличить буферы для HD кадров (~40-50KB) - используем lwIP константы
  // client->setSocketOption(TCP_SND_BUF, 65536);   // Не поддерживается в WiFiClient
  // Полагаемся на системные настройки TCP буферов из platformio.ini

  info->frame = frameNumber - 1;
  info->client = client;
  info->buffer = NULL;
  info->len = 0;

  //  Creating task to push the stream to all connected clients
  int rc = xTaskCreatePinnedToCore(
             streamCB,
             "streamCB",
             STREAM_STACK_SIZE,  // Optimized stack size
             (void*) info,
             STREAM_TASK_PRIORITY,  // Optimized priority for streaming
             &info->task,
             APP_CPU);
  if ( rc != pdPASS ) {
    Serial.printf("handleJPGSstream: error creating RTOS task. rc = %d\n", rc);
    Serial.printf("handleJPGSstream: free heap  : %d\n", ESP.getFreeHeap());
    //    Serial.printf("stk high wm: %d\n", uxTaskGetStackHighWaterMark(tSend));
    delete info;
  }

  noActiveClients++;
  clientsConnected = noActiveClients;  // Update global counter for web interface

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
}


// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency;

  streamInfo_t* info = (streamInfo_t*) pvParameters;

  if ( info == NULL ) {
    Serial.printf("streamCB: a NULL pointer passed");
    delay(5000);
    ESP.restart();
  }

  
  xLastWakeTime = xTaskGetTickCount();
  xFrequency = pdMS_TO_TICKS(1000 / FPS);
  Serial.printf("streamCB: Client Connected\n");

  //  Immediately send this client a header
  info->client->write(HEADER, hdrLen);
  info->client->write(BOUNDARY, bdrLen);

#if defined(BENCHMARK)
  uint32_t streamStart = 0;
  uint32_t waitTime = 0;
  uint32_t streamTime = 0;
  uint32_t frameSize = 0;
  uint32_t lastPrint = millis();
  uint32_t lastFrame = millis();
  float currentStreamFPS = 0.0;
#endif

  for (;;) {
    //  Only send anything if there is someone watching
    if ( info->client->connected() ) {

      if ( info->frame != frameNumber) { // do not send same frame twice

#if defined (BENCHMARK)
        streamStart = micros();
#endif        

        // МАКСИМАЛЬНЫЙ FPS: Блокирующий захват - НЕ пропускаем кадры!
        xSemaphoreTake( frameSync, portMAX_DELAY );
        size_t currentSize = camSize;

#if defined (BENCHMARK)
        waitTime = micros() - streamStart;
        frameSize = currentSize;
        streamStart = micros();
#endif


//  ======================== OPTION1 ==================================
//  server a copy of the buffer - overhead: have to allocate and copy a buffer for all frames

/*
        if ( info->buffer == NULL ) {
          info->buffer = allocateMemory (info->buffer, camSize, FAIL_IF_OOM, ANY_MEMORY);
          info->len = camSize;
        }
        else {
          if ( camSize > info->len ) {
            info->buffer = allocateMemory (info->buffer, camSize, FAIL_IF_OOM, ANY_MEMORY);
            info->len = camSize;
          }
        }
        memcpy(info->buffer, (const void*) camBuf, camSize);
        
        xSemaphoreGive( frameSync );

        sprintf(buf, "%d\r\n\r\n", currentSize);
        info->client->flush();
        info->client->write(CTNTTYPE, cntLen);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) info->buffer, currentSize);
        info->client->write(BOUNDARY, bdrLen);
        info->client->flush(); // Force immediate transmission
*/

//  ======================== OPTION2 ==================================
//  just server the common buffer protected by mutex - MINIMAL LATENCY
// /*
        // МАКСИМАЛЬНАЯ ОПТИМИЗАЦИЯ: Единый буфер для всех данных
        sprintf(buf, "%d\r\n\r\n", camSize);
        
        // Создаем единый буфер для всего кадра
        size_t totalSize = cntLen + strlen(buf) + camSize + bdrLen;
        char* singleBuffer = (char*)malloc(totalSize);
        
        if (singleBuffer) {
          // Собираем все данные в один буфер
          size_t offset = 0;
          memcpy(singleBuffer + offset, CTNTTYPE, cntLen);
          offset += cntLen;
          memcpy(singleBuffer + offset, buf, strlen(buf));
          offset += strlen(buf);
          memcpy(singleBuffer + offset, (char*)camBuf, camSize);
          offset += camSize;
          
          xSemaphoreGive( frameSync );  // Освобождаем семафор раньше
          
          memcpy(singleBuffer + offset, BOUNDARY, bdrLen);
          
          // ОДНА отправка вместо четырех!
          info->client->write(singleBuffer, totalSize);
          info->client->flush();
          

          
          free(singleBuffer);
        } else {
          // Fallback к старому методу если не хватает памяти
          info->client->write(CTNTTYPE, cntLen);
          info->client->write(buf, strlen(buf));
          info->client->write((char*) camBuf, (size_t)camSize);
          xSemaphoreGive( frameSync );
          info->client->write(BOUNDARY, bdrLen);
          info->client->flush();
        }
// */

//  ====================================================================
        info->frame = frameNumber;
#if defined (BENCHMARK)
          streamTime = micros() - streamStart;
#endif        
      }
    }
    else {
      //  client disconnected - clean up.
      noActiveClients--;
      clientsConnected = noActiveClients;  // Update global counter for web interface
      Serial.printf("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      info->client->stop();
      if ( info->buffer ) {
        free( info->buffer );
        info->buffer = NULL;
      }
      delete info->client;
      delete info;
      info = NULL;
      Serial.printf("streamCB: Client disconnected\n");
      vTaskDelay(100);
      vTaskDelete(NULL);
    }
    //  МАКСИМАЛЬНАЯ СКОРОСТЬ: Никаких задержек!
    // Убираем все задержки для максимального FPS

    // Always update frame size for web interface
    currentFrameSize = camSize / 1024; // Convert to KB

#if defined (BENCHMARK)
    // Calculate real FPS based on actual frame delivery, not streaming intervals
    uint32_t currentTime = millis();
    if (info->frame != frameNumber && currentTime > lastFrame) {
      float actualFPS = 1000.0 / (float)(currentTime - lastFrame);
      if (actualFPS < 100.0) { // Filter out unrealistic values
        currentStreamFPS = actualFPS;
        currentFPS = actualFPS; // Update global variable for web interface
      }
      lastFrame = currentTime;
    }

    if ( millis() - lastPrint > BENCHMARK_PRINT_INT ) {
      lastPrint = millis();
      Serial.printf("streamCB: wait=%d us, stream=%d us, frame size=%d bytes, fps=%.2f\n", waitTime, streamTime, frameSize, currentStreamFPS);
    }
#else
    // Simple FPS calculation: count frames delivered to this client
    // Real client-side FPS should be measured in browser
#endif
  }
}

#endif
