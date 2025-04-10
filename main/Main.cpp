//Main.cpp

#include "HumidifierController.hpp"
#include "DHTSensor.hpp"
#include "WIFIManager.hpp"
#include "BlynkManager.hpp"
#include "pinDefinitions.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Blynk configuration
static const std::string AUTH_TOKEN = "--------";
static const std::string BASE_URL = "http://blynk.cloud";

extern "C" void app_main(void)
{
    static WIFIManager wifi("ASWINBABS", "00000000");
    wifi.connect();
    if(wifi.isConnectedToWifi()) {
        ESP_LOGI("Main", "Wifi is connected!");
    }
    else{
        ESP_LOGI("Main", "Wifi is not connected!");
        return;
    }

    static DHTSensor dht(DHT_SENSOR);
    dht.start();

    static HumidifierController humidifier(&dht, HUMIDIFIER_SENSOR);

    // Create the Blynk manager before starting the humidifier controller
    static BlynkManager blynk(AUTH_TOKEN, BASE_URL, &dht, &humidifier);
    // Start the Blynk monitor task
    blynk.start();
     // Start the humidifier controller after Blynk is initialized
    humidifier.start();


    while (true)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        if (dht.read(temperature, humidity)) {
            blynk.updateSensorReadings(temperature, humidity);

            //Log current state
            ESP_LOGI("Main", "Mode: %s, Temperature:%.2f°C, Humidity: %.2f%%", 
                blynk.isAutoMode() ? "Auto" : "Manual", temperature, humidity);
        } else {
            ESP_LOGW("Main", "Failed to read DHT sensor");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for 5 seconds
    }
}
