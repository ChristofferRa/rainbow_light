// ESP32-C3 Rainbow Lamp Main Control Code
// Configured with WiFiManager captive portal (5-minute timeout), ArduinoOTA, Hardware Watchdog,
// and Home Assistant integration (discrete entities). Removed uptime sensor.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ArduinoHA.h>
#include <esp_task_wdt.h>
#include <Adafruit_NeoPixel.h>
#include "SettingsManager.h"

// Watchdog timeout in seconds
#define WDT_TIMEOUT 10

//***************************
//***        WiFi         ***
//***************************
WiFiClient espClient;

//***************************
//***   Home Assistant    ***
//***************************
HADevice device;
// Using single client class for device-wide subscription list
HAMqtt mqtt(espClient, device);

HASwitch powerSwitch("rainbow_power");
HANumber brightnessNumber("rainbow_brightness");
HASwitch autoBrightnessSwitch("rainbow_auto_brightness");
HASensorNumber lightSensor("rainbow_light_sensor", HASensorNumber::PrecisionP1);
HASensorNumber rssiSensor("rainbow_rssi", HASensorNumber::PrecisionP0);
HASelect animationSelect("rainbow_animation");

//***************************
//***      Sensors        ***
//***************************
const int light_sensor_pin = 0; // photoresistor sensor pin

//***************************
//***       LED-Strip     ***
//***************************
#define WS2812B_PIN 4
const int Num_Rings = 5;
const int Pixels_In_Ring[] = {37, 33, 28, 25, 24};
const int Num_Pixels = 37 + 33 + 28 + 25 + 24;
Adafruit_NeoPixel WS2812B(Num_Pixels, WS2812B_PIN, NEO_GRB + NEO_KHZ800);

struct rgb_colors {
  int r, g, b;
};
rgb_colors ring_color[Num_Rings];

// State
bool light_on = true;
int brightness_level = 15;
bool auto_brightness = true;
const int max_brightness_allowed = 50;
const int min_brightness_allowed = 1;

enum AnimationMode {
  ANIM_NORMAL = 0,
  ANIM_RAINBOW_CHASE,
  ANIM_PULSE,
  ANIM_SPARKLE
};
AnimationMode currentAnimation = ANIM_NORMAL;
unsigned long animationStartTime = 0;
const unsigned long ANIMATION_TIMEOUT = 5 * 60 * 1000; // 5 minutes

// Forward declarations
void light_rainbow();
void light_off();
void update_leds();
void run_animation();

// HA Callbacks
void onPowerCommand(bool state, HASwitch *sender) {
  light_on = state;
  sender->setState(state); // Report back
  if (!light_on) {
    light_off();
  } else {
    update_leds();
  }
}

void onBrightnessCommand(HANumeric number, HANumber *sender) {
  if (!auto_brightness) {
    brightness_level = number.toInt8();
    sender->setState(number); // Report back
    if (light_on)
      update_leds();
  }
}

void onAutoBrightnessCommand(bool state, HASwitch *sender) {
  auto_brightness = state;
  sender->setState(state);
  if (!auto_brightness) {
    // Report current brightness to the slider
    brightnessNumber.setCurrentState((int32_t)brightness_level);
  }
}

void onAnimationCommand(int8_t index, HASelect *sender) {
  currentAnimation = (AnimationMode)index;
  sender->setState(index);
  animationStartTime = millis();
  if (light_on) {
    if (currentAnimation == ANIM_NORMAL) {
      light_rainbow();
    }
  }
}

void setup_ha() {
  byte mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.setName("Rainbow Light");
  device.setSoftwareVersion("2.0.0");
  device.setManufacturer("Rapp Industries");
  device.setModel("Rainbow 1");

  powerSwitch.setName("Power");
  powerSwitch.setIcon("mdi:power");
  powerSwitch.onCommand(onPowerCommand);

  brightnessNumber.setName("Brightness");
  brightnessNumber.setIcon("mdi:brightness-6");
  brightnessNumber.setMin(min_brightness_allowed);
  brightnessNumber.setMax(max_brightness_allowed);
  brightnessNumber.onCommand(onBrightnessCommand);

  autoBrightnessSwitch.setName("Auto Brightness");
  autoBrightnessSwitch.setIcon("mdi:brightness-auto");
  autoBrightnessSwitch.onCommand(onAutoBrightnessCommand);

  lightSensor.setName("Light Level");
  lightSensor.setUnitOfMeasurement("%");
  // Removed setDeviceClass("illuminance") because % unit of measurement is not
  // supported by the illuminance device class in HA.

  rssiSensor.setName("Signal Strength");
  rssiSensor.setUnitOfMeasurement("dBm");
  rssiSensor.setDeviceClass("signal_strength");

  animationSelect.setName("Animation");
  animationSelect.setOptions("Normal;Rainbow Chase;Pulse;Sparkle");
  animationSelect.onCommand(onAnimationCommand);

  mqtt.setBufferSize(1024);
  mqtt.begin(Settings.mqttServer.c_str(), Settings.mqttUser.c_str(),
             Settings.mqttPass.c_str());
}

void get_light_conditions() {
  int max_light_conditions = 4095;
  int min_light_conditions = 0;
  double k = ((double)(max_brightness_allowed - min_brightness_allowed)) /
             (max_light_conditions - min_light_conditions);

  double adc_val = 0;
  int nrSamples = 5;
  for (int i = 0; i < nrSamples; i++) {
    adc_val += analogRead(light_sensor_pin);
    delay(50);
  }
  adc_val /= nrSamples;

  if (auto_brightness) {
    brightness_level =
        k * (adc_val - min_light_conditions) + min_brightness_allowed;
    brightnessNumber.setCurrentState((int32_t)brightness_level);
    if (light_on && currentAnimation == ANIM_NORMAL) {
      WS2812B.setBrightness(brightness_level);
      WS2812B.show();
    }
  }

  // Publish sensor % (0-100)
  float percent = (adc_val / 4095.0) * 100.0;
  lightSensor.setValue(percent);
}

void light_rainbow() {
  int pixel_to_light;
  int pixels_in_previous_rings;
  for (int j = 0; j < Num_Rings; j++) {
    pixels_in_previous_rings = 0;
    for (int k = 0; k < j; k++) {
      pixels_in_previous_rings += Pixels_In_Ring[k];
    }
    for (int p = 0; p < Pixels_In_Ring[j]; p++) {
      pixel_to_light = p + pixels_in_previous_rings;
      WS2812B.setPixelColor(
          pixel_to_light,
          WS2812B.Color(ring_color[j].r, ring_color[j].g, ring_color[j].b));
    }
  }
  WS2812B.setBrightness(brightness_level);
  WS2812B.show();
}

void light_off() {
  WS2812B.clear();
  WS2812B.show();
}

void update_leds() {
  if (currentAnimation == ANIM_NORMAL) {
    light_rainbow();
  }
}

// Simple animations
void anim_rainbow_chase() {
  static uint16_t j = 0;
  for (int i = 0; i < WS2812B.numPixels(); i++) {
    esp_task_wdt_reset(); // Feed WDT
    WS2812B.setPixelColor(i, WS2812B.gamma32(WS2812B.ColorHSV(
                                 (i * 65536L / WS2812B.numPixels()) + j)));
  }
  WS2812B.setBrightness(brightness_level);
  WS2812B.show();
  j += 256;
}

void anim_pulse() {
  static int p = 0;
  static int dir = 1;

  // Draw base rainbow
  light_rainbow();

  // Adjust global brightness based on pulse
  float factor = (float)p / 100.0;
  int current_bright = brightness_level * factor;
  WS2812B.setBrightness(max(1, current_bright));
  WS2812B.show();

  p += dir * 2;
  if (p >= 100)
    dir = -1;
  if (p <= 10)
    dir = 1;
}

void anim_sparkle() {
  light_rainbow();
  int randomPixel = random(WS2812B.numPixels());
  WS2812B.setPixelColor(randomPixel, WS2812B.Color(255, 255, 255));
  WS2812B.show();
  delay(50);
}

void run_animation() {
  if (currentAnimation == ANIM_NORMAL)
    return;

  if (millis() - animationStartTime > ANIMATION_TIMEOUT) {
    currentAnimation = ANIM_NORMAL;
    animationSelect.setState(ANIM_NORMAL);
    light_rainbow();
    return;
  }

  switch (currentAnimation) {
  case ANIM_RAINBOW_CHASE:
    anim_rainbow_chase();
    delay(10);
    break;
  case ANIM_PULSE:
    anim_pulse();
    delay(20);
    break;
  case ANIM_SPARKLE:
    anim_sparkle();
    break;
  default:
    break;
  }
}

void setup() {
  Serial.begin(115200);
  Settings.init();

  ring_color[0] = {255, 0, 0};   // ring 1, red
  ring_color[1] = {230, 115, 0}; // ring 2, orange
  ring_color[2] = {0, 255, 0};   // ring 3, green
  ring_color[3] = {0, 0, 255};   // ring 4, blue
  ring_color[4] = {179, 0, 179}; // ring 5, violet

  WS2812B.begin();
  light_off();

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Workaround for getting wifi working
                                      // stably on ESP32-C3

  WiFiManager wm;
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server",
                                          Settings.mqttServer.c_str(), 40);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username",
                                        Settings.mqttUser.c_str(), 40);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password",
                                        Settings.mqttPass.c_str(), 40,
                                        "type=\"password\"");

  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

  // Boot animation
  WS2812B.setPixelColor(0, WS2812B.Color(255, 115, 0));
  WS2812B.setBrightness(15);
  WS2812B.show();

  // Force config portal if MQTT is not configured
  if (Settings.mqttServer == "") {
    wm.resetSettings();
  }

  wm.setConfigPortalTimeout(300); // 5 minute timeout for the portal

  if (!wm.autoConnect("RainbowLamp_Setup")) {
    Serial.println(
        "Failed to connect and hit timeout. Continuing without WiFi.");
  } else {
    Serial.println("Connected to WiFi!");
    // Only save credentials if we actually connected via the portal or
    // successfully booted
    Settings.saveMqttCredentials(custom_mqtt_server.getValue(),
                                 custom_mqtt_user.getValue(),
                                 custom_mqtt_pass.getValue());
  }

  // Hostname
  String hostname = "RainbowLamp-" + WiFi.macAddress();
  hostname.replace(":", "");
  WiFi.setHostname(hostname.c_str());

  // OTA Setup
  ArduinoOTA.setHostname("rainbow_lamp");
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
  ArduinoOTA.onStart([]() {
    // Remove loopTask from the watchdog during OTA to prevent reboots during
    // flashing
    esp_task_wdt_delete(NULL);
  });
  ArduinoOTA.begin();

  setup_ha();

  // Get initial light level and adjust brightness immediately
  get_light_conditions();

  // Init state
  powerSwitch.setCurrentState(true);
  autoBrightnessSwitch.setCurrentState(true);
  brightnessNumber.setCurrentState((int32_t)brightness_level);
  animationSelect.setCurrentState(ANIM_NORMAL);

  light_rainbow();

  // Initialize Watchdog after WiFi is connected to avoid WDT triggers during
  // captive portal
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  esp_task_wdt_reset(); // Feed WDT
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    mqtt.loop();
  }

  static unsigned long lastLightUpdate = 0;
  if (lastLightUpdate == 0 || millis() - lastLightUpdate > 60000) {
    lastLightUpdate = millis();
    get_light_conditions();
  }

  static unsigned long lastDiagUpdate = 0;
  if (lastDiagUpdate == 0 || millis() - lastDiagUpdate > 60000) {
    lastDiagUpdate = millis();
    if (WiFi.status() == WL_CONNECTED) {
      rssiSensor.setValue(WiFi.RSSI());
    }
  }

  if (light_on) {
    run_animation();
  }
}
