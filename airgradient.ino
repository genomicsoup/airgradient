/*
This is the code for the AirGradient DIY Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

For build instructions please visit https://www.airgradient.com/diy/

Compatible with the following sensors:
Plantower PMS5003 (Fine Particle Sensor)
SenseAir S8 (CO2 Sensor)
SHT30/31 (Temperature/Humidity Sensor)

Please install ESP8266 board manager (tested with version 3.0.0)

The codes needs the following libraries installed:
"WifiManager by tzapu, tablatronix" tested with Version 2.0.3-alpha
"ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg" tested with Version 4.1.0

If you have any questions please visit our forum at https://forum.airgradient.com/

Configuration:
Please set in the code below which sensor you are using and if you want to connect it to WiFi.
You can also switch PM2.5 from ug/m3 to US AQI and Celcius to Fahrenheit

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/schools/

Kits with all required components are available at https://www.airgradient.com/diyshop/

MIT License
*/

#include <AirGradient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>

#include "SSD1306Wire.h"
#include "config.h"

AirGradient ag = AirGradient();

SSD1306Wire display(0x3c, SDA, SCL);

/*
// Enable different hardware: CO2 sensor, PM2 sensor, Temp/humidity sensor
enum HardwareOptions {
    hoCO2 = 0x01,
    hoPM = 0x02,
    hoSHT = 0x04,
};

// To enable different sensors, set the proper flags.
unsigned char hardwareOptions = hoCO2 | hoPM | hoSHT;

// If false, these sensors will be disabled
// Particulate matter sensor (PMS5003)
bool hasPM = true;
// SenseAir S8 CO2 sensor
bool hasCO2 = true;
// SHT30 temp/humidity sensor
bool hasSHT = true;

// If true, PM2.5 uses US AQI instead of ug/m3
bool inUSaqi = false;

// If true, use F instead of C for temp
bool inF = false;

// If true, connect to WIFI
bool connectWIFI = true;

// Send data over WIFI to the following API endpoint
String API_ENDPOINT = "http://192.168.0.69:9000/airquality";
*/

/*
 * A struct containing sensor data.
 * fields:
 *  activeHardware, a bit flag containing info about currently active hardware
 *  pm2,            value containing PM2.5 values in PPM
 *  co2,            value containing CO2 values in ug/m3
 *  th,             struct containing temperature and humidity values
 *  boardTime,      result of the millis() function
 */
struct SensorData {
    unsigned char activeHardware;
    int pm2;
    int co2;
    TMP_RH th;
    unsigned long boardTime;
};

// Capacity of the JSON object payload in bytes
const size_t JSON_CAPACITY = 512;

/*
 * Generates a payload (a JSON object) based on the sensor data
 */
StaticJsonDocument<JSON_CAPACITY> createPayload(SensorData &data) {
    StaticJsonDocument<JSON_CAPACITY> json;

    if (data.activeHardware & hoCO2)
        json["co2"] = data.co2;

    if (data.activeHardware & hoPM)
        json["pm2"] = data.pm2;

    if (data.activeHardware & hoSHT) {
        json["temperature"] = data.th.t;
        json["humidity"] = data.th.rh;
    }

    json["board_time"] = data.boardTime;

    return json;
}

/*
 * Called when the sketch starts. Set up the sensors and display.
 */
void setup() {
    Serial.begin(9600);

    display.init();
    display.flipScreenVertically();

    showTextRectangle("Init", String(ESP.getChipId(), HEX), true, false);

    if (CONFIG.hardwareOptions & hoPM)
        ag.PMS_Init();
    if (CONFIG.hardwareOptions & hoCO2)
        ag.CO2_Init();
    if (CONFIG.hardwareOptions & hoSHT)
        ag.TMP_RH_Init(0x44);

    if (CONFIG.connectWifi)
        connectToWifi();

    delay(2000);
}

/*
 * The main loop.
 */
void loop() {
    struct SensorData data;

    data.activeHardware = CONFIG.hardwareOptions;
    data.boardTime = millis();

    if (data.activeHardware & hoCO2) {
        data.co2 = ag.getCO2_Raw();
        delay(200);
    }
    if (data.activeHardware & hoPM) {
        data.pm2 = ag.getPM2_Raw();
        delay(200);
    }
    if (data.activeHardware & hoSHT) {
        data.th = ag.periodicFetchData();
        delay(200);
    }

    // Display sensor data on the LCD
    displayFullStats(data);

    // Generate the JSON object from SensorData
    StaticJsonDocument<JSON_CAPACITY> json = createPayload(data);

    // Send the payload off to the server
    sendPayload(json);

    // The CO2 sensor can only be queried every 2 seconds otherwise it returns erroneous values
    delay(2000);

    // send payload
    //
    // update display
    //
    /*
    if (hasPM) {
        int PM2 = ag.getPM2_Raw();
        payload = payload + "\"pm02\":" + String(PM2);

        if (inUSaqi) {
            showTextRectangle("AQI", String(PM_TO_AQI_US(PM2)), false);
        } else {
            showTextRectangle("PM2", String(PM2), false);
        }

        delay(3000);
    }

    if (hasCO2) {
        if (hasPM)
            payload = payload + ",";
        int CO2 = ag.getCO2_Raw();
        payload = payload + "\"rco2\":" + String(CO2);
        showTextRectangle("CO2", String(CO2), false);
        delay(3000);
    }

    if (hasSHT) {
        if (hasCO2 || hasPM)
            payload = payload + ",";
        TMP_RH result = ag.periodicFetchData();
        payload = payload + "\"atmp\":" + String(result.t) + ",\"rhum\":" + String(result.rh);

        if (inF) {
            showTextRectangle(String((result.t * 9 / 5) + 32), String(result.rh) + "%", false);
        } else {
            showTextRectangle(String(result.t), String(result.rh) + "%", false);
        }

        delay(3000);
    }

    payload = payload + "}";

    // send payload
    if (connectWIFI) {
        Serial.println(payload);
        String POSTURL = APIROOT + "sensors/airgradient:" + String(ESP.getChipId(), HEX) + "/measures";
        Serial.println(POSTURL);
        WiFiClient client;
        HTTPClient http;
        http.begin(client, POSTURL);
        http.addHeader("content-type", "application/json");
        int httpCode = http.POST(payload);
        String response = http.getString();
        Serial.println(httpCode);
        Serial.println(response);
        http.end();
        delay(2000);
    }
    */
}

/*
 * Sends a JSON payload to the given address and endpoint.
 */
void sendPayload(StaticJsonDocument<JSON_CAPACITY> payload) {
    if (!connectToWifi)
        return;

    // String POSTURL = API_ENDPOINT + "sensors/airgradient:" + String(ESP.getChipId(), HEX) + "/measures";
    // Serial.println(POSTURL);
    WiFiClient client;
    HTTPClient http;
    String plString;

    // Store the sensor data into a JSON string
    serializeJson(payload, plString);

    http.begin(client, API_ENDPOINT);
    http.addHeader("content-type", "application/json");

    int httpCode = http.POST(plString);
    String response = http.getString();

    http.end();
}

/*
 * Displays all stats onto the string (CO2, PM2, temperature, and humidity).
 * Uses the tiniest font to fit everything onto the screen.
 */
void displayFullStats(String co2, String pm2, String temp, String hum) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(32, 14, "CO2: " + co2);
    display.drawString(32, 24, "PM2: " + pm2);
    display.drawString(32, 34, "T (C): " + temp);
    display.drawString(32, 44, "H: " + hum + "%");
    display.display();
}

void displayFullStats(SensorData &data) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    if (data.activeHardware & hoCO2)
        display.drawString(32, 14, "CO2: " + String(data.co2));

    if (data.activeHardware & hoPM)
        display.drawString(32, 24, "PM2: " + String(data.pm2));

    if (data.activeHardware & hoSHT) {
        display.drawString(32, 34, "T (C): " + String(data.th.t));
        display.drawString(32, 44, "H: " + String(data.th.rh) + "%");
    }

    display.display();
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, bool small, bool tiny) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    if (tiny) {
        display.setFont(ArialMT_Plain_10);
    } else if (small) {
        display.setFont(ArialMT_Plain_16);
    } else {
        display.setFont(ArialMT_Plain_24);
    }
    display.drawString(32, 16, ln1);
    display.drawString(32, 36, ln2);
    display.display();
}

// Wifi Manager
void connectToWifi() {
    WiFiManager wifiManager;
    // WiFi.disconnect(); //to delete previous saved hotspot
    String HOTSPOT = "AIRGRADIENT-" + String(ESP.getChipId(), HEX);
    wifiManager.setTimeout(120);
    if (!wifiManager.autoConnect((const char *)HOTSPOT.c_str())) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
        delay(5000);
    }
}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
    if (pm02 <= 12.0)
        return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
    else if (pm02 <= 35.4)
        return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
    else if (pm02 <= 55.4)
        return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
    else if (pm02 <= 150.4)
        return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
    else if (pm02 <= 250.4)
        return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
    else if (pm02 <= 350.4)
        return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
    else if (pm02 <= 500.4)
        return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
    else
        return 500;
};
