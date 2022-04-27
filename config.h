#ifndef _CONFIG_H_
#define _CONFIG_H_

// Different hardware that can be on the board depending on what you DIY.
// Hardware: CO2 sensor, PM2.5 sensor, Temp/humidity sensor
enum HardwareOptions {
    hoCO2 = 0x01,
    hoPM = 0x02,
    hoSHT = 0x04,
};

/* Configuration values for the board.
 * fields:
 *  hardwareOptions, bitfield which represents the available hardware on the board
 *  useFarenheit,    if true, use Farenheit instead of Celsius
 *  connectWifi,     if true, connect to wifi
 *  apiEndpoint,     URL to an endpoint to send sensor data
 *  location,        location of the sensor
 */
struct Config {
    unsigned char hardwareOptions;
    bool useFarenheit;
    bool connectWifi;
    String apiEndpoint;
    String location;
};

// Set up your config values here
struct Config CONFIG = {
    hoCO2 | hoPM | hoSHT,
    false,
    true,
    "http://192.168.0.69:9000/airquality",
    "office"
};

#endif