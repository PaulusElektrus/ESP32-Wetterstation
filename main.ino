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

// Sensorbibliotheken & Server laden
WiFiMulti wifiMulti;
Adafruit_BMP280 bme;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

// Variablen
float temperature_in, pressure, altitude, lux, humidity, temperature_out, gt, regen, da;
Point sensor("wetterdaten");

 
//////////////////////////// SETUP

void setup() 
{
  // serieller Monitor
  Serial.begin(9600);
  Serial.println("Booting...");

  Serial.println("Verbinden mit ");
  Serial.println(WIFI_SSID);

  // W-Lan Verbindung
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  // W-Lan prüfen 
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi verbunden..!");
  Serial.print("IP= ");  Serial.println(WiFi.localIP());

  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USER, INFLUXDB_PASSWORD);

  // Sensoren starten
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  bme.begin(0x76);   
  lightMeter.begin();
  dht.begin();

  // Check InfluxDB connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  Serial.println("Start des Hauptprogramms");
}


//////////////////////////// HAUPTPROGRAMM

void loop() {
  sensor.clearFields();

  HTTPClient http; //Instanz von HTTPClient starten
  http.begin("http://***ShellyIPAdress***/rpc/EM.GetStatus?id=0"); //Abfrage-URL
  int httpCode = http.GET(); //Antwort des Servers abrufen
  if (httpCode == 200) {
    String payload = http.getString(); //Daten in eine Variable speichern
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    JsonObject root = doc.as<JsonObject>();
    for (JsonPair kv : root) {
      sensor.addField(kv.key().c_str(), kv.value().as<float>());
    }
  }
  
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
  Serial.println("Sensoren ausgelesen");

  sensor.addField("T_in", temperature_in);
  sensor.addField("Pressure", pressure);
  sensor.addField("da", da);
  sensor.addField("LUX", lux);
  sensor.addField("Humidity", humidity);
  sensor.addField("T_out", temperature_out);
  sensor.addField("gt", gt);
  sensor.addField("rain", regen);
  
  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensor));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  
  //Wait 10s
  Serial.println("Wait 10s");
  delay(10000);
}