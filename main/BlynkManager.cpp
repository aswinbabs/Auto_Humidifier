//Blynkmanager.cpp

#include "BlynkManager.hpp"
#include "HumidifierController.hpp"
#include "esp_log.h"

static const char* TAG = "BlynkManager";

BlynkManager::BlynkManager(const std::string& authToken, const std::string& baseURL, DHTSensor* dhtSensor, HumidifierController* humidifierController)
    : authToken(authToken), baseURL(baseURL), dhtSensor(dhtSensor), humidifierController(humidifierController), autoMode(true), manualControl(false)
{
    begin();
}

void BlynkManager::begin(){
    //Initial read to sync control states
    fetchControlMode();
    if(!autoMode){
        fetchManualSwitchState();
    }
}

void BlynkManager::updateSensorReadings(float temp, float hum){
    std::string tempStr = std::to_string(temp);
    std::string humStr = std::to_string(hum);
    sendToBlynk(0, tempStr); // Virtual pin V0 for temperature
    sendToBlynk(1, humStr); // virtual pin V1 for humidity
    ESP_LOGI(TAG, "Updated Temp: %.2f, Hum: %.2f to Blynk", temp, hum);
}

void BlynkManager::fetchControlMode(){
    std::string response = fetchFromBlynk(3);  //V3 for Mode
    int mode = std::stoi(response);

    autoMode = (mode == 0); // 0 = auto, 1 = manual
    ESP_LOGI(TAG, "Control mode: %s", autoMode ? "Auto" : "Manual");

    if(!autoMode){
        fetchManualSwitchState();
    }
}

void BlynkManager::fetchManualSwitchState(){
    std::string response = fetchFromBlynk(2); //V2 for manual ON/ OFF
    ESP_LOGI(TAG, "Received switch response: %s", response.c_str());
    int switchState = std::stoi(response);

    if(switchState == 1){
        if(!manualControl){    //Only update if there's change
            manualControl = true;
            humidifierController->turnOn();
            ESP_LOGI(TAG, "Manual Mode: Humidifier ON");
        }
    }
    else{
        if(manualControl){
            ESP_LOGI(TAG, "Manual Mode: Humidifier OFF");
        }
     }
}

bool BlynkManager::isAutoMode() const {
    return autoMode;
}

bool BlynkManager::isManualSwitchOn() const {
    return manualControl;
}

void BlynkManager::sendToBlynk(int virtualPin, const std::string& value){

    std::string url = baseURL;
    if(baseURL.empty() && baseURL.back() != '/'){   //baseURL must end with slash
        url += '/'; 
    }

    url += "external/api/update?token=" + authToken + "&V" + std::to_string(virtualPin) + "=" + value;
    ESP_LOGI(TAG, "Sending to Blynk URL:%s", url.c_str());

    esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;  //5 sec timeout
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(client == nullptr){
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if(err == ESP_OK){
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG, "Successfully sent to V%d: %s", virtualPin, value.c_str());
        } else {
            ESP_LOGW(TAG, "Sent to Blynk V%d returned status code: %d", virtualPin, status_code);
        }
    }
    else{
        ESP_LOGW(TAG, "Failed to send to Blynk V%d: %s", virtualPin, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

std::string BlynkManager::fetchFromBlynk(int virtualPin) {
    std::string url = baseURL;
    if(!baseURL.empty() && baseURL.back() != '/'){
        url += '/';
    }

    url += "external/api/get?token=" + authToken + "&V" + std::to_string(virtualPin);
    ESP_LOGI(TAG, "Fetching from Blynk URL: %s", url.c_str());

    esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000; //5sec timeout

    esp_http_client_handle_t client = esp_http_client_init(&config);
    std::string response = "";

    if(client == nullptr){
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return response;
    }

    esp_err_t err = esp_http_client_perform(client);
    if(err == ESP_OK){
        int status_code = esp_http_client_get_status_code(client);
        if(status_code == 200){
            int content_len = esp_http_client_get_content_length(client);
            if(content_len >= 0){
                int buffer_size = content_len > 0 ? content_len : 64;      //default length 64 if content_len = 0
                if(buffer_size > 1024){
                    buffer_size = 1024; //Limit to prevent buffer overflow
                }

                char* buffer = new char[buffer_size + 1];
                int read_len = esp_http_client_read(client, buffer, buffer_size);
                if(read_len >= 0) {
                    buffer[read_len] = 0;  
                    response = std::string(buffer, read_len);
                    ESP_LOGI(TAG, "Fetched fromV%d: '%s' ", virtualPin, response.c_str());
                }
                else{
                    ESP_LOGW(TAG, "Failed to read response data");
                }
                delete[] buffer;
            }
            else{
                ESP_LOGW(TAG, "Invalid content length: %d", content_len);
            }
        }
        else{
            ESP_LOGW(TAG, "HTTP request failed with status code:%d", status_code);
        }
    }
    else{
        ESP_LOGW(TAG, "Failed to fetch from V%d: %s", virtualPin, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return response;
}

void BlynkManager::start(){
    BaseType_t result = xTaskCreate (
        blynkMonitorTask,
        "blynkMonitorTask",
        4096,
        this,
        1,
        nullptr);
    if(result != pdPASS){
        ESP_LOGE(TAG, "Failed to create blynkMonitorTask");
    }
    else
    {
        ESP_LOGI(TAG, "Successfully created blynkMonitorTask");
    }
}

void BlynkManager::blynkMonitorTask(void* pvParameters){
    BlynkManager* blynkManager = static_cast<BlynkManager*>(pvParameters);
    const TickType_t xDelay = pdMS_TO_TICKS(5000); //checks every 5 sec
    while(true){
        //Check for mode changes
        blynkManager->fetchControlMode();

        //If in Manual mode, check switch state
        if(!blynkManager->isAutoMode()){
            blynkManager->fetchManualSwitchState();
        }
        vTaskDelay(xDelay);
    }
}

