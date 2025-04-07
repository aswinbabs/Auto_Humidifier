//Main.cpp

#include "HumidifierController.hpp"
#include "DHTSensor.hpp"
#include "pinDefinitions.hpp"


extern "C" void app_main(void)
{
    static DHTSensor dht(DHT_SENSOR);
    dht.start();

    static HumidifierController humidifier(&dht, HUMIDIFIER_SENSOR);
    humidifier.start();
}
