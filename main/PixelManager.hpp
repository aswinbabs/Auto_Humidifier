//PixelManager.hpp
#pragma once

#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <pinDefinitions.hpp>
#include <string>
#include <cmath>
#include <atomic>

class PixelManager {
public:
    // Effects enum
    enum Mode {
        OFF = 0,
        SOLID,
        RAINBOW_RING,
        OCEAN_WAVE,
        BREATHING,
        MODE_COUNT
    };

    // Event types for parameter changes
    enum EventType {
        EVENT_MODE_CHANGE = 0,
        EVENT_COLOR_CHANGE,
        EVENT_BRIGHTNESS_CHANGE,
        EVENT_SHUTDOWN
    };

    struct PixelEvent {
        EventType type;
        union {
            Mode mode;
            struct {
                uint8_t r, g, b;
            } color;
            uint8_t brightness;
        } data;
    };

    PixelManager(uint8_t PIXEL_LED_PIN, uint16_t NUM_LEDS);
    ~PixelManager();
    
    esp_err_t start();
    esp_err_t stop();

    void setMode(Mode mode);
    Mode getMode() const;

    void setBrightness(uint8_t value);
    void setColourFromBlynk(uint8_t r, uint8_t g, uint8_t b);
    void updateModeFromBlynk(int value);
    
    // Get task statistics for monitoring
    uint32_t getTaskHighWaterMark() const;
    bool isTaskRunning() const;

private:
    // RTOS task function
    static void pixelTaskWrapper(void* parameter);
    void pixelTask();
    
    // LED strip operations
    void refreshLEDStrip();
    void applySolidMode();
    void applyOffMode();
    void applyRainbowRingMode();
    void applyOceanWaveMode();
    void applyBreathingMode();

    // Utility functions
    void hsvToRgb(uint16_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b);
    bool sendEvent(const PixelEvent& event);
    
    // Hardware configuration
    led_strip_handle_t led_strip;
    uint8_t pixelPin;
    uint16_t numLeds;

    // Current state (thread-safe access)
    std::atomic<Mode> current_mode;
    std::atomic<uint8_t> red, green, blue;
    std::atomic<uint8_t> brightness;
    
    // RTOS components
    TaskHandle_t pixelTaskHandle;
    QueueHandle_t eventQueue;
    SemaphoreHandle_t stripMutex;
    TimerHandle_t animationTimer;
    
    // Task control
    std::atomic<bool> taskRunning;
    std::atomic<bool> shutdownRequested;
    
    // Animation state variables (protected by task context)
    uint32_t lastAnimationTime;
    float breathingPhase;
    uint16_t rainbowStartHue;
    uint16_t oceanWaveOffset;
    
    // Configuration constants
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 3;
    static constexpr uint32_t EVENT_QUEUE_SIZE = 10;
    static constexpr uint32_t ANIMATION_INTERVAL_MS = 50;
    static constexpr TickType_t MAX_WAIT_TIME = pdMS_TO_TICKS(100);
};