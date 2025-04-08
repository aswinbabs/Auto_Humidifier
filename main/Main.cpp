//Main.cpp

#include "HumidifierController.hpp"
#include "DHTSensor.hpp"
#include "WIFIManager.hpp"
#include "pinDefinitions.hpp"

#include "esp_log.h"

extern "C" void app_main(void)
{
    static WIFIManager wifi("ASWINBABS", "00000000");
    wifi.connect();
    if(wifi.isConnectedToWifi()) {
        ESP_LOGI("Main", "Wifi is connected!");
    }
    else{
        ESP_LOGI("Main", "Wifi is not connected!");
    }

    static DHTSensor dht(DHT_SENSOR);
    dht.start();

    static HumidifierController humidifier(&dht, HUMIDIFIER_SENSOR);
    humidifier.start();
}
