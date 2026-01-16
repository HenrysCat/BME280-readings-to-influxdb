#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "BME680"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "BME680"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define WIFI_SSID "ssid" // Your network name
#define WIFI_PASSWORD "password" // Your network password
#define INFLUXDB_URL "http://ip-address:8086" // ip address os esp8266
#define INFLUXDB_TOKEN "paste token here" // influxdb token
#define INFLUXDB_ORG "esp8266"
#define INFLUXDB_BUCKET "TVOC"

#define TZ_INFO "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensorReadings("measurements");

// BME680
Adafruit_BME680 bme; // I2C

float temperature;
float humidity;
float pressure;
float gasResistance;

// Initialize BME680
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms
}

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  
  // Init BME680 sensor
  initBME();
  
  // Add tags
  sensorReadings.addTag("device", DEVICE);
  sensorReadings.addTag("SSID", WiFi.SSID());

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  // Trigger a reading
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading!");
    return;
  }

  // Store measured values into points
  sensorReadings.clearFields();
  
  temperature = bme.temperature;
  humidity = bme.humidity;
  pressure = bme.pressure / 100.0F; // Convert Pa to hPa
  gasResistance = bme.gas_resistance / 1000.0F; // Convert Ohms to kOhms

  // Add readings as fields to point
  sensorReadings.addField("Temperature", temperature);
  sensorReadings.addField("Humidity", humidity);
  sensorReadings.addField("Pressure", pressure);
  sensorReadings.addField("GasResistance", gasResistance);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensorReadings));
  
  // Write point into buffer
  client.writePoint(sensorReadings);

  // Clear fields for next usage. Tags remain the same.
  sensorReadings.clearFields();

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  // Wait 60s
  Serial.println("Wait 60s");
  delay(60000);
}
