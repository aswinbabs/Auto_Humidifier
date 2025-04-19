//PixelManager.hpp
#pragma once

#include "esp_log.h"
#include "led_strip.h"
#include "stdint.h"
#include <pinDefinitions.hpp>
#include "driver/gpio.h"




class PixelManager{
public:
    //Effects enum
    enum Mode {
        OFF = 0,
        SOLID,
        CALM_PULSE,
        RAINDROPS,
        OCEAN_WAVE,
        GENTLE_WAVE,
        BREATHING,
        MODE_COUNT
    };

    PixelManager(uint8_t PIXEL_LED_PIN, uint16_t NUM_LEDS);
    void start();
    void setMode(Mode mode);
    Mode getMode() const;
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void turnOff();
    void updateModeFromBlynk(int value);
    void setBrightness(uint8_t value);

private:
   led_strip_handle_t led_strip;
   uint8_t pixelPin;
   uint16_t numLeds;
   Mode current_mode;

   //color values
   uint8_t red;
   uint8_t green;
   uint8_t blue;
   uint8_t brightness;

};