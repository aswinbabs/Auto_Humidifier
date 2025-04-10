//BlynkManager.hpp

#pragma once

#include "esp_wifi.h"
#include "esp_http_client.h"
#include <cstring>
#include <string>

class DHTSensor;
class HumidifierController;

class BlynkManager {
public:
    //Constructor
    BlynkManager(const std::string& authToken, const std::string& baseURL, DHTSensor* dhtSensor, HumidifierController* humidifierController);

    void begin(); // Initalize state form blynk server
    void updateSensorReadings(float temp, float hum);  // set temperature & humidity levels
    void fetchControlMode();            //Reads mode (Auto / Manual)
    void fetchManualSwitchState();      //Read switch state when manual
    
    bool isAutoMode() const;     
    bool isManualSwitchOn() const;

    void start(); // Start the blynk monitor task

private:
    void sendToBlynk(int virtualPin, const std::string& value);
    std::string fetchFromBlynk(int virtualPin);
    static void blynkMonitorTask(void* pvParameters);

    std::string authToken;
    std::string baseURL;
    
    DHTSensor* dhtSensor;
    HumidifierController* humidifierController;

    bool autoMode;
    bool manualControl;

};