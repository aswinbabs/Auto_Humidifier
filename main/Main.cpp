
//Main.cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "pinDefinitions.hpp"
#include "DHTSensor.hpp"
#include "HumidifierController.hpp"
#include "WIFIManager.hpp"
#include "BlynkManager.hpp"
#include "esp_log.h"

extern "C" {
    void app_main(void);
}

void app_main(void) {
    // Print system info
    
    //Init DHT
    static DHTSensor dhtSensor(DHT_SENSOR);
    dhtSensor.start();

    // Init WiFi
    static WIFIManager wifiManager("ASWINBABS", "00000000");
    wifiManager.connect();
    vTaskDelay(pdMS_TO_TICKS(5000));
     if(wifiManager.isConnectedToWifi()){
        ESP_LOGI("Main", "WIFI:Connected!");
     }
    
    static BlynkManager blynkManager("", "http://blynk.cloud", &dhtSensor, nullptr);
    
    blynkManager.start();
    blynkManager.fetchControlMode();
    //syncing data
    if(!blynkManager.isAutoMode()){
        blynkManager.fetchManualSwitchState();
    }

    static HumidifierController humidifierController(&dhtSensor, &blynkManager, HUMIDIFIER_SENSOR);
    blynkManager.setHumidifierController(&humidifierController);
    humidifierController.start();
    ESP_LOGI("Main", "Auto-Humidifier System starts working");
}

