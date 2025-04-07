#include "DHTSensor.hpp"
#include "esp_log.h"

static const char* TAG = "DHTSensor";

DHTSensor::DHTSensor(gpio_num_t dhtPin): dhtControlPin(dhtPin), temperature(0.0f), humidity(0.0f), readSuccess(false){
    //Initialize GPIO for dht sensor
    conf_DHTGPIO();
}

float DHTSensor::getTemperature() const{
    return temperature;
}

float DHTSensor::getHumidity() const{
    return humidity;
}

void DHTSensor::conf_DHTGPIO(){
    gpio_config_t dht_conf{};
    dht_conf.pin_bit_mask = (1ULL << dhtControlPin);
    dht_conf.mode = GPIO_MODE_INPUT;
    dht_conf.pull_up_en  = GPIO_PULLUP_DISABLE;
    dht_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    dht_conf.intr_type = GPIO_INTR_DISABLE;
    
    esp_err_t err = gpio_config(&dht_conf);
    if(err == ESP_OK){
        ESP_LOGI(TAG, "DHT11 GPIO initialized successfully");
    }
    else{
        ESP_LOGE(TAG, "DHT11 failed to initialize, err:%s",esp_err_to_name(err));
    }
}

bool DHTSensor::isReadSuccessful() const {
    return readSuccess;
}

void DHTSensor::start(){
    // TaskHandle_t taskHandle = nullptr;
    BaseType_t result = xTaskCreate(
        dht_task,
        "dht_task",
        4096, 
        this,
        1,
        nullptr);
    
    if(result != pdPASS){
        ESP_LOGE(TAG, "Failed to create dht_task");
    }
    else{
        ESP_LOGI(TAG, "Successfully created dht_task");
    }  
}

void DHTSensor::dht_task(void* pvParameters){
    DHTSensor* dhtController = static_cast<DHTSensor*>(pvParameters);
    const TickType_t xDelay = pdMS_TO_TICKS(2000); //2 seconds
    while(true){
        dhtController->DhtRead();
        vTaskDelay(xDelay);
    }
}

void DHTSensor::DhtRead(){
    const int maxTries = 3;
    int attempt = 0;
    esp_err_t result;
    float temp, hum;

    while(attempt < maxTries){
        result = dht_read_float_data(DHT_TYPE_DHT11, dhtControlPin, &hum, &temp);
        if(result == ESP_OK){
            temperature = temp;
            humidity = hum;
            readSuccess = true;
            ESP_LOGI(TAG, "DHT11 read success [Attempt %d]: Temp = %.2f °C, Humidity = %.2f%%", attempt+1, temperature, humidity);
            return;
        }
        else{
            readSuccess = false;
            ESP_LOGW(TAG, "DHT11 read failed [Attempt %d]: %s", attempt + 1, esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(1000)); //Delay between retries
        }
        attempt++;
    }
    
    ESP_LOGE(TAG, "DHT11 read failed after %d attempts", maxTries);
}