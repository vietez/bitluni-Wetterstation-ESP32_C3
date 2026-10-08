

#include <Arduino.h> // Include Arduino library
#include <Wire.h>    // Include the I2C library
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT20.h"
#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN 8 // LED on ESP32C3-SuperMini-V2
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 1 // Popular NeoPixel ring size

/**************************************************************************************************
** Declare all program constants                                                                 **
**************************************************************************************************/

const uint32_t SERIAL_SPEED{115200}; ///< Set the baud rate for Serial I/O
#define PUBLISH_VOLTAGE              // now check esp VCC
#define MSG_BUFFER_SIZE 64
#define BATT_VOLT 0
#define SDA 5
#define SCL 6
// milliseconds to sleep
const unsigned long intervall = 300000;
// time out loop count
const int timeout = 200;
unsigned long now = millis();

// Update these with values suitable for your network.
DHT20 DHT;
// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
uint32_t start, stop;
static char ssid[] = "RT-Labor-1";    // network SSID (name)
static char password[] = "hardies42"; // network password
static char mqtt_server[] = "mqtt.42volt.de";
// static char ssid[] = "hw1_gast"; // network SSID (name)
// static char password[] = "KeineAhnung";
// static char mqtt_server[] = "dz-pi.hw1.fb4.fh";

// static char ssid[] = "NETPASS";         //network SSID (name)
// static char password[] = "SP-tollberg3007MvB18";    //network password
//  static char ssid[] = "FRITZ!Box 7490";
//  static char password[] = "95642445727877808620";    //network password
//   static char ssid[] = "GastbeiZumkehr";
//  static char password[] = "KeineAhnung";    //network password
//  static char mqtt_server[] = "192.168.178.28";
const char *topicAlive = "RT-Labor-1/Alive";
// const char *topicHumidity = "wulfen/sdz/ownWeather/humidity";
const char *topicHumidity = "/rt-labor/ownWeather/humidity";
const char *topicTemperature = "wulfen/sdz/ownWeather/temperatur";
const char *topicPress = "wulfen/sdz/ownWeather/pressure";
const char *topicGas = "wulfen/sdz/ownweather/gas";
const char *topicVolt = "wulfen/sdz/ownWeather/Voltage";
const char *topicHeight = "wulfen/sdz/ownWeather/height";
const char *topicWakeuptime = "wulfen/sdz/ownWeather/wake";
/**************************************************************************************************
** Declare global variables and instantiate classes                                              **
**************************************************************************************************/

char topicBuf[MSG_BUFFER_SIZE];
char payloadBuf[MSG_BUFFER_SIZE];
int alive = 0;
float temperature;
int humidity;

int voltage;
int numberRetrys = 0;
int charge;
int standby;

unsigned long wakeUpTime = 0;
char msg[MSG_BUFFER_SIZE];
float calibrateVcc = 1.1090;
int value = 0;

unsigned long startMeasurement = millis();

WiFiClient espClient;
PubSubClient client(espClient);

void deepSleep(int intervall)
{
  WiFi.disconnect();                                  // Trenne alle Verbindungen
  WiFi.mode(WIFI_OFF);                                // Deaktiviere WiFi
  delay(10);                                          // Warte kurz, um Stabilität zu gewährleisten
  esp_sleep_enable_timer_wakeup(intervall * 1000000); // Setze den Timer für Deep Sleep
  esp_deep_sleep_start();                             // Starte den Deep Sleep
}

void setup_wifi()
{
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    numberRetrys++;
    if (numberRetrys > 20)
    {
      deepSleep(10); // sleep 10 seconds
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  snprintf(topicBuf, strlen(topic), "%s", topic);
  Serial.print("Message received: ");
  Serial.println(topic);
  Serial.print(": ");
  for (uint i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    if (i < MSG_BUFFER_SIZE)
    {
      payloadBuf[i] = (char)payload[i];
    }
  }
  if ((String)topic == ((String)topicWakeuptime))
  {
    wakeUpTime = atol(payloadBuf);
    Serial.print("Awake: ");
    Serial.println(wakeUpTime);
  }
  if ((String)topic == ((String)topicHumidity))
  {
    humidity = atoi(payloadBuf);
    // Serial.print("Humi ");
    // Serial.println(humidity);
  }
  if ((String)topic == ((String)topicTemperature))
  {
    temperature = atoi(payloadBuf);
    // Serial.print("Temp ");
    // Serial.println(temperature);
  }

  if ((String)topic == ((String)topicVolt))
  {
    voltage = atoi(payloadBuf);
    // Serial.print("Volt  ");
    // Serial.println(voltage);
  }
  if ((String)topic == ((String)topicAlive))
  {
    // client.publish(topicAlive, "1", true);
    Serial.print("Alive!!");
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32C3Client-";
    clientId += String("OWS1");

    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topicAlive, "ALIVE", true);
      Serial.printf("Publish message: %s\t%s\n", topicAlive, "ALIVE!");
      //... and resubscribe
      client.subscribe(topicAlive);
      client.subscribe(topicWakeuptime);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(SERIAL_SPEED); // Start serial port at Baud rate
  delay(5000);
  Serial.println();
  Serial.println(__FILE__);
  Serial.print("DHT20 LIBRARY VERSION: ");
  Serial.println(DHT20_LIB_VERSION);
  Serial.println();

  Serial.println("\nNOTE: datasheet states 400 KHz as maximum.\n");

  Wire.begin(SDA, SCL);
  DHT.begin(); //  ESP32 default pins 21, 22
  delay(2000);
  Serial.println("OK");
  delay(100);
  setup_wifi();
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); // 320�c for 150 milliseconds
    pixels.clear(); // Set all pixel colors to 'off'
} // of method setup()

void loop()
{
  /*!
  @brief    Arduino method for the main program loop
  @details  This is the main program for the Arduino IDE, it is an infinite loop and keeps on
            repeating. The "sprintf()" function is to pretty-print the values, since floating
            point is not supported on the Arduino, split the values into those before and those
            after the decimal point.
  @return   void
  */
  static unsigned int counter = 0; // counter for loop() calls
  static float temp, humidity; // BME readings
  static char buf[16];         // sprintf text buffer
                               // Temporary variable

  if (millis() - DHT.lastRead() >= 5000)
  {
    startMeasurement = millis();
    //  READ DATA

    int status = DHT.read();

    Serial.print("DHT20 \t");
    humidity = DHT.getHumidity();
    temp = DHT.getTemperature();
    //  DISPLAY DATA, sensor has only one decimal.
    Serial.print(humidity, 1);
    Serial.print("\t\t");
    Serial.print(temp, 1);
    Serial.print("\t\t");
    Serial.print(stop - start);
    Serial.print("\t\t");
    switch (status)
    {
    case DHT20_OK:
      Serial.print("OK");
      break;
    case DHT20_ERROR_CHECKSUM:
      Serial.print("Checksum error");
      break;
    case DHT20_ERROR_CONNECT:
      Serial.print("Connect error");
      break;
    case DHT20_MISSING_BYTES:
      Serial.print("Missing bytes");
      break;
    case DHT20_ERROR_BYTES_ALL_ZERO:
      Serial.print("All bytes read zero");
      break;
    case DHT20_ERROR_READ_TIMEOUT:
      Serial.print("Read time out");
      break;
    case DHT20_ERROR_LASTREAD:
      Serial.print("Error read too fast");
      break;
    default:
      Serial.print("Unknown error");
      break;
    }
    Serial.print("\n");

    snprintf(payloadBuf, MSG_BUFFER_SIZE, "%4.1f", temp);
    client.publish(topicTemperature, payloadBuf, true);
    Serial.printf("Publish message: %s\t%s\n", topicTemperature, payloadBuf);
    snprintf(payloadBuf, MSG_BUFFER_SIZE, "%3.1f", humidity);
    client.publish(topicHumidity, payloadBuf, true);
    Serial.printf("Publish message: %s\t%s\n", topicHumidity, payloadBuf);
    voltage = int(analogRead(BATT_VOLT) * calibrateVcc);

    snprintf(payloadBuf, MSG_BUFFER_SIZE, "%d", voltage);
    client.publish(topicVolt, payloadBuf, true);
    Serial.printf("Publish message: %s\t%s\n", topicVolt, payloadBuf);
    // Calculate and process wakeUpTime
    wakeUpTime = millis() - startMeasurement;
    Serial.print(F("Reading completed at "));
    Serial.println(wakeUpTime);
    snprintf(payloadBuf, MSG_BUFFER_SIZE, "%lu", wakeUpTime);
    client.publish(topicWakeuptime, payloadBuf, true);
    pixels.setPixelColor(0, pixels.Color(0, 50, 0));

    pixels.show();   // Send the updated pixel colors to the hardware.
    Serial.printf("Publish message: %s\t%s\n", topicWakeuptime, payloadBuf);
    counter++;
    Serial.print(F("Loop counter: "));
    Serial.println(counter);
    delay(1000);
    // Serial.println("deep sleep");
    // deepSleep(int(wakeUpTime / 1000.0) + intervall / 1000); // sleep time + measurement time
    // delay(10000); // Wait 10s
  } // of ignore first reading

} // of method loop()
