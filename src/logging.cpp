//  === Simple logging implementation  =================================================================
#include "logging.h"

// Implementation of SimpleLog methods
void SimpleLog::begin(int level, Print* output) {
  _level = level;
  _output = output;
}

void SimpleLog::setPrefix(void (*func)(Print*)) {
  _prefixFunc = func;
}

void SimpleLog::trace(const char* format, ...) {
  if (_level >= 5) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

void SimpleLog::verbose(const char* format, ...) {
  if (_level >= 6) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

void SimpleLog::notice(const char* format, ...) {
  if (_level >= 4) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

void SimpleLog::warning(const char* format, ...) {
  if (_level >= 3) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

void SimpleLog::error(const char* format, ...) {
  if (_level >= 2) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

void SimpleLog::fatal(const char* format, ...) {
  if (_level >= 1) {
    if (_prefixFunc) _prefixFunc(_output);
    va_list args;
    va_start(args, format);
    _output->printf(format, args);
    va_end(args);
  }
}

SimpleLog Log;

// Setup default logging system
void setupLogging() {
#ifndef DISABLE_LOGGING
  Log.begin(LOG_LEVEL, &Serial);
  Log.setPrefix(printTimestampMillis);
  Log.trace("setupLogging()" CR);
#endif  //  #ifndef DISABLE_LOGGING
}


// === millis() - based timestamp ==
void printTimestamp(Print* logOutput) {
  char c[24];
  sprintf(c, "%10lu ", (long unsigned int) MILLIS_FUNCTION);
  logOutput->print(c);
}


// start-time-based timestamp ========
void printTimestampMillis(Print* logOutput) {
  char c[64];
  unsigned long mm = MILLIS_FUNCTION;
  int ms = mm % 1000;
  int s = mm / 1000;
  int m = s / 60;
  int h = m / 60;
  int d = h / 24;
  sprintf(c, "%02d:%02d:%02d:%02d.%03d ", d, h % 24, m % 60, s % 60, ms);
  logOutput->print(c);
}


#ifndef DISABLE_LOGGING
void printBuffer(const char* aBuf, size_t aSize) {
    Serial.println("Buffer contents:");

    char c;
    String s;
    size_t cnt = 0;

    for (int j = 0; j < aSize / 16 + 1; j++) {
      Serial.printf("%04x : ", j * 16);
      for (int i = 0; i < 16 && cnt < aSize; i++) {
        c = aBuf[cnt++];
        Serial.printf("%02x ", c);
        if (c < 32) {
          s += '.';
        }
        else {
          s += c;
        }
      }
      Serial.print(" : ");
      Serial.println(s);
      s = "";
    }
    Serial.println();
}
#else
void printBuffer(const char* aBuf, size_t aSize) {}
#endif
