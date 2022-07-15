# AirGradient

AirGradient is a [DIY sensor](https://www.airgradient.com/diy/) designed to monitor air quality,
including CO2, particulate matter, temperature, and humidity.
This repo contains a custom firmware to flash onto the AirGradient.
The changes made to the original firmware include some code cleanup, proper JSON 
serialization, easier WIFI configuration, and integration with a custom home automation library 
(HAL). 
Additionally, the firmware restarts the device every six-ish hours; I've ran into some problems with
invalid CO2 measurements (possibly a parts batch problem?) despite building two devices using two 
different PCBs, one of which I printed myself. Restarting the board seems to fix the issue.

## Development

This section describes setting up a development environment for VS Code. 

### 1. Install the [Arduino IDE](https://www.arduino.cc/en/software#download)

VS Code runs this in the background, so it's necessary for development.

### 2. Install the Arduino extension

Install the VS Code Arduino extension (identifier `vsciot-vscode.vscode-arduino`).

### 3. Install required Arduino libraries

There are a few libraries Arduino that AirGradient makes use of.
Launch the Arduino IDE, then navigate to `Tools > Manage Libraries...`.

Using the `Library Manager` search, find and install the following libraries:

* ESP8266 board manager (>= v3.0.0)
* WifiManager by tzapu, tablatronix (>= v2.0.3-alpha)
* ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg (>= v4.1.0)

### 4. Finish setup

The `.vscode/arduino.json` should contain the necessary configuration to enable VS Code to write 
the firmware to the board.
In the bottom status bar, ensure the `Board Config` is set to `LOLIN(WEMOS) D1 R2 & mini`, and 
the correct serial port is set, mine is `/dev/tty.usbserial-210`.