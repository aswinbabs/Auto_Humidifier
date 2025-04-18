
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
#include "PixelManager.hpp"
#include "esp_log.h"
#include "Private.hpp"

extern "C" {
    void app_main(void);
}

void app_main(void) {
    // Print system info
    
    //Init DHT
    static DHTSensor dhtSensor(DHT_SENSOR);
    dhtSensor.start();

    // Init WiFi
    static WIFIManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
    wifiManager.connect();
    vTaskDelay(pdMS_TO_TICKS(5000));
     if(wifiManager.isConnectedToWifi()){
        ESP_LOGI("Main", "WIFI:Connected!");
     }
    
    //PixelManager instance
    static PixelManager pixelManager(PIXEL_LED_PIN, NUM_LEDS);
    pixelManager.start();

    static BlynkManager blynkManager(BLYNK_AUTH_TOKEN, BLYNK_SERVER, &dhtSensor, nullptr, &pixelManager);
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

