#include <Arduino.h>

//***************************
//*** Adaptive Brightness ***
//***************************
#include <Wire.h>
const int light_sensor_pin = 0; // photoresistor sensor pin

//***************************
//***        WiFi         ***
//***************************
#include <WiFi.h>

// Shoud wifi be used
const bool wifi_active = true;

const char* deviceName = "rainbow_lamp";
const char* ssid = "your_ssid";
const char* pwd = "your_password";

// Event Handling
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("\n\nConnected to; " + String(ssid));
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, pwd);
}

//***************************
//***       MQTT          ***
//***************************
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

// Should a mqtt-server be used for remote operation
const bool mqtt_active = true;

// MQTT Broker
// Server and credentials
const char* mqtt_server = "your_mqtt_server_address";
const char* mqtt_user = "your_username";
const char* mqtt_password = "your_password";

// Used topics
const char* light_topic = "lights/rainbow_light";
const char* light_status_topic = "lights/rainbow_light_status";
const char* bright_topic = "sensors/rainbow_light/brightness";

WiFiClient espClient;
PubSubClient client(espClient);

// Function to connect to MQTT-server
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Create client ID
    String clientId = "rainbow_light";
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user,mqtt_password)) {
      Serial.println("MTTQ connected");
      Serial.print("Server: ");
      Serial.println(mqtt_server);
      //subscribe
      client.subscribe(light_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Se callback in Set-up section of the code for MQTT-listener

//***************************
//***    Multitasking     ***
//***************************
unsigned long currentMillis;

// Update Brightness
unsigned long lastMillisBright;
const long intervallBright = 1*60*1000; // How often to update brightness

// Run Animation
unsigned long lastMillisAnim;
const long intervallAnim = 15*60*1000; // How often to update animation

//***************************
//***       LED-Strip     ***
//***        WS2812B      ***
//***************************
// https://github.com/adafruit/Adafruit_NeoPixel/tree/master

#include <Adafruit_NeoPixel.h>
#define WS2812B_PIN    4   // Pin connected to strip

// Number of rings in rainbow
const int Num_Rings = 5;

// Define the number of each ring in the rainbow.
//                     Ring    1   2   3   4   5
const int Pixels_In_Ring[] = {37, 33, 28, 25, 24};

// Sum of all pixels
const int Num_Pixels = 37+ 33+ 28+ 25+ 24;

// Brightness
int brightness_level = 15; // Default Level of brightness of LED:s 1->255

const int max_brightness_allowed = 50; // Maximum allowed brightness to limit current
const int min_brightness_allowed = 1;

// Light state
bool light_on = false;

// Define struct with color information of each ring.
// The colors are set in the setup-function.
struct rgb_colors {
    int r;
    int g;
    int b;
};

rgb_colors ring_color[Num_Rings];

// Init WS2712B object
Adafruit_NeoPixel WS2812B(Num_Pixels, WS2812B_PIN, NEO_GRB + NEO_KHZ800);

void light_rainbow(){
  // Function to light rainbow with an animation
  int pixel_sum = 0;
  int ring = 0;

  WS2812B.clear();
  /*
  for (int element : Pixels_In_Ring){
    
    for (int k = pixel_sum; k<pixel_sum + element; k++){
      
      WS2812B.setPixelColor(k, WS2812B.Color(ring_color[ring].r, ring_color[ring].g, ring_color[ring].b));
      WS2812B.setBrightness(brightness_level);
      WS2812B.show();
      delay(100);
    }
  ring++;
  pixel_sum += element;
  }
  */
 int discrete_steps = 100;
 double step;
 int pixel_to_light;
 int pixels_in_previous_rings;
 for (int i = 0; i<discrete_steps; i++){
  for (int j = 0; j<Num_Rings; j++){

    pixels_in_previous_rings = 0;
    for(int k = 0; k<j;k++){
      pixels_in_previous_rings = pixels_in_previous_rings + Pixels_In_Ring[k];
    }

    step = Pixels_In_Ring[j]/(double)discrete_steps;

    pixel_to_light = floor(step * i);
    
    // Check if even ring number, then invert counting to start from the right direction
    if (j % 2 == 0){
      }
    else {
      pixel_to_light = Pixels_In_Ring[j] - pixel_to_light -1;
    }
    pixel_to_light = pixel_to_light + pixels_in_previous_rings;
    WS2812B.setPixelColor(pixel_to_light, WS2812B.Color(ring_color[j].r, ring_color[j].g, ring_color[j].b));
  }
  WS2812B.setBrightness(brightness_level);
  WS2812B.show();
  delay(50);

  }
  light_on = true;
}

// Function to turn of rainbow light
void light_off(){
  WS2812B.clear();
  WS2812B.show();
  light_on = false;
}


void connection_animation(){
  // Function for Wifi-connection animation
  int pixel_sum = 0;
  int ring = 0;
  
  for (int element : Pixels_In_Ring){

    WS2812B.clear(); // Only light one ring at the same time

    for (int k = pixel_sum; k<pixel_sum + element; k++){
      
      WS2812B.setPixelColor(k, WS2812B.Color(255, 115, 0));
      WS2812B.setBrightness(brightness_level);
      WS2812B.show();
      
    }
    delay(200);

  ring++;
  pixel_sum += element;

  }
}

void light_all_one_color(int r, int g, int b){
  // Function for lighting all rings with one RGB-color
  int pixel_sum = 0;
  int ring = 0;
  
  WS2812B.clear();
  for (int element : Pixels_In_Ring){

    for (int k = pixel_sum; k<pixel_sum + element; k++){
      
      WS2812B.setPixelColor(k, WS2812B.Color(r, g, b));
      WS2812B.setBrightness(brightness_level);
      WS2812B.show();
    
    }

  ring++;
  pixel_sum += element;

  }
  
}

void get_light_conditions(){
  // Function for adaptive brightness, adjusts brightness depending on ambient light conditions
  // Within predefined range

  // 5528 Photoresistor togheter with 10kOhm resistor yields
  // 0 -> Complete darkness
  // 1500 -> Dark room
  // 3700 -> Lit room
  // 4095 -> Flashlight on photoresistor....

  int max_light_conditions = 4095;
  int min_light_conditions = 0;

  // Calculate slope of brightness curve
  double k = ((double)(max_brightness_allowed-min_brightness_allowed))/(max_light_conditions-min_light_conditions);

  // Sample adc 5 times and average to smooth out readings
  double adc_val = 0;
  int nrSamples = 5;
  for (int i=0; i<nrSamples; i++){
    adc_val = adc_val + analogRead(light_sensor_pin); //Read adc value
    delay(200);
  }
  adc_val = adc_val / nrSamples;

  // Calculate new brightness level
  brightness_level = k*(adc_val-min_light_conditions)+min_brightness_allowed;
  Serial.println("New brightness level: " + String(brightness_level) + "/255");

  // If light is on, set new brightness level
  if(light_on){
    WS2812B.setBrightness(brightness_level);
    WS2812B.show();
  }

  // Publish brightnesslevel to MQTT
  if(mqtt_active){
    char tempString[4];
    dtostrf(adc_val, 1, 2, tempString);
    client.publish(bright_topic, tempString);
  }

}

//********************
//***    Setup     ***
//********************

void connect_wifi(){
  // Connect to wifi
  Serial.print("\nTrying to connect to WiFi: ");
  Serial.print(ssid);
  Serial.println("");
  
  // Make sure wifi is disconnected before trying to connect.
  WiFi.disconnect(true);

  // Listen for events
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  
  // Wifi-setup
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Workaround for getting wifi working on ESP32-C3
  WiFi.hostname(deviceName);

  delay(500);
  // Connect
  WiFi.begin(ssid, pwd);
  
  int timeout_wifi = 0;
  while (WiFi.status() != WL_CONNECTED && timeout_wifi < 30) {
    // Wait for connection and show animation
    Serial.print(".");
    connection_animation();
    timeout_wifi++;
  }
  if (WiFi.status() == WL_CONNECTED){
    // If succesfully connected
    light_all_one_color(0, 255, 0); // Light all rings green to show success
    Serial.println("WiFi connected");
    Serial.println("\n\nConnected to: " + String(ssid));
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(1000);
  }else{
    // If not connected after time-out
    light_all_one_color(255, 0, 0);
    Serial.println("\n\nFailed to Connect to " + String(ssid));
    delay(5000);
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  // Function listening for MQTT Messages

  Serial.print("Message arrived on topic: " + String(topic) + ". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == light_topic) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      light_rainbow();
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      light_off();
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  
  /**************
   --- Serial ---
  ***************/
  Serial.begin(115200);
  Serial.println("Rainbow Light!...");

  /********************
   --- WS2812b Init ---
  *********************/
  // Set ring colors
  ring_color[0] = {255,  0,  0}; // ring 1, red
  ring_color[1] = {230, 115, 0}; // ring 2, orange 
  ring_color[2] = {0,   255, 0}; // ring 3, green
  ring_color[3] = {0,   0,   255}; // ring 4, blue 
  ring_color[4] = {179, 0,   179}; // ring 5, violet

  WS2812B.begin(); // Init led-strip

  /*********************
   --- Connect Wifi ---
  *********************/
  // If wifi has been set up, then connect
  if(wifi_active){
    connect_wifi();
  }
  
  /*****************
   --- MQTT Init ---
  ******************/
 // If MQTT has been set up, then connect
  if(mqtt_active){
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
  }

  /*********************
   --- Light Rainbow ---
  **********************/
  light_rainbow(); // Always light up on power-on
}

//*******************
//***    Loop     ***
//*******************
void loop() {

  //Check MQTT-server connection and listen for MQTT-messages
  if(mqtt_active){
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }

  currentMillis = millis();

  // Adjust brightness and report status to MQTT once and a while...
  if(currentMillis - lastMillisBright > intervallBright){
    lastMillisBright = currentMillis;
    //Adjust brightness as needed
    get_light_conditions();

    // Publish status of rainbowlight to MQTT
    if(light_on){
      client.publish(light_status_topic, "on");
    }
    else {
      client.publish(light_status_topic, "off");
    }
    
  }

  // Rerun animation
  if(currentMillis - lastMillisAnim > intervallAnim){
    lastMillisAnim = currentMillis;
    //Run light animation i light is on
    if(light_on){
      light_rainbow();
    }
  }

}
