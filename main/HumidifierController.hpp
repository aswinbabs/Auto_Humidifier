//HumidifierController.hpp
#pragma once
#include <pinDefinitions.hpp>
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class DHTSensor;  
class HumidifierController {
public:
    void turnOn(void);
    void turnOff(void);
    bool getState() const;  //Read-only access to state

    explicit HumidifierController(DHTSensor* dhtSensor, gpio_num_t humPin);  //Constructor with humControlPin
    void start();  //Starts the control FreeRTOS task

private:
    void conf_HumidifierGPIO(); 
    DHTSensor* dhtSensor; //pointer to DHTSensor instance
    gpio_num_t humControlPin;  //Stores GPIO pin
    bool humidifierState;  //Flag to store ON/OFF state
    static void HMD_ControlTask(void* pvParameters); //static RTOS task
    float humidityThreshold = 50.0f;
};