/*
 * file: airgradient.ino
 * desc: Powers the AirGradient air quality sensor with an ESP8266.
 *       See airgradient.com/diy for additional info.
 *       Uses the following sensors:
 *          Plantower PMS5003 (particulate matter)
 *          SenseAir S8 (CO2)
 *          SHT30/31 (temperature/humidity)
 *
 *       Uses the following libraries:
 *          ESP8266 board manager (>= v3.0.0)
 *          WifiManager by tzapu, tablatronix (>= v2.0.3-alpha)
 *          ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg (>= v4.1.0)
 */
#include <AirGradient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <Wire.h>

#include "SSD1306Wire.h"
#include "config.h"

/*
 * Structs
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
    String location;
};

/*
 * Globals / consts
 */

AirGradient ag = AirGradient();

// Capacity of the JSON object payload in bytes
const size_t JSON_CAPACITY = 512;

SSD1306Wire display(0x3c, SDA, SCL);

/*
 * Function decls
 */

StaticJsonDocument<JSON_CAPACITY> createPayload(SensorData &data);

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
        connectToWifi(CONFIG.wifiSSID, CONFIG.wifiPassword);

    delay(2000);
}

/*
 * Reset function which will restart the board.
 */
void (*restart)(void) = 0;

/*
 * The main loop.
 */
void loop() {
    struct SensorData data;

    data.activeHardware = CONFIG.hardwareOptions;
    data.boardTime = millis();
    data.location = CONFIG.location;

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
    sendPayload(json, CONFIG.apiEndpoint);

    // Wait for ~4 seconds since the CO2 sensor can only be queried at longer intervals otherwise
    // it returns erroneous values
    delay(4200);

    // Has it been longer than six hours? Restart the board. This is done to prevent erroneous
    // CO2 values which keep popping up despite trying some of the fixes here
    // (https://forum.airgradient.com/t/s8-co2-reading-of-1/69/60) including printing out a custom
    // PCB.
    if (millis() >= (12 * 60 * 60 * 1000)) {
        restart();
    }
}

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

    json["active_hardware"] = data.activeHardware;
    json["board_time"] = data.boardTime;
    json["location"] = data.location;

    return json;
}

/*
 * Sends a JSON payload to the given address and endpoint.
 *
 * args
 *  payload,  a JSON object representing the data to send to the server
 *  endpoint, a server URL to send the data to
 */
void sendPayload(StaticJsonDocument<JSON_CAPACITY> payload, String endpoint) {
    if (!connectToWifi)
        return;

    WiFiClient client;
    HTTPClient http;
    String plString;

    // Store the sensor data into a JSON string
    serializeJson(payload, plString);

    http.begin(client, endpoint);
    http.addHeader("content-type", "application/json");

    int httpCode = http.POST(plString);
    String response = http.getString();

    Serial.println("sendPayload HTTP code: " + String(httpCode));
    Serial.println("sendPayload response: " + response);

    http.end();
}

/*
 * Displays all stats onto the string (CO2, PM2, temperature, and humidity).
 * Uses the tiniest font to fit everything onto the screen.
 *
 * args
 *  data, SensorData object
 */
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

/*
 * Helper function to display two lines of text onto the LCD screen.
 * Uses the tiniest font to fit everything onto the screen.
 */
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

/*
 * Same as above but displays up to four lines of text using a smaller font.
 *
 * args
 *  strings, one for each line
 */
void showTinyTextRectangle(String ln1, String ln2, String ln3, String ln4) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    display.drawString(32, 16, ln1);
    display.drawString(32, 26, ln2);
    display.drawString(32, 36, ln3);
    display.drawString(32, 46, ln4);
    display.display();
}

/*
 * Connects to a wifi network.
 */
void connectToWifi(String ssid, String password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        showTinyTextRectangle("WIFI", "connecting...", "status: " + String(WiFi.status()), "");
    }
    showTinyTextRectangle("WIFI", "CONNECTED", WiFi.localIP().toString(), "");
}
