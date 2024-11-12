#include <WiFi.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// --------------------------------------------------------------------------------------------
//        UPDATE CONFIGURATION TO MATCH YOUR ENVIRONMENT
// --------------------------------------------------------------------------------------------

// Add GPIO pins used to connect devices
#define DHT_PIN 4 // GPIO pin the data line of the DHT sensor is connected to



// Specify DHT11 (Blue) or DHT22 (White) sensor
#define DHTTYPE DHT11

// Temperatures to set LED by (assume temp in C)
#define ALARM_COLD 0.0
#define ALARM_HOT 30.0
#define WARN_COLD 10.0
#define WARN_HOT 25.0


// MQTT connection details
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "d:hwu.mt4007"
#define MQTT_USER "" // no need for authentication, for now
#define MQTT_TOKEN "" // no need for authentication, for now
#define MQTT_TOPIC "d:hwu.mt4007/evt/status/fmt/json"
#define MQTT_TOPIC_INTERVAL "d:hwu.mt4007/evt/interval/fmt/json"
#define MQTT_TOPIC_DISPLAY "d:hwu.mt4007/cmd/display/fmt/json"


// Add WiFi connection information
const char *ssid = "ENTER YOUR WIFI NAME";  // your network SSID (name)
const char *pass = "ENTER YOUR WIFI PASSWORD";  // your network password


// --------------------------------------------------------------------------------------------
//        SHOULD NOT NEED TO CHANGE ANYTHING BELOW THIS LINE
// --------------------------------------------------------------------------------------------
DHT dht(DHT_PIN, DHTTYPE);

// variables to hold data
StaticJsonDocument<100> jsonDoc;
JsonObject payload = jsonDoc.to<JsonObject>();
JsonObject status = payload.createNestedObject("d");
static char msg[50];

float h = 0.0; // humidity
float t = 0.0; // temperature
unsigned char r = 0; // LED RED value
unsigned char g = 0; // LED Green value
unsigned char b = 0; // LED Blue value

uint8_t r_led = 25;
uint8_t g_led = 26;
uint8_t b_led = 27;


int interval = 10;

// MQTT objects
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  
  payload[length] = 0; // ensure valid content is zero terminated so can treat as c-string
  Serial.println((char *)payload);

  StaticJsonDocument<200> doc; // Adjust size as needed

  // Deserialize the JSON data
  DeserializationError error = deserializeJson(doc, payload);

  // Check if deserialization was successful
  if (error) {
    Serial.print("Deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (strcmp(topic, MQTT_TOPIC_INTERVAL) == 0) {
    int new_interval = doc["interval"];

    Serial.print("\nnew interval: ");
    Serial.println(new_interval);
    interval = new_interval;
  }

  if (strcmp(topic, MQTT_TOPIC_DISPLAY) == 0) {
    int r_val = doc["r"];
    int g_val = doc["g"];
    int b_val = doc["b"];

    Serial.print("Updating values to: r: ");
    Serial.print(r_val);
    Serial.print("  g: ");
    Serial.print(g_val);
    Serial.print("  b: ");
    Serial.println(b_val);
    ledcWrite(r_led, 255-r_val);
    ledcWrite(g_led, 255-g_val);
    ledcWrite(b_led, 255-b_val);
  }


}



void setup() {
  // put your setup code here, to run once:
  //Start serial console
  Serial.begin(115200);
  Serial.setTimeout(2000);
  while (!Serial) { }
  Serial.println();
  Serial.println("ESP32 Sensor Application");

  // Start WiFi connection
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected");

  dht.begin();

  ledcAttach(r_led, 12000, 8);  // 12 kHz PWM, 8-bit resolution
  ledcAttach(g_led, 12000, 8);
  ledcAttach(b_led, 12000, 8);

   // Connect to MQTT broker
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial.println("MQTT Connected");
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);
    mqtt.subscribe(MQTT_TOPIC_INTERVAL);

  } else {
    Serial.println("MQTT Failed to connect!");
  }

}

void loop() {

  mqtt.loop();
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println("MQTT Connected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqtt.subscribe(MQTT_TOPIC_INTERVAL);
    } else {
      Serial.println("MQTT Failed to connect!");
      delay(5000);
    }
  }
  // put your main code here, to run repeatedly:
  h = dht.readHumidity();
  t = dht.readTemperature(); // uncomment this line for Celsius
  // t = dht.readTemperature(true); // uncomment this line for Fahrenheit

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Set RGB LED Colour based on temp
    // b = (t < ALARM_COLD) ? 255 : ((t < WARN_COLD) ? 150 : 0);
    // r = (t >= ALARM_HOT) ? 255 : ((t > WARN_HOT) ? 150 : 0);
    // g = (t > ALARM_COLD) ? ((t <= WARN_HOT) ? 255 : ((t < ALARM_HOT) ? 150 : 0)) : 0;

    // ledcWrite(r_led, 255-r);
    // ledcWrite(g_led, 255-g);
    // ledcWrite(b_led, 255-b);


    // Print Message to console in JSON format
    status["temp"] = t;
    status["humidity"] = h;
    serializeJson(jsonDoc, msg, 50);
    Serial.println(msg);

    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }
    // Pause - but keep polling MQTT for incoming messages
  for (int i = 0; i < interval; i++) {
    mqtt.loop();
    delay(1000);
  }
}