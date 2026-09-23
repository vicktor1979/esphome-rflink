#pragma once
inline void capture_test_log(const char*, const char*, ...) __attribute__((format(printf,2,3)));
inline void capture_test_log(const char*, const char*, ...) {}
#define ESP_LOGI(...) capture_test_log(__VA_ARGS__)
#define ESP_LOGD(...) capture_test_log(__VA_ARGS__)
#define ESP_LOGW(...) capture_test_log(__VA_ARGS__)
#define ESP_LOGE(...) capture_test_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) capture_test_log(__VA_ARGS__)
#define LOG_PIN(msg,pin) ((void)(pin))
