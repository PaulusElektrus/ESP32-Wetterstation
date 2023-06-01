// W-Lan & Serverbibliothek
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Sensorbibliotheken
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <DHT.h>

// Definitionen
#define DEVICE "ESP32"
#define SEALEVELPRESSURE_HPA (1013.25)
#define NORMAL_HEIGHT (537.00)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define DHTPIN 4
#define DHTTYPE DHT22

// WLAN Zugangsdaten
#define WIFI_SSID "***"
#define WIFI_PASSWORD "***"

// InfluxDB
#define INFLUXDB_URL "***"
#define INFLUXDB_DB_NAME "***"
#define INFLUXDB_USER "***"
#define INFLUXDB_PASSWORD "***"

// Initialisieren
WiFiMulti wifiMulti;
Adafruit_BMP280 bme;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

// Variablen
float temperature_in, pressure, altitude, lux, humidity, temperature_out, gt, regen, da;
Point sensor("wetterdaten");
Point energy("shelly");

// Timing
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long updateInterval = 60000;


void setup() {
  // serieller Monitor
  Serial.begin(115200);

  // W-Lan Verbindung
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  wifiMulti.run();
  Serial.print("IP = ");  Serial.println(WiFi.localIP());

  // Sensoren starten
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  bme.begin(0x76);   
  lightMeter.begin();
  dht.begin();

  // Check InfluxDB connection
  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USER, INFLUXDB_PASSWORD);
  client.validateConnection();
}


void loop() {
  currentMillis = millis();
    if (currentMillis - startMillis >= updateInterval) {
      getWeatherData();
      startMillis = currentMillis;
    }
  getShellyData();
  Serial.println("Loop");
}


void getShellyData() {
  energy.clearFields();

  HTTPClient http;
  http.begin("http://192.168.0.121/rpc/EM.GetStatus?id=0");
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    JsonObject root = doc.as<JsonObject>();
    for (JsonPair kv : root) {
      energy.addField(kv.key().c_str(), kv.value().as<float>());
    }
    writeInfluxDB(true);
  }
  http.end();
}


void getWeatherData() {
  sensor.clearFields();

  temperature_in = bme.readTemperature();
  pressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  da = NORMAL_HEIGHT-altitude;
  lux = lightMeter.readLightLevel();
  humidity = dht.readHumidity();
  temperature_out = dht.readTemperature();
  gt = dht.computeHeatIndex(temperature_out, humidity, false);
  regen = analogRead(34);
  regen = regen/4095;
  regen = (1/regen)-1.6;

  sensor.addField("T_in", temperature_in);
  sensor.addField("Pressure", pressure);
  sensor.addField("da", da);
  sensor.addField("LUX", lux);
  sensor.addField("Humidity", humidity);
  sensor.addField("T_out", temperature_out);
  sensor.addField("gt", gt);
  sensor.addField("rain", regen);

  writeInfluxDB(false);
}


void writeInfluxDB(bool which) {
  if (which == true) {client.pointToLineProtocol(energy); client.writePoint(energy);}
  else {client.pointToLineProtocol(sensor); client.writePoint(sensor);}
  delay(500);
}
