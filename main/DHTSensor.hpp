//DHTSensor.hpp
#pragma once

#include "driver/gpio.h"
#include "pinDefinitions.hpp"
#include "dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
    #include "dht.h"
}

class DHTSensor {
public: 
    void start();  //starts freeRTOS tasks
    float getTemperature() const;
    float getHumidity() const;

    explicit DHTSensor(gpio_num_t dhtPin);  //Constructor with dhtControlPin
    bool isReadSuccessful() const;

private:
    static void dht_task(void* pvParameters);
    void DhtRead();
    gpio_num_t dhtControlPin;  //Stores GPIO pin
    float temperature;
    float humidity;
    bool readSuccess;
    void conf_DHTGPIO();
};