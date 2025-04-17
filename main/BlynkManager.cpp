// BlynkManager.cpp
#include "BlynkManager.hpp"
#include "DHTSensor.hpp"
#include "HumidifierController.hpp"
#include "esp_log.h"
#include "esp_http_client.h"
#include <cstdlib>

static const char* TAG = "BlynkManager";

BlynkManager::BlynkManager(const std::string& authToken, const std::string& baseURL, DHTSensor* dhtSensor, HumidifierController* humidifierController)
    : authToken(authToken), baseURL(baseURL), dhtSensor(dhtSensor), humidifierController(humidifierController), autoMode(true), manualSwitchOn(false) {}

void BlynkManager::start() {
    BaseType_t result = xTaskCreate(
        blynkMonitorTask,
        "blynkMonitorTask",
        4096,
        this,
        1,
        nullptr
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create blynkMonitorTask");
    } else {
        ESP_LOGI(TAG, "Successfully created blynkMonitorTask");
    }
}

void BlynkManager::blynkMonitorTask(void* pvParameters) {
    BlynkManager* blynkManager = static_cast<BlynkManager*>(pvParameters);
    const TickType_t xDelay = pdMS_TO_TICKS(3000);

    vTaskDelay(pdMS_TO_TICKS(3000)); // Initial delay

    while (true) {
        blynkManager->updateSensorReadings();
        blynkManager->fetchControlMode();
        blynkManager->fetchHumidityThreshold();

        if (!blynkManager->isAutoMode()) {
            blynkManager->fetchManualSwitchState();
        }

        vTaskDelay(xDelay);
    }
}

void BlynkManager::updateSensorReadings() {
    std::string tempStr = std::to_string(dhtSensor->getTemperature());
    std::string humStr = std::to_string(dhtSensor->getHumidity());
    sendToBlynk(0, tempStr);
    sendToBlynk(1, humStr);
    ESP_LOGI(TAG, "Updated Temp: %s, Hum: %s to Blynk", tempStr.c_str(), humStr.c_str());
}

void BlynkManager::fetchControlMode() {
    std::string response = fetchFromBlynk(3);  // V3: mode (0 = Auto, 1 = Manual)
    ESP_LOGI(TAG, "Control mode response: '%s'", response.c_str());
    
    if (response.empty()) {
        ESP_LOGE(TAG, "Empty control mode response");
        return;
    }

    bool previousMode = autoMode;
    autoMode = (response == "0");

    if (previousMode != autoMode) {
        ESP_LOGI(TAG, "Control mode changed to: %s", autoMode ? "Auto" : "Manual");
    }

    if (!autoMode) {
        fetchManualSwitchState();
    }
}

void BlynkManager::fetchManualSwitchState() {
    std::string response = fetchFromBlynk(2);  // V2: switch (0 = OFF, 1 = ON)
    ESP_LOGI(TAG, "Received switch response: '%s'", response.c_str());

    if (response.empty()) {
        ESP_LOGE(TAG, "Empty switch state response");
        return;
    }

    bool previousState = manualSwitchOn;
    manualSwitchOn = (response == "1");

    if (previousState != manualSwitchOn) {
        ESP_LOGI(TAG, "Manual switch changed to: %s", manualSwitchOn ? "ON" : "OFF");

        if (humidifierController) {
            manualSwitchOn ? humidifierController->turnOn() : humidifierController->turnOff();
        } else {
            ESP_LOGW(TAG, "Humidifier controller not set");
        }
    }
}

void BlynkManager::fetchHumidityThreshold() {
    std::string response = fetchFromBlynk(4);  //V4: HumidityThreshold
    ESP_LOGI(TAG, "Humidity Threshold response: '%s'", response.c_str());

    if(response.empty()){
        ESP_LOGE(TAG, "Empty humidity threshold response");
        return;
    }

    float humThreshold = std::stof(response);
    if(humThreshold >= 0.0f && humThreshold <= 100.0f){
        if(humidifierController){
            humidifierController->setHumidityThreshold(humThreshold);
            ESP_LOGI(TAG, "Humidity threshold updated to: %.2f%%", humThreshold);
        }
        else{
            ESP_LOGW(TAG, "Humidifier controller not set");
        }
    }
    else{
        ESP_LOGW(TAG, "Invalid humidity threshold value: %.2f, ignoring", humThreshold);
    }
}

std::string BlynkManager::fetchFromBlynk(int virtualPin) {
    std::string url = "http://blynk.cloud/external/api/get?token=" + authToken + "&V" + std::to_string(virtualPin);
    ESP_LOGI(TAG, "Fetching from Blynk URL: %s", url.c_str());

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 5000;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    std::string response;

    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return response;
    }
    
    // Open connection and send request
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return response;
    }
    
    // Get response content length
    int content_length = esp_http_client_fetch_headers(client); 
    if (content_length > 0) {
        // Allocate buffer
        char* buffer = new char[content_length + 1];
        // Read data in chunks
        int total_read = 0;
        int remaining = content_length;
        while (remaining > 0) {
            int read_len = esp_http_client_read(client, buffer + total_read, remaining);
            if (read_len <= 0) {
                break;  // Error or end of data
            }
            total_read += read_len;
            remaining -= read_len;
        }
        if (total_read > 0) {
            buffer[total_read] = '\0';
            response = std::string(buffer, total_read);
            
            // Clean any whitespace or quotes
            if (!response.empty()) {
                if (response.front() == '"' && response.back() == '"') {
                    response = response.substr(1, response.length() - 2);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to read data: %d/%d bytes read", total_read, content_length);
        }
        delete[] buffer;
    } else {
        ESP_LOGW(TAG, "Invalid content length: %d", content_length);
    }
    
    // Close connection
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    return response;
}

void BlynkManager::sendToBlynk(int virtualPin, const std::string& value) {
    std::string url = baseURL;
    if (!url.empty() && url.back() != '/') {
        url += '/';
    }

    url += "external/api/update?token=" + authToken + "&v" + std::to_string(virtualPin) + "=" + value;
    ESP_LOGI(TAG, "Sending to Blynk URL: %s", url.c_str());

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 5000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Sent to V%d: %s (HTTP %d)", virtualPin, value.c_str(), status_code);
    } else {
        ESP_LOGW(TAG, "Failed to send to V%d: %s", virtualPin, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

bool BlynkManager::isAutoMode() const {
    return autoMode;
}

bool BlynkManager::isManualSwitchOn() const {
    return manualSwitchOn;
}

void BlynkManager::setHumidifierController(HumidifierController* controller) {
    this->humidifierController = controller;
}

