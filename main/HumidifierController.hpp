//HumidifierController.hpp
#pragma once
#include <pinDefinitions.hpp>
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class DHTSensor;
class BlynkManager;  

class HumidifierController {
public:
    void turnOn(void);
    void turnOff(void);
    bool getState() const;  //Read-only access to state

    explicit HumidifierController(DHTSensor* dhtSensor, BlynkManager* blynkManager, gpio_num_t humPin);
    void start();  
    void setHumidityThreshold(float threshold);
    float getHumidityThreshold() const;

private:
    void conf_HumidifierGPIO(); 
    DHTSensor* dhtSensor; 
    BlynkManager* blynkManager; 
    gpio_num_t humControlPin;  //Stores GPIO pin
    bool humidifierState;  //Flag to store ON/OFF state
    static void HMD_ControlTask(void* pvParameters); 
    float humidityThreshold = 60.0f;
};
