#pragma once

// ESP32 has two cores: APPlication core and PROcess core (the one that runs ESP32 SDK stack)
#define APP_CPU     1
#define PRO_CPU     0

// WiFi optimization includes
#include "esp_wifi.h"

#define KILOBYTE    1024

#define SERIAL_RATE 115200

// === СБАЛАНСИРОВАННЫЕ ПРИОРИТЕТЫ RTOS ===
#define CAMERA_TASK_PRIORITY      (tskIDLE_PRIORITY + 6)  // Высокий приоритет для камеры
#define STREAM_TASK_PRIORITY      (tskIDLE_PRIORITY + 4)  // Средний приоритет для стриминга  
#define NETWORK_TASK_PRIORITY     (tskIDLE_PRIORITY + 5)  // Средне-высокий приоритет для сети
#define WEB_TASK_PRIORITY         (tskIDLE_PRIORITY + 2)  // Низкий приоритет для веб-сервера

// === ПРОВЕРЕННЫЕ РАЗМЕРЫ СТЕКА ===
#define CAMERA_STACK_SIZE         (6 * KILOBYTE)  // 6KB для камеры
#define STREAM_STACK_SIZE         (5 * KILOBYTE)   // 5KB для стриминга
#define NETWORK_STACK_SIZE        (6 * KILOBYTE)   // 6KB для сети
#define WEB_STACK_SIZE            (4 * KILOBYTE)   // 4KB для веб-сервера

// === АДАПТИВНОЕ КАЧЕСТВО JPEG ===
#ifdef ADAPTIVE_JPEG_QUALITY
#define MIN_JPEG_QUALITY          6    // Минимальное качество при высокой нагрузке
#define MAX_JPEG_QUALITY          4    // Максимальное качество при низкой нагрузке
#define QUALITY_THRESHOLD_CLIENTS 3    // Порог клиентов для переключения качества
#define QUALITY_THRESHOLD_HEAP    50000 // Порог свободной памяти (байты)
#endif
