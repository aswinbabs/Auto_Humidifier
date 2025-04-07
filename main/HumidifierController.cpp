//HumidifierController.cpp
#include "HumidifierController.hpp"
#include "DHTSensor.hpp"
#include "esp_log.h"

static const char* TAG = "HUMIDIFIER";

HumidifierController::HumidifierController(DHTSensor* dhtSensor, gpio_num_t humPin) : dhtSensor(dhtSensor), humControlPin(humPin), humidifierState(false){
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
    //TaskHandle_t taskHandle = nullptr;
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

//Static task
void HumidifierController::HMD_ControlTask(void* pvParameters){
    //cast the pointer back to HumidifierController instance
    HumidifierController* controller = static_cast<HumidifierController*>(pvParameters);
    const TickType_t xDelay = pdMS_TO_TICKS(500);

    while(true){

        if(!controller->dhtSensor->isReadSuccessful()){
            ESP_LOGE(TAG, "Failed to read temperature  from DHT sensor!");
            controller->turnOff(); //safe fallback
        }
        else{
            float temp = controller->dhtSensor->getTemperature();
            if(temp > controller->tempThreshold){
                controller->turnOn();
                ESP_LOGW(TAG, "Room temperature %.2f °C exceeds threshold %.2f °C", temp, controller->tempThreshold);
            }
            else{
                controller->turnOff();
                ESP_LOGI(TAG, "Room temperature %.2f °C below threshold %.2f °C", temp, controller->tempThreshold);  
            }
        }
        vTaskDelay(xDelay);
    }
}