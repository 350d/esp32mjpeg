#pragma once
#include "definitions.h"
#include "references.h"

typedef struct {
  uint32_t        frame;
  WiFiClient      *client;
  TaskHandle_t    task;
  char*           buffer;
  size_t          len;
} streamInfo_t;

typedef struct {
  uint8_t   cnt;  // served to clients counter. when equal to number of active clients, could be deleted
  void*     nxt;  // next chunck
  uint32_t  fnm;  // frame number
  uint32_t  siz;  // frame size
  uint8_t*  dat;  // frame pointer
} frameChunck_t;


void camCB(void* pvParameters);
void handleJPGSstream(void);
void handleRoot(void);
void handleNotFound(void);
void handleControl(void);
void handleStatus(void);
void handleReset(void);
void handleReboot(void);

// UI asset handlers
void handleCSS(void);
void handleJSControls(void);
void handleJSFPS(void);
void handleJSStatus(void);

void streamCB(void * pvParameters);
void mjpegCB(void * pvParameters);

#define FAIL_IF_OOM true
#define OK_IF_OOM   false
#define PSRAM_ONLY  true
#define ANY_MEMORY  false
char* allocateMemory(char* aPtr, size_t aSize, bool fail = FAIL_IF_OOM, bool psramOnly = ANY_MEMORY);

extern const char* HEADER;
extern const char* BOUNDARY;
extern const char* CTNTTYPE;
extern const int hdrLen;
extern const int bdrLen;
extern const int cntLen;
extern volatile uint32_t frameNumber;
extern volatile uint32_t clientsConnected;
extern volatile float currentFPS;
extern volatile float cameraFPS;
extern volatile uint32_t currentFrameSize;
// currentFrameSizeIndex removed - framesize is now fixed at VGA in platformio.ini

extern frameChunck_t* fstFrame;
extern frameChunck_t* curFrame;

// Streaming control variables
extern QueueHandle_t streamingClients;
extern volatile bool streamingFlag;
extern TaskHandle_t tStream; 
