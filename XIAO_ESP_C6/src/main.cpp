#include <Arduino.h>
/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>      //Adiciona a biblioteca Adafruit NeoPixel
#include <Preferences.h>            //For NVS (Non-Volatile Storage)
#include <time.h>                   //For time functions


#define D_in D10          // arduino pin to handle data line
#define led_count 15      // Count of leds of stripe
#define NVS_NAMESPACE "nvm_params"

// Structure for persistent parameters
struct nvm_parameters {
  uint8_t brightness;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint32_t timestamp;  // Unix timestamp for RTC
  int8_t tz_offset_hours; // timezone offset hours (e.g. +1 for CET)
  uint8_t auto_dst; // 1=auto DST enabled, 0=disabled
};

Adafruit_NeoPixel pixels(led_count, D_in);
Preferences preferences;
nvm_parameters nvm_params;

// Software RTC variables
uint32_t rtc_timestamp = 0;
unsigned long last_millis = 0;
uint32_t last_printed_second = 0;

// Replace with your network credentials
const char* ssid     = "ESP32-Access-Point";
const char* password = "123456789";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output26State = "off";
String output27State = "off";

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;

uint32_t color_table[led_count] = {
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(255, 255, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255),
  pixels.Color(0, 0, 255)
};

// prototypes
void update_color_table();
void load_nvm_parameters();
void save_nvm_parameters();
void set_default_nvm_parameters();
void handle_wifi_client();
void update_rtc();
void set_rtc_time(uint32_t timestamp);
String get_rtc_string();



void setup() 
{
  Serial.begin(115200);
  pixels.begin();

  set_default_nvm_parameters();

  // Load persistent parameters
  load_nvm_parameters();

  update_color_table();

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();

  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
}


void loop() 
{
  // Update software RTC
  update_rtc();
  // Handle incoming client requests
  handle_wifi_client();
}

void update_color_table()
{
  pixels.clear(); // desliga todos os LEDs
  pixels.setBrightness(nvm_params.brightness);
  for (int i = 0; i < led_count; i++)
  {                                          
    pixels.setPixelColor(i, pixels.Color(nvm_params.red, nvm_params.green, nvm_params.blue)); 
    pixels.show();                           
  }
}


void load_nvm_parameters()
{
  preferences.begin(NVS_NAMESPACE, true); // read-only mode
  
  // Load parameters, use defaults if not found
  nvm_params.brightness = preferences.getUChar("brightness", 100);
  nvm_params.red = preferences.getUChar("red", 0);
  nvm_params.green = preferences.getUChar("green", 0);
  nvm_params.blue = preferences.getUChar("blue", 0);
  nvm_params.timestamp = preferences.getULong("timestamp", 0);
  nvm_params.tz_offset_hours = preferences.getChar("tz", 1);
  nvm_params.auto_dst = preferences.getUChar("auto_dst", 1);
  
  preferences.end();
  
  // Set RTC from saved timestamp and tz
  rtc_timestamp = nvm_params.timestamp;
  last_millis = millis();
  
  Serial.println("NVM Parameters loaded:");
  Serial.print("  Brightness: ");
  Serial.println(nvm_params.brightness);
  Serial.print("  Red: ");
  Serial.println(nvm_params.red);
  Serial.print("  Green: ");
  Serial.println(nvm_params.green);
  Serial.print("  Blue: ");
  Serial.println(nvm_params.blue);
  Serial.print("  RTC: ");
  Serial.println(get_rtc_string());
}


void save_nvm_parameters()
{
  preferences.begin(NVS_NAMESPACE, false); // write mode
  
  preferences.putUChar("brightness", nvm_params.brightness);
  preferences.putUChar("red", nvm_params.red);
  preferences.putUChar("green", nvm_params.green);
  preferences.putUChar("blue", nvm_params.blue);
  preferences.putULong("timestamp", rtc_timestamp);
  preferences.putChar("tz", nvm_params.tz_offset_hours);
  preferences.putUChar("auto_dst", nvm_params.auto_dst);
  
  preferences.end();
  
  Serial.println("NVM Parameters saved!");
}


void set_default_nvm_parameters()
{
  nvm_params.brightness = 100;
  nvm_params.red = 0;
  nvm_params.green = 0;
  nvm_params.blue = 255;
  nvm_params.timestamp = 0;  // Default to 1970-01-01
  nvm_params.tz_offset_hours = 1; // default CET
  nvm_params.auto_dst = 1; // enable DST by default
  save_nvm_parameters();
  update_color_table();
}


void update_rtc()
{
  // Update timestamp based on elapsed milliseconds
  unsigned long current_millis = millis();
  unsigned long elapsed = current_millis - last_millis;
  
  uint32_t sec_increment = elapsed / 1000;  // whole seconds elapsed
  if (sec_increment > 0) {
    rtc_timestamp += sec_increment;  // Add seconds
    // advance last_millis by the consumed whole seconds to keep remainder
    last_millis += sec_increment * 1000;

    // Print the current time once per second (when seconds change)
    if (rtc_timestamp != last_printed_second) {
      last_printed_second = rtc_timestamp;
      Serial.print("RTC: ");
      Serial.println(get_rtc_string());
    }
  }
}


void set_rtc_time(uint32_t timestamp)
{
  rtc_timestamp = timestamp;
  last_millis = millis();
  nvm_params.timestamp = rtc_timestamp;
  save_nvm_parameters();
  
  Serial.print("RTC set to: ");
  Serial.println(get_rtc_string());
}


String get_rtc_string()
{
  // Apply timezone offset stored in nvm_params and format as local time
  time_t adj = (time_t)rtc_timestamp + (int32_t)nvm_params.tz_offset_hours * 3600;

  // If auto DST is enabled, apply DST rule for typical European DST (last Sunday Mar-Oct)
  if (nvm_params.auto_dst) {
    // compute local tm for adjusted time
    time_t localt = adj;
    struct tm tm_local = *gmtime(&localt);
    int year = tm_local.tm_year + 1900;
    int month = tm_local.tm_mon + 1;
    int day = tm_local.tm_mday;
    int hour = tm_local.tm_hour;

    // helper: compute weekday for a given date (Zeller's congruence) 0=Sunday
    auto weekday = [](int y, int m, int d)->int {
      if (m < 3) { m += 12; y -= 1; }
      int K = y % 100;
      int J = y / 100;
      int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7; // 0=Saturday
      int w = ((h + 6) % 7); // convert to 0=Sunday
      return w;
    };

    auto lastSunday = [&](int y, int m)->int {
      int lastDay;
      // days per month
      if (m==1||m==3||m==5||m==7||m==8||m==10||m==12) lastDay = 31;
      else if (m==4||m==6||m==9||m==11) lastDay = 30;
      else { // feb
        bool leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
        lastDay = leap ? 29 : 28;
      }
      int wd = weekday(y, m, lastDay);
      int lastSun = lastDay - wd;
      return lastSun;
    };

    if ( (month > 3 && month < 10) ) {
      // definitely DST
      adj += 3600;
    } else if (month == 3) {
      int ls = lastSunday(year, 3);
      if (day > ls || (day == ls && hour >= 1)) adj += 3600;
    } else if (month == 10) {
      int ls = lastSunday(year, 10);
      if (day < ls || (day == ls && hour < 1)) adj += 3600;
    }
  }

  struct tm* timeinfo = gmtime(&adj); // use gmtime since we've adjusted

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}


void handle_wifi_client()
{
  WiFiClient client = server.accept(); // Listen for incoming clients

  if (client)
  {                                // If a new client connects,
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected())
    { // loop while the client's connected
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Parse HTTP requests for GPIO control
            if (header.indexOf("GET /26/on") >= 0)
            {
              Serial.println("GPIO 26 on");
              output26State = "on";
              digitalWrite(output26, HIGH);
            }
            else if (header.indexOf("GET /26/off") >= 0)
            {
              Serial.println("GPIO 26 off");
              output26State = "off";
              digitalWrite(output26, LOW);
            }
            else if (header.indexOf("GET /27/on") >= 0)
            {
              Serial.println("GPIO 27 on");
              output27State = "on";
              digitalWrite(output27, HIGH);
            }
            else if (header.indexOf("GET /27/off") >= 0)
            {
              Serial.println("GPIO 27 off");
              output27State = "off";
              digitalWrite(output27, LOW);
            }
            // Parse HTTP requests for brightness control
            else if (header.indexOf("GET /brightness/") >= 0)
            {
              int startIdx = header.indexOf("GET /brightness/") + 16;
              int endIdx = header.indexOf(" ", startIdx);
              String brightnessStr = header.substring(startIdx, endIdx);
              uint8_t brightnessValue = brightnessStr.toInt();
              nvm_params.brightness = brightnessValue;
              save_nvm_parameters();
              update_color_table();
              Serial.print("Brightness set to: ");
              Serial.println(nvm_params.brightness);
            }
            // Parse HTTP requests for red control
            else if (header.indexOf("GET /red/") >= 0)
            {
              int startIdx = header.indexOf("GET /red/") + 9;
              int endIdx = header.indexOf(" ", startIdx);
              String redStr = header.substring(startIdx, endIdx);
              uint8_t redValue = redStr.toInt();
              nvm_params.red = redValue;
              save_nvm_parameters();
              update_color_table();
              Serial.print("Red set to: ");
              Serial.println(nvm_params.red);
            }
            // Parse HTTP requests for green control
            else if (header.indexOf("GET /green/") >= 0)
            {
              int startIdx = header.indexOf("GET /green/") + 11;
              int endIdx = header.indexOf(" ", startIdx);
              String greenStr = header.substring(startIdx, endIdx);
              uint8_t greenValue = greenStr.toInt();
              nvm_params.green = greenValue;
              save_nvm_parameters();
              update_color_table();
              Serial.print("Green set to: ");
              Serial.println(nvm_params.green);
            }
            // Parse HTTP requests for blue control
            else if (header.indexOf("GET /blue/") >= 0)
            {
              int startIdx = header.indexOf("GET /blue/") + 10;
              int endIdx = header.indexOf(" ", startIdx);
              String blueStr = header.substring(startIdx, endIdx);
              uint8_t blueValue = blueStr.toInt();
              nvm_params.blue = blueValue;
              save_nvm_parameters();
              update_color_table();
              Serial.print("Blue set to: ");
              Serial.println(nvm_params.blue);
            }
            // Parse HTTP requests for RTC time setting (format: /settime/1234567890)
            else if (header.indexOf("GET /settime/") >= 0)
            {
              int startIdx = header.indexOf("GET /settime/") + 13;
              int endIdx = header.indexOf(" ", startIdx);
              String timestampStr = header.substring(startIdx, endIdx);
              uint32_t newTimestamp = (uint32_t)timestampStr.toInt();
              set_rtc_time(newTimestamp);
            }
            // Parse HTTP requests for timezone setting (format: /settz/<hours>)
            else if (header.indexOf("GET /settz/") >= 0)
            {
              int startIdx = header.indexOf("GET /settz/") + 11;
              int endIdx = header.indexOf(" ", startIdx);
              String tzStr = header.substring(startIdx, endIdx);
              int tz = tzStr.toInt();
              nvm_params.tz_offset_hours = (int8_t)tz;
              save_nvm_parameters();
              Serial.print("Timezone offset set to: ");
              Serial.println(nvm_params.tz_offset_hours);
            }
            // Parse HTTP requests for auto DST setting (format: /setautodst/0 or /setautodst/1)
            else if (header.indexOf("GET /setautodst/") >= 0)
            {
              int startIdx = header.indexOf("GET /setautodst/") + 16;
              int endIdx = header.indexOf(" ", startIdx);
              String v = header.substring(startIdx, endIdx);
              int val = v.toInt();
              nvm_params.auto_dst = (val != 0) ? 1 : 0;
              save_nvm_parameters();
              Serial.print("Auto DST set to: ");
              Serial.println(nvm_params.auto_dst);
            }
            // Parse HTTP requests for reset
            else if (header.indexOf("GET /reset") >= 0)
            {
              Serial.println("Resetting to default parameters");
              set_default_nvm_parameters();
            }
            // Parse HTTP requests for color control
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}");
            client.println("input[type=range] { width: 300px; height: 20px; margin: 10px; }");
            client.println("</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display RTC Section
            client.println("<h2>System Time (RTC)</h2>");
            client.println("<p>Current Time: " + get_rtc_string() + "</p>");
            //client.println("<p>Unix Timestamp: " + String(rtc_timestamp) + "</p>");
            //client.println("<p>Timezone offset (hours): " + String(nvm_params.tz_offset_hours) + "</p>");
            //client.println("<input type=\"text\" id=\"tzInput\" placeholder=\"e.g. 1 or -5\" value=\"" + String(nvm_params.tz_offset_hours) + "\" style=\"width:80px; padding:6px; margin:6px; font-size:16px;\">");
            //client.println("<button onclick=\"setTz()\" class=\"button\" style=\"padding: 8px 20px; font-size: 16px;\">Set TZ</button>");
            //client.println("<label style=\"margin-left:10px; font-size:16px;\"><input type=\"checkbox\" id=\"autoDstCb\" " + String(nvm_params.auto_dst ? "checked" : "") + " onclick=\"setAutoDst()\" /> Auto DST</label>");
            client.println("<button onclick=\"syncNow()\" class=\"button\" style=\"padding: 8px 20px; font-size: 16px;\">Sync time with smartphone</button>");

            // Display current state, and ON/OFF buttons for GPIO 26
            client.println("<h2>GPIO Control</h2>");
            client.println("<p>GPIO 26 - State " + output26State + "</p>");
            if (output26State == "off")
            {
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            }
            else
            {
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display current state, and ON/OFF buttons for GPIO 27
            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            if (output27State == "off")
            {
              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            }
            else
            {
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display LED Color Control Section
            client.println("<h2>LED Color Control</h2>");
            client.println("<p>Brightness: " + String(nvm_params.brightness) + "</p>");
            client.println("<input type=\"range\" min=\"0\" max=\"100\" value=\"" + String(nvm_params.brightness) + "\" id=\"brightnessSlider\">");
            
            client.println("<p>Red: " + String(nvm_params.red) + "</p>");
            client.println("<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(nvm_params.red) + "\" id=\"redSlider\">");
            
            client.println("<p>Green: " + String(nvm_params.green) + "</p>");
            client.println("<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(nvm_params.green) + "\" id=\"greenSlider\">");

            client.println("<p>Blue: " + String(nvm_params.blue) + "</p>");
            client.println("<input type=\"range\" min=\"0\" max=\"255\" value=\"" + String(nvm_params.blue) + "\" id=\"blueSlider\">");

            client.println("<p><a href=\"/reset\"><button class=\"button button2\">Reset to Default</button></a></p>");

            // JavaScript to handle slider inputs and RTC functions
            client.println("<script>");
            client.println("document.getElementById('brightnessSlider').addEventListener('input', function() {");
            client.println("  window.location = '/brightness/' + this.value;");
            client.println("});");
            client.println("document.getElementById('redSlider').addEventListener('input', function() {");
            client.println("  window.location = '/red/' + this.value;");
            client.println("});");
            client.println("document.getElementById('greenSlider').addEventListener('input', function() {");
            client.println("  window.location = '/green/' + this.value;");
            client.println("});");
            client.println("document.getElementById('blueSlider').addEventListener('input', function() {");
            client.println("  window.location = '/blue/' + this.value;");
            client.println("});");
            client.println("");
            client.println("function setTz() {");
            client.println("  var tz = document.getElementById('tzInput').value;");
            client.println("  if (tz !== '') {");
            client.println("    window.location = '/settz/' + tz;");
            client.println("  } else {");
            client.println("    alert('Please enter a timezone offset (e.g. 1 or -5)');");
            client.println("  }");
            client.println("}");
            client.println("function setAutoDst() {");
            client.println("  var cb = document.getElementById('autoDstCb');");
            client.println("  var v = cb.checked ? 1 : 0;");
            client.println("  window.location = '/setautodst/' + v;");
            client.println("}");
            client.println("function syncNow() {");
            client.println("  var now = Math.floor(Date.now() / 1000);");
            client.println("  window.location = '/settime/' + now;");
            client.println("}");
            client.println("</script>");
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
