//WIFIManager.hpp

#pragma once

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <cstring>
#include <string>

#define WIFI_CONNECTED_BIT BIT0     //Bit to indicate if wifi connected
#define WIFI_FAIL_BIT BIT1          //Bit to indicate if wifi disconnected
#define WIFI_MAX_RETRY 5            //Maximum retry for wifi connection


class WIFIManager {
public:
    WIFIManager(const std::string& ssid, const std::string& password);
    void connect();
    bool isConnectedToWifi() const;
private:
    void wifi_init();

    std::string ssid;
    std::string password;
    wifi_config_t wifiConfig;

    static EventGroupHandle_t wifi_event_group; //Variable to track connection state
    static int s_retry_num;

    static void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};
