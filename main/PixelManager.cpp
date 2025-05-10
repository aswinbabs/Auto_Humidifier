//PixelManager.cpp

#include "PixelManager.hpp"

static const char* TAG = "PixelManager";

PixelManager::PixelManager(uint8_t PIXEL_LED_PIN, uint16_t NUM_LEDS)
            :pixelPin(PIXEL_LED_PIN), numLeds(NUM_LEDS),
            current_mode(Mode::OFF), red(0),
            green(0), blue(0),
            brightness(50)
            {
                //Initialize random for effects 
                srand(time(NULL));
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

    //Create LED strip with RMT
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
            refreshLEDStrip();
            break;
        case Mode::CALM_PULSE:
            updateCalmPulse();
            break;
        case Mode::RAINDROPS:
            updateRainDrops();
            break;
        case Mode::OCEAN_WAVE:
            updateOceanWave();
            break;
        case Mode::GENTLE_WAVE:
            updateGentleWave();
            break;
        case Mode::BREATHING:
            updateBreathing();
            break;
        default:
            ESP_LOGE(TAG, "Invalid mode: %d", mode);
    }
}

void PixelManager::refreshLEDStrip(){
    if(current_mode == Mode::OFF){
        return; //No updates if mode is OFF
    }

    //Apply brightness to colours
    uint8_t bright_r = (red * brightness) / 100;
    uint8_t bright_g = (green * brightness) / 100;
    uint8_t bright_b = (blue * brightness) / 100;

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
        ESP_LOGI(TAG, "All LEDs updated to color RGB(%d, %d, %d) with brightness %d%%", red, green, blue, brightness);
    }

}

void PixelManager::setColourFromBlynk(uint8_t r, uint8_t g, uint8_t b) {
    ESP_LOGI(TAG, "Received RGB update: %d %d %d", r, g, b);

        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

        //Update color values
        red = r;
        green = g;
        blue = b;

        ESP_LOGI(TAG, "Parsed RGB: R=%d, G=%d, B=%d", red, green, blue);
        
        //Only for solid colour mode
        if(current_mode == Mode::SOLID) {
            refreshLEDStrip();
        }
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
    
    if(current_mode == Mode::SOLID) {
        refreshLEDStrip();
    }
    
}

void PixelManager::updateCalmPulse() {
    if(current_mode != Mode::CALM_PULSE) {
        return;
    }

    const int pulse_period_ms = 3000;  //pulse cycle
    const float min_intensity = 0.2f;  //Minimum intensity(percentage of full brightness)

    //get current time in ms
    int64_t time_ms = esp_timer_get_time() / 1000;

    float cycle_position = (float)(time_ms % pulse_period_ms) / pulse_period_ms;

    // Calculate pulse intensity using a sine wave (smooth pulsing)
    // sin() returns values from -1 to 1, we adjust it to range from min_intensity to 1.0
    float pulse_intensity = min_intensity + (1.0f - min_intensity) * (0.5f + 0.5f * sinf(2.0f * M_PI * cycle_position));

    //Define colours for CALM_PULSE
    uint8_t base_r = 10;
    uint8_t base_g = 30;
    uint8_t base_b = 220;

    uint8_t pulse_r = (base_r * brightness * pulse_intensity) / 100;
    uint8_t pulse_g = (base_g * brightness * pulse_intensity) / 100;
    uint8_t pulse_b = (base_b * brightness * pulse_intensity) / 100;

    // Set all LEDs 
    for(int i = 0; i < numLeds; i++) {
        esp_err_t err = led_strip_set_pixel(led_strip, i, pulse_r, pulse_g, pulse_b);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }
    }

    //Refresh strip to display changes
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh the LED strip: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "CALM_PULSE update: intensity=%.2f, RGB(%d,%d,%d)", 
                pulse_intensity, pulse_r, pulse_g, pulse_b);
    }

    vTaskDelay(30 / portTICK_PERIOD_MS);

    if(current_mode == Mode::CALM_PULSE) {
        updateCalmPulse();
    }
}

void PixelManager::blendColors(uint8_t color1_r, uint8_t color1_g, uint8_t color1_b,
                uint8_t color2_r, uint8_t color2_g, uint8_t color2_b,
                float blend_factor, uint8_t* result_r, uint8_t* result_g, uint8_t* result_b) {

    //Ensure blend factor is between 0 and 1
    if(blend_factor < 0.0f) blend_factor = 0.0f;
    if(blend_factor > 1.0f) blend_factor = 1.0f;

    //Linear interpolation between the two colors
    *result_r = color1_r + (color2_r - color1_r) * blend_factor;
    *result_g = color1_g + (color2_g - color1_g) * blend_factor;
    *result_b = color1_b + (color2_b - color1_b) * blend_factor;
}

void PixelManager::updateRainDrops() {
    if(current_mode != Mode::RAINDROPS) {
        return;
    }

    //Parameters for raindrops effect
    const uint8_t drop_chance = 10; // chance out of 100 for new raindrop to appear
    const uint8_t fade_speed = 5;   // Fade speed (higher = faster) 
    const uint8_t max_active_drops = 3; 

    //Raindrops color
    const uint8_t drop_r = 0;
    const uint8_t drop_g = 200;
    const uint8_t drop_b = 240;

    //Background color
    const uint8_t bg_r = 0;
    const uint8_t bg_g = 0;
    const uint8_t bg_b = 5;

    //Static array to track intensities of each LED
    static uint8_t* drop_intensities = NULL;

    //Initialize intensities if its first call
    if(drop_intensities == NULL) {
        drop_intensities = (uint8_t*)malloc(numLeds * sizeof(uint8_t));
    
        if(drop_intensities == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for raindrop intensities");
            return;
        }

        //Initialize all intensities to 0
        for(int i = 0; i < numLeds; i++) {
            drop_intensities[i] = 0;
        }
    }

    //Count current active drops
    uint8_t active_drops = 0;
    for(int i = 0; i < numLeds; i++) {
        if(drop_intensities[i] > 0) {
            active_drops++;
        }
    }

    //Add new raindrop if not reached max
    if(active_drops < max_active_drops) {
        if(rand() % 100 < drop_chance) {
            //Choose random LED pos
            int drop_pos = rand() % numLeds;
            //Make sure to create drop on inactive LED
            if(drop_intensities[drop_pos] == 0) {
                drop_intensities[drop_pos] = 255; //full intensity
                ESP_LOGI(TAG, "New raindrop at position %d", drop_pos);
            }
        }
    }

    // Update all LEDs with their current state
    for(int i = 0; i < numLeds; i++) {
        uint8_t intensity = drop_intensities[i];

        uint8_t r = bg_r + ((drop_r - bg_r) * intensity) / 255;
        uint8_t g = bg_g + ((drop_g - bg_g) * intensity) / 255;
        uint8_t b = bg_g + ((drop_b - bg_b) * intensity) / 255;

        r = (r * brightness) / 100;
        g = (g * brightness) / 100;
        b = (b * brightness) / 100;

        //Set LED color
        esp_err_t err = led_strip_set_pixel(led_strip, i, r, g, b);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }

        //Fade out raindrop for next cycle
        if(intensity > 0) {
            uint16_t new_intensity = intensity - fade_speed;
            //Ensure we don't underflow
            drop_intensities[i] = (new_intensity > 255) ? 0 : new_intensity;
        }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);

    if(current_mode == Mode::RAINDROPS) {
        updateRainDrops();
    }
    else {
        if(drop_intensities != NULL) {
            free(drop_intensities);
            drop_intensities = NULL;
        }
    }

}

void PixelManager::updateOceanWave() {
    if(current_mode != Mode::OCEAN_WAVE) {
        return;
    }

    //Parameters for ocean wave effect
    const uint16_t wave_period_ms = 4000; // Complete wave cycle
    const uint8_t wave_length = numLeds; //Length of one complete wave
    const float wave_amplitude = 0.8f; //Amplitude of color variation (0 - 1)

    //Get current time 
    int64_t time_ms = esp_timer_get_time() / 1000;

    //color1:Deep orange - sunset
    const uint8_t color1_r = 255;
    const uint8_t color1_g = 64;
    const uint8_t color1_b = 0;

    //color2:Vibrant purple
    const uint8_t color2_r = 200;
    const uint8_t color2_g = 0;
    const uint8_t color2_b = 128;

    //color3:Golden yellow
    const uint8_t color3_r = 255;
    const uint8_t color3_g = 215;
    const uint8_t color3_b = 0;

    //Calculate global wave progress (0.0 to 1.0) based on time
    float global_progress = (float)((time_ms % wave_period_ms) / (float)wave_period_ms);

    //Update all LEDs with wave calculated colors
    for (int i = 0; i < numLeds; i++) {
        // Calculate this LED's position in the wave cycle (0.0 to 1.0)
        // Adding the global progress makes the wave appear to move
        float position = fmodf(((float)i / wave_length) + global_progress, 1.0f);
        
        // Calculate wave intensity using a sine wave
        // scaled to range from (0.5-amplitude/2) to (0.5+amplitude/2)
        float wave_intensity = 0.5f + (wave_amplitude/2) * sinf(2.0f * M_PI * position);
        
        // Determine which color pair to blend between based on position in wave
        uint8_t result_r, result_g, result_b;
        
        if (position < 0.33f) {
            // First third: blend between color1 and color2
            float blend = position / 0.33f;
            blendColors(color1_r, color1_g, color1_b, 
                        color2_r, color2_g, color2_b, 
                        blend, &result_r, &result_g, &result_b);
        } 
        else if (position < 0.67f) {
            // Second third: blend between color2 and color3
            float blend = (position - 0.33f) / 0.33f;
            blendColors(color2_r, color2_g, color2_b, 
                        color3_r, color3_g, color3_b, 
                        blend, &result_r, &result_g, &result_b);
        } 
        else {
            // Final third: blend between color3 and color1
            float blend = (position - 0.67f) / 0.33f;
            blendColors(color3_r, color3_g, color3_b, 
                        color1_r, color1_g, color1_b, 
                        blend, &result_r, &result_g, &result_b);
        }
        
        // Further modulate color based on wave intensity
        result_r = result_r * wave_intensity;
        result_g = result_g * wave_intensity;
        result_b = result_b * wave_intensity;
        
        // Apply global brightness
        result_r = (result_r * brightness) / 100;
        result_g = (result_g * brightness) / 100;
        result_b = (result_b * brightness) / 100;
        
        // Set the LED color
        esp_err_t err = led_strip_set_pixel(led_strip, i, result_r, result_g, result_b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }
    }
    // Refresh the LED strip
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "OCEAN_WAVE update: progress = %.2f", global_progress);
    }
    
    // Schedule the next update
    vTaskDelay(30 / portTICK_PERIOD_MS); // ~33 updates per second
    
    // Continue the effect if still in OCEAN_WAVE mode
    if (current_mode == Mode::OCEAN_WAVE) {
        updateOceanWave();
    }
}

void PixelManager::updateGentleWave() {
    if (current_mode != Mode::GENTLE_WAVE) {
        return;
    }
    
    // Parameters for the gentle wave effect
    const uint16_t wave_period_ms = 6000;  // Complete wave cycle 
    const float wave_amplitude = 0.4f;     // Amplitude of wave
    const uint8_t phases = 2;              // Number of wave phases (creates multiple smaller waves)
    
    // Get current time in milliseconds
    int64_t time_ms = esp_timer_get_time() / 1000;
    
    // Color 1: Soft lavender
    const uint8_t color1_r = 180;
    const uint8_t color1_g = 160;
    const uint8_t color1_b = 240;
    
    // Color 2: Gentle mint green
    const uint8_t color2_r = 150;
    const uint8_t color2_g = 222;
    const uint8_t color2_b = 209;
    
    // Calculate global wave progress (0.0 to 1.0) based on time
    float global_progress = (float)((time_ms % wave_period_ms) / (float)wave_period_ms);
    
    // Update all LEDs with their wave-calculated colors
    for (int i = 0; i < numLeds; i++) {
        // Calculate this LED's position in the wave considering multiple phases
        float position = (float)i / (float)numLeds;
    
        // Multiple sine waves are added together to create a more complex pattern
        float wave_val = 0.0f;
        for (int p = 1; p <= phases; p++) {
            wave_val += sinf(2.0f * M_PI * p * position + 
                           global_progress * 2.0f * M_PI) / p;
        }
        // Normalize the result to -1 to 1 range
        wave_val /= 1.5f;  // Empirical factor to keep within range
        
        // Convert to 0-1 range and apply amplitude
        float wave_intensity = 0.5f + wave_amplitude * wave_val;
        
        // Use the wave intensity to blend between the two colors
        uint8_t result_r, result_g, result_b;
        blendColors(color1_r, color1_g, color1_b,
                    color2_r, color2_g, color2_b,
                    wave_intensity, &result_r, &result_g, &result_b);
        
        // Apply global brightness
        result_r = (result_r * brightness) / 100;
        result_g = (result_g * brightness) / 100;
        result_b = (result_b * brightness) / 100;
        
        // Set the LED color
        esp_err_t err = led_strip_set_pixel(led_strip, i, result_r, result_g, result_b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }
    }

    // Refresh the LED strip
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "GENTLE_WAVE update: progress = %.2f", global_progress);
    }
    
    vTaskDelay(40 / portTICK_PERIOD_MS); 
    
    // Continue the effect if still in GENTLE_WAVE mode
    if (current_mode == Mode::GENTLE_WAVE) {
        updateGentleWave();
    }
}

void PixelManager::updateBreathing() {
     if (current_mode != Mode::BREATHING) {
        return;
    }
    
    // Parameters for the breathing effect
    const uint16_t inhale_ms = 2500;    // Time for inhale phase
    const uint16_t exhale_ms = 3500;    // Time for exhale phase (slower than inhale)
    const uint16_t cycle_ms = inhale_ms + exhale_ms;  // Total breathing cycle
    const float min_intensity = 0.05f;  // Minimum intensity when "exhaled"
    
    // Get current time in milliseconds
    int64_t time_ms = esp_timer_get_time() / 1000;
    
    // Calculate position in breathing cycle (0.0 to 1.0)
    float cycle_position = (float)((time_ms % cycle_ms) / (float)cycle_ms);
    
    // Determine which phase we're in (inhale or exhale)
    float inhale_ratio = (float)inhale_ms / cycle_ms;
    bool is_inhale = cycle_position < inhale_ratio;
    
    // Calculate breathing intensity based on phase
    float intensity;
    if (is_inhale) {
        // Inhale phase: progressively brighter (eased)
        float inhale_progress = cycle_position / inhale_ratio;
        // Ease-in-out function for natural breathing curve during inhale
        intensity = min_intensity + (1.0f - min_intensity) * 
                     (inhale_progress * inhale_progress * (3.0f - 2.0f * inhale_progress));
    } else {
        // Exhale phase: progressively dimmer (eased)
        float exhale_progress = (cycle_position - inhale_ratio) / (1.0f - inhale_ratio);
        // Ease-in-out function for natural breathing curve during exhale
        intensity = 1.0f - (1.0f - min_intensity) * 
                    (exhale_progress * exhale_progress * (3.0f - 2.0f * exhale_progress));
    }
    
    // Soft amber/gold color for breathing 
    uint8_t base_r = 255;  // Full red
    uint8_t base_g = 147;  // Partial green (creates amber)
    uint8_t base_b = 41;   // Slight blue (creates warmth)
    
    // Apply intensity to base color
    uint8_t r = base_r * intensity;
    uint8_t g = base_g * intensity;
    uint8_t b = base_b * intensity;
    
    // Apply global brightness
    r = (r * brightness) / 100;
    g = (g * brightness) / 100;
    b = (b * brightness) / 100;
    
    // Set all LEDs to the same breathing color
    for (int i = 0; i < numLeds; i++) {
        esp_err_t err = led_strip_set_pixel(led_strip, i, r, g, b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
        }
    }
    
    // Refresh strip to display changes
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "BREATHING update: phase=%s, intensity=%.2f", 
                 is_inhale ? "inhale" : "exhale", intensity);
    }
    
    // Schedule the next update
    vTaskDelay(40 / portTICK_PERIOD_MS); // ~25 updates per second
    
    // Continue the effect if still in BREATHING mode
    if (current_mode == Mode::BREATHING) {
        updateBreathing();
    }
}