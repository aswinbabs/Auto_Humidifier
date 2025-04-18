// BlynkManager.hpp


#include <string>

class DHTSensor;
class HumidifierController;
class PixelManager;

class BlynkManager {
public:
    BlynkManager(const std::string& authToken, const std::string& baseURL, DHTSensor* dhtSensor, HumidifierController* humidifierController, PixelManager* pixelManager);
    void start();

    bool isAutoMode() const;
    bool isManualSwitchOn() const;
    void setHumidifierController(HumidifierController* controller);
    void setPixelManager(PixelManager* manager);
    void fetchControlMode();
    void fetchManualSwitchState();
    void fetchHumidityThreshold();
    void fetchPixelMode();
    
private:
    std::string authToken;
    std::string baseURL;
    DHTSensor* dhtSensor;
    HumidifierController* humidifierController;
    PixelManager* pixelManager;
    bool autoMode;
    bool manualSwitchOn;

    static void blynkMonitorTask(void* pvParameters);
    void updateSensorReadings();
    std::string fetchFromBlynk(int virtualPin);
    void sendToBlynk(int virtualPin, const std::string& value);
};
