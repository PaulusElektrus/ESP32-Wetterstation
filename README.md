# ESP32 Wetterstation

I created a ESP32 Wetterstation which sends the sensor data to a InfluxDB which is then displayed in Grafana. This repository contains only the ESP32 Code in .ino format. You can upload it with Arduino IDE.

### Used Hardware

- ESP32 Microcontroller
- DHT22 Humidity & Temperature Sensor
- BMP280 Pressure & Temperature Sensor
- BH1750 Light Meter
- Analog Rain Sensor

### Update 05.2023

I added a Shelly 3EM in my local network which should also be sent to InfluxDB.
