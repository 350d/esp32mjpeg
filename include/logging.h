#pragma once
#include <Arduino.h>

#define MILLIS_FUNCTION xTaskGetTickCount()
// #define MILLIS_FUNCTION millis()

// Simple logging class to replace ArduinoLog
class SimpleLog {
public:
  void begin(int level, Print* output);
  void setPrefix(void (*func)(Print*));
  void trace(const char* format, ...);
  void verbose(const char* format, ...);
  void notice(const char* format, ...);
  void warning(const char* format, ...);
  void error(const char* format, ...);
  void fatal(const char* format, ...);
  
private:
  int _level = 6;
  Print* _output = &Serial;
  void (*_prefixFunc)(Print*) = nullptr;
};

extern SimpleLog Log;

// ==== prototypes ===============================
void    setupLogging();

#ifndef DISABLE_LOGGING
void    printTimestampMillis(Print* logOutput);
void    printBuffer(const char* aBuf, size_t aSize);
#endif  //   #ifndef DISABLE_LOGGING
