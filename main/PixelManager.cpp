//PixelManager.cpp

#include "PixelManager.hpp"

static const char* TAG = "PixelManager";

PixelManager::PixelManager(uint8_t PIXEL_LED_PIN, uint16_t NUM_LEDS)
            :pixelPin(PIXEL_LED_PIN), numLeds(NUM_LEDS),
            current_mode(Mode::OFF), red(0),
            green(0), blue(0),
            brightness(0)
            {
                
            }

void PixelManager::start() {
    //Configure LED strip
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = pixelPin;
    strip_config.max_leds = numLeds;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;


    //Configure RMT driver (Timing parameter)
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10000000; //10MHz
    rmt_config.flags.with_dma = false;

    //Create LED strip with 
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
    }

    //Clear all LEDS 
    err = led_strip_clear(led_strip);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));        
    }

}

void PixelManager::setMode(Mode mode) {
    current_mode = mode;
    switch(mode){
        case Mode::OFF:
            turnOff();
            break;
        case Mode::SOLID:
            setColor(red, green, blue);
            break;
        default:
            ESP_LOGE(TAG, "Unimplemented mode: %d", mode);
    }
}

PixelManager::Mode PixelManager::getMode() const {
    return current_mode;
}

void PixelManager::setColor(uint8_t r, uint8_t g, uint8_t b){
    red = r;
    green = g;
    blue = b;

    //Apply brightness to colours
    uint8_t bright_r = (r * brightness) / 100;
    uint8_t bright_g = (g * brightness) / 100;
    uint8_t bright_b = (b * brightness) / 100;

    //Set all leds to the specified color
    for (int i = 0; i < numLeds; i++){
        esp_err_t err = led_strip_set_pixel(led_strip, i, bright_r, bright_g, bright_b);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }
    }

    //Refresh strip to display changes
    esp_err_t err = led_strip_refresh(led_strip);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "failed to refresh the LED strip:%s", esp_err_to_name(err));
    }
    else{
        ESP_LOGI(TAG, "All LEDs set to color RGB(%d, %d, %d) with brightness %d%%", r, g, b, brightness);
    }

}

void PixelManager::setColourFromBlynk(uint8_t r, uint8_t g, uint8_t b) {
    ESP_LOGI(TAG, "Received RGB update: %d %d %d", r, g, b);

        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

        red = r;
        green = g;
        blue = b;

        ESP_LOGI(TAG, "Parsed RGB: R=%d, G=%d, B=%d", red, green, blue);
        setColor(red, green, blue);
}


void PixelManager::turnOff() {
    current_mode = Mode::OFF;

    esp_err_t err = led_strip_clear(led_strip);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to refresh LED strip:%s", esp_err_to_name(err));
    }
    else{
        ESP_LOGI(TAG, "All LEDs turned off");
    }

}

void PixelManager::updateModeFromBlynk(int value) {
    ESP_LOGI(TAG, "Received Blynk mode update: %d", value);

    if(value >= 0 && value < Mode::MODE_COUNT){
        Mode new_mode = static_cast<Mode>(value);
        setMode(new_mode);
    }
    else{
        ESP_LOGW(TAG, "Invalid mode:%d", value);
    }
}

void PixelManager::setBrightness(uint8_t value) {
    if(value > 100){
        value = 100;
    }
    if(value < 0){
        value = 0;
    }

    brightness = value;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    setColor(red, green, blue);
}