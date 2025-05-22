//PixelManager.cpp

#include "PixelManager.hpp"
#include "esp_timer.h"

static const char* TAG = "PixelManager";

PixelManager::PixelManager(uint8_t PIXEL_LED_PIN, uint16_t NUM_LEDS)
    : pixelPin(PIXEL_LED_PIN), numLeds(NUM_LEDS),
      current_mode(Mode::OFF), red(0), green(0), blue(0), brightness(50),
      pixelTaskHandle(nullptr), eventQueue(nullptr), stripMutex(nullptr),
      animationTimer(nullptr), taskRunning(false), shutdownRequested(false),
      lastAnimationTime(0), breathingPhase(0.0f), rainbowStartHue(0), oceanWaveOffset(0) {
}

PixelManager::~PixelManager() {
    stop();
}

esp_err_t PixelManager::start() {
    ESP_LOGI(TAG, "Starting PixelManager with %d LEDs on pin %d", numLeds, pixelPin);
    
    // Configure LED strip
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = pixelPin;
    strip_config.max_leds = numLeds;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;

    // Configure RMT driver
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10000000; // 10MHz
    rmt_config.flags.with_dma = false;

    // Create LED strip with RMT
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
        return err;
    }

    // Clear all LEDs initially
    err = led_strip_clear(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));
        return err;
    }
    
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
        return err;
    }

    // Create RTOS components
    eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(PixelEvent));
    if (eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    stripMutex = xSemaphoreCreateMutex();
    if (stripMutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create strip mutex");
        vQueueDelete(eventQueue);
        return ESP_ERR_NO_MEM;
    }

    // Create pixel management task
    BaseType_t result = xTaskCreate(
        pixelTaskWrapper,
        "PixelTask",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &pixelTaskHandle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pixel task");
        vSemaphoreDelete(stripMutex);
        vQueueDelete(eventQueue);
        return ESP_ERR_NO_MEM;
    }

    taskRunning = true;
    ESP_LOGI(TAG, "PixelManager started successfully");
    return ESP_OK;
}

esp_err_t PixelManager::stop() {
    if (!taskRunning) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping PixelManager");
    
    // Signal shutdown
    shutdownRequested = true;
    PixelEvent shutdownEvent = { EVENT_SHUTDOWN, {} };
    sendEvent(shutdownEvent);

    // Wait for task to finish
    if (pixelTaskHandle != nullptr) {
        // Wait up to 1 second for task to finish
        for (int i = 0; i < 100 && taskRunning; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        if (taskRunning) {
            ESP_LOGW(TAG, "Force deleting pixel task");
            vTaskDelete(pixelTaskHandle);
        }
        pixelTaskHandle = nullptr;
    }

    // Clean up RTOS components
    if (stripMutex != nullptr) {
        vSemaphoreDelete(stripMutex);
        stripMutex = nullptr;
    }
    
    if (eventQueue != nullptr) {
        vQueueDelete(eventQueue);
        eventQueue = nullptr;
    }

    // Turn off all LEDs and clean up hardware
    if (led_strip != nullptr) {
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        led_strip_del(led_strip);
        led_strip = nullptr;
    }

    taskRunning = false;
    ESP_LOGI(TAG, "PixelManager stopped");
    return ESP_OK;
}

void PixelManager::pixelTaskWrapper(void* parameter) {
    PixelManager* manager = static_cast<PixelManager*>(parameter);
    manager->pixelTask();
}

void PixelManager::pixelTask() {
    PixelEvent event;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t animationDelay = pdMS_TO_TICKS(ANIMATION_INTERVAL_MS);
    
    ESP_LOGI(TAG, "Pixel task started");

    while (!shutdownRequested) {
        bool eventReceived = false;
        
        // Check for events with timeout
        if (xQueueReceive(eventQueue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            eventReceived = true;
            
            switch (event.type) {
                case EVENT_SHUTDOWN:
                    ESP_LOGI(TAG, "Shutdown event received");
                    goto task_exit;
                    
                case EVENT_MODE_CHANGE:
                    ESP_LOGI(TAG, "Mode change event: %d", event.data.mode);
                    current_mode = event.data.mode;
                    // Reset animation state when mode changes
                    breathingPhase = 0.0f;
                    rainbowStartHue = 0;
                    oceanWaveOffset = 0;
                    refreshLEDStrip();
                    break;
                    
                case EVENT_COLOR_CHANGE:
                    ESP_LOGI(TAG, "Color change event: R:%d G:%d B:%d", 
                            event.data.color.r, event.data.color.g, event.data.color.b);
                    red = event.data.color.r;
                    green = event.data.color.g;
                    blue = event.data.color.b;
                    if (current_mode == SOLID || current_mode == BREATHING) {
                        refreshLEDStrip();
                    }
                    break;
                    
                case EVENT_BRIGHTNESS_CHANGE:
                    ESP_LOGI(TAG, "Brightness change event: %d", event.data.brightness);
                    brightness = event.data.brightness;
                    refreshLEDStrip();
                    break;
            }
        }

        // Handle animations for modes that need continuous updates
        Mode currentMode = current_mode.load();
        if (currentMode == RAINBOW_RING || currentMode == OCEAN_WAVE || currentMode == BREATHING) {
            uint32_t currentTime = esp_timer_get_time() / 1000; // Convert to ms
            
            // Update animation at regular intervals
            if (currentTime - lastAnimationTime >= ANIMATION_INTERVAL_MS || eventReceived) {
                refreshLEDStrip();
                lastAnimationTime = currentTime;
            }
        }

        // Use delay until to maintain consistent timing
        vTaskDelayUntil(&lastWakeTime, animationDelay);
    }

task_exit:
    taskRunning = false;
    ESP_LOGI(TAG, "Pixel task exiting");
    vTaskDelete(nullptr);
}

bool PixelManager::sendEvent(const PixelEvent& event) {
    if (eventQueue == nullptr) {
        return false;
    }
    
    return xQueueSend(eventQueue, &event, MAX_WAIT_TIME) == pdTRUE;
}

void PixelManager::setMode(Mode mode) {
    ESP_LOGI(TAG, "Setting mode to %d", mode);
    
    if (mode >= MODE_COUNT) {
        ESP_LOGW(TAG, "Invalid mode %d, ignoring", mode);
        return;
    }
    
    if (mode != current_mode.load()) {
        PixelEvent event = { EVENT_MODE_CHANGE, {} };
        event.data.mode = mode;
        
        if (!sendEvent(event)) {
            ESP_LOGW(TAG, "Failed to send mode change event");
        }
    }
}

PixelManager::Mode PixelManager::getMode() const {
    return current_mode.load();
}

void PixelManager::setBrightness(uint8_t value) {
    if (value != brightness.load()) {
        ESP_LOGI(TAG, "Brightness changed from %d to %d", brightness.load(), value);
        
        PixelEvent event = { EVENT_BRIGHTNESS_CHANGE, {} };
        event.data.brightness = value;
        
        if (!sendEvent(event)) {
            ESP_LOGW(TAG, "Failed to send brightness change event");
        }
    }
}

void PixelManager::setColourFromBlynk(uint8_t r, uint8_t g, uint8_t b) {
    if (r != red.load() || g != green.load() || b != blue.load()) {
        ESP_LOGI(TAG, "Color changed from (R:%d G:%d B:%d) to (R:%d G:%d B:%d)",
                red.load(), green.load(), blue.load(), r, g, b);
        
        PixelEvent event = { EVENT_COLOR_CHANGE, {} };
        event.data.color.r = r;
        event.data.color.g = g;
        event.data.color.b = b;
        
        if (!sendEvent(event)) {
            ESP_LOGW(TAG, "Failed to send color change event");
        }
    }
}

void PixelManager::updateModeFromBlynk(int value) {
    Mode newMode;
    switch (value) {
        case 0: newMode = OFF; break;
        case 1: newMode = SOLID; break;
        case 2: newMode = RAINBOW_RING; break;
        case 3: newMode = OCEAN_WAVE; break;
        case 4: newMode = BREATHING; break;
        default:
            ESP_LOGW(TAG, "Invalid mode value from Blynk: %d", value);
            return;
    }
    
    setMode(newMode);
}

void PixelManager::refreshLEDStrip() {
    if (xSemaphoreTake(stripMutex, MAX_WAIT_TIME) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire strip mutex");
        return;
    }

    Mode currentMode = current_mode.load();
    
    switch (currentMode) {
        case OFF:
            applyOffMode();
            break;
        case SOLID:
            applySolidMode();
            break;
        case RAINBOW_RING:
            applyRainbowRingMode();
            break;
        case OCEAN_WAVE:
            applyOceanWaveMode();
            break;
        case BREATHING:
            applyBreathingMode();
            break;
        default:
            ESP_LOGW(TAG, "Unknown mode %d, defaulting to OFF", currentMode);
            applyOffMode();
            break;
    }

    xSemaphoreGive(stripMutex);
}

void PixelManager::applyOffMode() {
    esp_err_t err = led_strip_clear(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));
        return;
    }
    
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
    }
}

void PixelManager::applySolidMode() {
    uint8_t currentBrightness = brightness.load();
    uint8_t currentRed = red.load();
    uint8_t currentGreen = green.load();
    uint8_t currentBlue = blue.load();
    
    uint8_t scaledRed   = (currentRed * currentBrightness) / 255;
    uint8_t scaledGreen = (currentGreen * currentBrightness) / 255;
    uint8_t scaledBlue  = (currentBlue * currentBrightness) / 255;

    for (uint16_t i = 0; i < numLeds; ++i) {
        esp_err_t err = led_strip_set_pixel(led_strip, i, scaledRed, scaledGreen, scaledBlue);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
            return;
        }
    }

    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
    }
}

void PixelManager::hsvToRgb(uint16_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float hf = h / 60.0f;
    int i = static_cast<int>(hf) % 6;
    float f = hf - i;

    float p = v * (1 - s / 255.0f);
    float q = v * (1 - f * s / 255.0f);
    float t = v * (1 - (1 - f) * s / 255.0f);

    float r_f, g_f, b_f;
    switch (i) {
        case 0: r_f = v; g_f = t; b_f = p; break;
        case 1: r_f = q; g_f = v; b_f = p; break;
        case 2: r_f = p; g_f = v; b_f = t; break;
        case 3: r_f = p; g_f = q; b_f = v; break;
        case 4: r_f = t; g_f = p; b_f = v; break;
        case 5: r_f = v; g_f = p; b_f = q; break;
        default: r_f = g_f = b_f = 0; break;
    }

    r = static_cast<uint8_t>(r_f);
    g = static_cast<uint8_t>(g_f);
    b = static_cast<uint8_t>(b_f);
}

void PixelManager::applyRainbowRingMode() {
    uint8_t currentBrightness = brightness.load();
    
    for (uint16_t i = 0; i < numLeds; ++i) {
        uint16_t hue = (rainbowStartHue + i * (360 / numLeds)) % 360;
        uint8_t r, g, b;
        hsvToRgb(hue, 255, currentBrightness, r, g, b);
        
        esp_err_t err = led_strip_set_pixel(led_strip, i, r, g, b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
            return;
        }
    }
    
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
        return;
    }
    
    rainbowStartHue = (rainbowStartHue + 3) % 360; // Advance for ring effect
}

void PixelManager::applyOceanWaveMode() {
    uint8_t currentBrightness = brightness.load();
    const uint16_t waveLength = 30;

    for (uint16_t i = 0; i < numLeds; ++i) {
        uint16_t wavePos = (i + oceanWaveOffset) % waveLength;
        float brightnessFactor = (sin(2 * M_PI * wavePos / waveLength) + 1.0f) / 2.0f;

        uint8_t r = static_cast<uint8_t>(0 * brightnessFactor * currentBrightness / 255);
        uint8_t g = static_cast<uint8_t>(100 * brightnessFactor * currentBrightness / 255);
        uint8_t b = static_cast<uint8_t>(200 * brightnessFactor * currentBrightness / 255);

        esp_err_t err = led_strip_set_pixel(led_strip, i, r, g, b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
            return;
        }
    }
    
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
        return;
    }

    oceanWaveOffset = (oceanWaveOffset + 1) % waveLength;
}

void PixelManager::applyBreathingMode() {
    uint8_t currentBrightness = brightness.load();
    uint8_t currentRed = red.load();
    uint8_t currentGreen = green.load();
    uint8_t currentBlue = blue.load();
    
    const float speed = 0.1f; // Breathing speed
    
    // Calculate breathing factor (0.1 to 1.0 for smooth breathing)
    float breathFactor = (sin(breathingPhase) + 1.0f) / 2.0f;
    breathFactor = 0.1f + breathFactor * 0.9f; // Scale to 0.1-1.0 range
    
    uint8_t scaledRed   = static_cast<uint8_t>(currentRed * currentBrightness * breathFactor / 255.0f);
    uint8_t scaledGreen = static_cast<uint8_t>(currentGreen * currentBrightness * breathFactor / 255.0f);
    uint8_t scaledBlue  = static_cast<uint8_t>(currentBlue * currentBrightness * breathFactor / 255.0f);

    for (uint16_t i = 0; i < numLeds; ++i) {
        esp_err_t err = led_strip_set_pixel(led_strip, i, scaledRed, scaledGreen, scaledBlue);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
            return;
        }
    }

    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
        return;
    }

    breathingPhase += speed;
    if (breathingPhase > 2 * M_PI) {
        breathingPhase -= 2 * M_PI;
    }
}

uint32_t PixelManager::getTaskHighWaterMark() const {
    if (pixelTaskHandle != nullptr) {
        return uxTaskGetStackHighWaterMark(pixelTaskHandle);
    }
    return 0;
}

bool PixelManager::isTaskRunning() const {
    return taskRunning.load();
}