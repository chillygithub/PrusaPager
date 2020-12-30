/*
  PrusaPager
  Uses ESP8266 board with 0.91 inch 128x32 OLED display
  Original design used a Heltec WiFi 8 but should be compatible with similar boards
  https://heltec.org/project/wifi-kit-8/

  Jonathan Pope  v1  2020-12-29

  This code is shared freely for non-commercial use.
*/

// User configurable variables;

// Settings for Wireless network:
char* ssid = "networkname";              // Wireless network SSID 2.4GHz
char* pskey = "presharedkey";         // WPA2 Preshared key


// Update the IP address in the api_url to match your Mini:
// Mini should use a static IP address
char* api_url = "http://172.17.16.10/api/telemetry";


// Sound preferences for completed print tone:
int frequency = 300;                  //Hz
int duration = 500;                   //mS
int count = 3;                        // Number of bleeps

// Refresh interval sets period between API calls in mS
int refresh_interval = 10000;          //mS


// These settings are dependant on the board:
int passive_buzzer = 13;              // Pin for buzzer
int off_switch = 0;                   // Pin for Off switch

// Set the low battery threshold in ADC counts
// The ESP8266 does not provide access to the battery voltage, so the 3.3v regulated line vcc is used.
// When this drops the system will shutdown.
int lowbattery_threshold = 2700;



// Main code begines here. Only change if you know what you are doing.

#include <Arduino.h>                  //  https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Arduino.h
#include <ESP8266WiFi.h>              //  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFi.h
#include <ESP8266WiFiMulti.h>         //  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiMulti.h
#include <ESP8266HTTPClient.h>        //  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h
#include <WiFiClient.h>               //  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClient.h
#include <ArduinoJson.h>              //  https://arduinojson.org/
#include <SPI.h>                      //  https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.h
#include <Wire.h>                     //  https://github.com/esp8266/Arduino/blob/master/libraries/Wire/Wire.h
#include <Adafruit_GFX.h>             //  https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>         //  https://github.com/adafruit/Adafruit_SSD1306

//Setup OLED display
#define SCREEN_WIDTH 128              // OLED display width, in pixels
#define SCREEN_HEIGHT 32              // OLED display height, in pixels
#define OLED_RESET    16              // Reset pin 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ESP8266WiFiMulti WiFiMulti;

// Setup the ADC to allow the 3.3v line to be measured. If this drops below 3.3v the battery is discharged
ADC_MODE(ADC_VCC); // 3.3v voltage sensor
int vcc;

// Setup printing status variable
bool printing = false;

// Setup
void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Clear the OLED display and show the initialising screen
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // White text
  display.setCursor(0, 0);            // Start at top-left corner
  display.print("Initialising.");
  display.display();

  // Allow time for the wireless to connect
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    display.print(".");
    display.display();
    delay(1000);
  }

  // Display IP address
  display.println();
  display.print("SSID: ");
  display.println(WiFi.SSID());
  display.print("IP:   ");
  display.println(WiFi.localIP());
  display.print("Ch:   ");
  display.println(WiFi.channel());
  display.display();
  delay(1000);


  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, pskey);

  // Setup a pin for the buzzer
  pinMode (passive_buzzer, OUTPUT) ;
  alert(3000, 50, 1);                 // frequency, duration, count


  // setup pin for off switch
  pinMode(off_switch, INPUT);

}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {



    WiFiClient client;

    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, api_url)) {  // HTTP

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);

          // Set up buffer for JSON
          DynamicJsonDocument doc(384);

          // Parse JSON array returned from Mini
          deserializeJson(doc, payload);

          // Load varabiles from JSON Array
          int temp_nozzle = doc["temp_nozzle"];
          int temp_bed = doc["temp_bed"];
          const char* material = doc["material"];
          float pos_z_mm = doc["pos_z_mm"];
          int printing_speed = doc["printing_speed"];
          int flow_factor = doc["flow_factor"];
          int progress = doc["progress"];
          const char* print_dur = doc["print_dur"];
          long time_est = doc["time_est"];
          const char* time_zone = doc["time_zone"];
          String project_name = doc["project_name"];

          // Calculated variables
          String short_name = project_name.substring(0, 20);
          int remaining_minutes = (time_est / 60) % 60;
          int remaining_hours = ((time_est / 60) - remaining_minutes) / 60;


          // Print all varables to debug
          Serial.println("   Nozzle Temperature: " + String(temp_nozzle));
          Serial.println("   Bed Temperature: " + String(temp_bed));
          Serial.println("   Material: " + String(material));
          Serial.println("   Z positon mm: " + String(pos_z_mm));
          Serial.println("   Printing speed: " + String(printing_speed));
          Serial.println("   Flow factor: " + String(flow_factor));
          Serial.println("   Progress: " + String(progress));
          Serial.println("   Print Duration: " + String(print_dur));
          Serial.println("   Time estimate: " + String(time_est));
          Serial.println("   Time zone: " + String(time_zone));
          Serial.println("   Project name: " + String(project_name));
          Serial.println("   Short name: " + String(short_name));

          Serial.println("Printing Status: " + String(printing));

          // Update status display
          display.clearDisplay();
          display.setTextSize(1);               // Normal 1:1 pixel scale
          display.setTextColor(SSD1306_WHITE);  // White text
          display.setCursor(0, 0);              // Start at top-left corner

          if (short_name == "null") {
            display.println("Idle");
            display.println();
            display.println();

            // Play alert tone if status was "printing" and set printing status to false
            if (printing) {
              alert(frequency, duration, count);
              Serial.println("Sounding alert tone");
              printing = false;
            }

          } else {
            display.println(short_name);
            display.print("Remaining: ");
            if (remaining_hours != 0) {
              display.print(String(remaining_hours) + "h ");
            }
            display.println(String(remaining_minutes) + "m");
            display.println("Progress: " + String(progress) + "%");
            printing = true;   // set printer status
          }

          display.print("N: " + String(temp_nozzle) + " B: " + String(temp_bed) + " " + String(material));
          display.display();

        }
      } else {
        // Handle error when HTTP GET fails
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        display.clearDisplay();
        display.setTextSize(1);               // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE);  // White text
        display.setCursor(0, 0);              // Start at top-left corner
        display.println("Error:");
        display.println(http.errorToString(httpCode).c_str());
        display.println("Ensure Mini is on and connected to network.");
        display.display();

      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }

    // Check if off swich is held down
    bool off_switch_status = digitalRead(off_switch);
    Serial.print("Off switch status: ");
    Serial.println(off_switch_status);
    if (off_switch_status ==  false) {
      Serial.println("Power down requested");
      display.clearDisplay();
      display.setTextSize(1);               // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);  // White text
      display.setCursor(0, 0);              // Start at top-left corner
      display.println("Hold switch to power down");
      display.display();
      delay(2000);
      bool off_switch_status = digitalRead(off_switch);
      if (off_switch_status ==  false) {
        Serial.println("Power down");
        display.clearDisplay();
        display.setTextSize(1);               // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE);  // White text
        display.setCursor(0, 0);              // Start at top-left corner
        display.println("Power down");
        display.display();
        delay(1000);
        display.ssd1306_command(SSD1306_DISPLAYOFF);  // Switch the oled display off
        ESP.deepSleep(0);                     // Put the ESP8266 into deep sleep
      }
    }

    // Check for low battery, average of 5 reading is used for improved accuracy
    vcc = 0;
    int i;
    for (i = 0; i < 5; i++) {
      delay(5);
      vcc = vcc + ESP.getVcc();
    }
    vcc = vcc / 5;
    Serial.println("vcc ADC count: " + String(vcc));

    if (vcc < lowbattery_threshold) {
      Serial.println("Powering down due to low battery");
      alert(300, 200, 2);
      display.clearDisplay();
      display.setTextSize(1);               // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);  // White text
      display.setCursor(0, 0);              // Start at top-left corner
      display.println("Powering down due to Low Battery");
      display.display();
      delay(5000);
      display.ssd1306_command(SSD1306_DISPLAYOFF);  // Switch the oled display off
      ESP.deepSleep(0);                     // Put the ESP8266 into deep sleep
    }
  }
  // Pause for the refresh interval.
  delay(refresh_interval);
}

// Tone is played when print finishes
void alert (int f, int d, int c) {
  int i;
  for (i = 0; i < c; i++) {
    tone(passive_buzzer, f) ;
    delay (d);
    noTone(passive_buzzer) ;
    delay (d);
  }

}
