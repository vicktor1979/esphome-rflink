#pragma once
#include <cstdio>
#define ESP_LOGCONFIG(tag, fmt, ...) do { std::printf("[%s] " fmt "\n",tag, ##__VA_ARGS__); } while(0)
#define ESP_LOGI ESP_LOGCONFIG
#define ESP_LOGD ESP_LOGCONFIG

#define ESP_LOGE ESP_LOGCONFIG
#define ESP_LOGW ESP_LOGCONFIG
