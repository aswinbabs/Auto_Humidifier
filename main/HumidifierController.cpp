//HumidifierController.cpp
#include "HumidifierController.hpp"
#include "DHTSensor.hpp"
#include "BlynkManager.hpp"
#include "esp_log.h"

static const char* TAG = "HUMIDIFIER";

HumidifierController::HumidifierController(DHTSensor* dhtSensor, BlynkManager* blynkManager, gpio_num_t humPin) 
    : dhtSensor(dhtSensor), blynkManager(blynkManager), humControlPin(humPin), humidifierState(false) {
    //Initialize GPIO pin for Humidifier
    conf_HumidifierGPIO();
}

void HumidifierController::conf_HumidifierGPIO(){
    gpio_config_t humid_conf = {};
    humid_conf.pin_bit_mask = (1ULL << humControlPin);
    humid_conf.mode = GPIO_MODE_OUTPUT;
    humid_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    humid_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    humid_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&humid_conf);
    if(err == ESP_OK){
        ESP_LOGI(TAG, "Humidifier GPIO initialized successfully");
    }
    else{
        ESP_LOGE(TAG, "Failed to initialize Humidifier GPIO, err:%s", esp_err_to_name(err));
    }
}

void HumidifierController::turnOn(){
    if(!humidifierState){
        gpio_set_level(humControlPin, 1);
        humidifierState = true;
        ESP_LOGI(TAG, "Humidifier turned ON, Pin: %d, State: %s", humControlPin, humidifierState ? "ON" : "OFF");
    }
}

void HumidifierController::turnOff(){
    if(humidifierState){
        gpio_set_level(humControlPin, 0);
        humidifierState = false;
        ESP_LOGI(TAG, "Humidifier turned OFF, Pin: %d, State: %s", humControlPin, humidifierState ? "ON" : "OFF");
    }
}

bool HumidifierController::getState() const {
    return humidifierState;
}

void HumidifierController::start(){
    //pass the current instance as pvParameters
    BaseType_t result = xTaskCreate(
        HMD_ControlTask,
        "HMD_ControlTask",
        4096,
        this,
        1,
        nullptr);

    if(result != pdPASS){
        ESP_LOGE(TAG, "Failed to create HMD_controlTask");
    }
    else{
        ESP_LOGI(TAG, "Successfully created HMD_controlTask");
    }
}

void HumidifierController::HMD_ControlTask(void* pvParameters){
    //cast the pointer back to HumidifierController instance
    HumidifierController* controller = static_cast<HumidifierController*>(pvParameters);
    const TickType_t xDelay = pdMS_TO_TICKS(2000);

    while(true){
        // Check if we're in auto or manual mode
        bool isAutoMode = controller->blynkManager->isAutoMode();
        
        if (isAutoMode) {
            // AUTO MODE: Control based on sensor readings and threshold
            if(!controller->dhtSensor->isReadSuccessful()){
                ESP_LOGE(TAG, "Failed to read temperature from DHT sensor!");
                controller->turnOff(); // safe fallback
            }
            else{
                float humidity = controller->dhtSensor->getHumidity();
                if (humidity > 0.0 && humidity < 100.0) {  
                    if(humidity < controller->humidityThreshold){
                        controller->turnOn();
                        ESP_LOGI(TAG, "[AUTO] Room humidity %.2f%% below threshold %.2f%%, Humidifier: ON", 
                               humidity, controller->humidityThreshold);
                    }
                    else{
                        controller->turnOff();
                        ESP_LOGI(TAG, "[AUTO] Room humidity %.2f%% above threshold %.2f%%, Humidifier: OFF", 
                               humidity, controller->humidityThreshold);  
                    }
                } else {
                    ESP_LOGW(TAG, "Invalid humidity reading: %.2f, skipping humidifier control", humidity);
                    // Keep previous humidifier state
                }
                
            }
        } 
        else {
            // MANUAL MODE: Control based on manual switch state (V2)
            bool manualSwitchOn = controller->blynkManager->isManualSwitchOn();
            
            if (manualSwitchOn) {
                controller->turnOn();
                ESP_LOGI(TAG, "[MANUAL] Switch V2 is ON, Humidifier: ON");
            } else {
                controller->turnOff();
                ESP_LOGI(TAG, "[MANUAL] Switch V2 is OFF, Humidifier: OFF");
            }
        }
        
        vTaskDelay(xDelay);
    }
}