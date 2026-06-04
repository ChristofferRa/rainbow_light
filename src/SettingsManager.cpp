#include "SettingsManager.h"

void SettingsManager::init() {
  prefs.begin("config", false);

  mqttServer = prefs.getString("mqtt_server", "");
  mqttUser = prefs.getString("mqtt_user", "");
  mqttPass = prefs.getString("mqtt_pass", "");
}

void SettingsManager::resetAll() {
  prefs.begin("config", false);
  prefs.clear();
  prefs.end();
}

void SettingsManager::saveMqttCredentials(const String &server,
                                          const String &user,
                                          const String &pass) {
  if (server != mqttServer) {
    mqttServer = server;
    prefs.putString("mqtt_server", mqttServer);
  }
  if (user != mqttUser) {
    mqttUser = user;
    prefs.putString("mqtt_user", mqttUser);
  }
  if (pass != mqttPass) {
    mqttPass = pass;
    prefs.putString("mqtt_pass", mqttPass);
  }
}
