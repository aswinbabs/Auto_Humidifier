//WIFIManager.cpp

#include "WIFIManager.hpp"
#include "esp_log.h"

static const char* TAG = "WIFIManager";

//Static member initialization
EventGroupHandle_t WIFIManager::wifi_event_group = nullptr;
int WIFIManager::s_retry_num = 0;

WIFIManager::WIFIManager(const std::string& ssid, const std::string& password)
: ssid(ssid), password(password)
 {
    //Initialize wifi config
    std::memset(&wifiConfig, 0, sizeof(wifiConfig));
    std::strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), ssid.c_str(), sizeof(wifiConfig.sta.ssid));
    std::strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), password.c_str(), sizeof(wifiConfig.sta.password));
 
    wifi_event_group = xEventGroupCreate();
 }

bool WIFIManager::isConnectedToWifi() const {
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    return bits & WIFI_CONNECTED_BIT;
}

void WIFIManager::eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if(s_retry_num < WIFI_MAX_RETRY){
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        }
        else{
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connect to the AP fail");
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void WIFIManager::wifi_init(){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WIFIManager::eventHandler,
                                                        nullptr,
                                                        nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WIFIManager::eventHandler,
                                                        nullptr,
                                                        nullptr));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "wifi_init finished.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, 
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);

    if(bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID:%s",
                ssid.c_str());
    }
    else if(bits & WIFI_FAIL_BIT){
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                ssid.c_str());
    }
    else{
        ESP_LOGE(TAG, "Unexpected Event");
    }
}

void WIFIManager::connect(){
    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init();
}
