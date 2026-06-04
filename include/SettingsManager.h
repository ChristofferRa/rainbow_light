#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class SettingsManager {
public:
    static SettingsManager& getInstance() {
        static SettingsManager instance;
        return instance;
    }

    void init();
    void resetAll();

    // WiFiManager / MQTT Strings
    String mqttServer;
    String mqttUser;
    String mqttPass;

    void saveMqttCredentials(const String& server, const String& user, const String& pass);

private:
    SettingsManager() {}
    Preferences prefs;
};

// Global instance macro
#define Settings SettingsManager::getInstance()

#endif // SETTINGS_MANAGER_H
