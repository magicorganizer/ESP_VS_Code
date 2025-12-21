#include <Arduino.h>

// Load Wi-Fi library
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>      //Adiciona a biblioteca Adafruit NeoPixel
#include <Preferences.h>            //For NVS (Non-Volatile Storage)
#include <time.h>                   //For time functions


#define D_in D10          // arduino pin to handle data line
#define led_count 15      // Count of leds of stripe
#define NVS_NAMESPACE "nvm_params"

// Structure for a single timer slot (on or off time)
struct timer_slot {
  uint8_t hour;   // 0-23
  uint8_t minute; // 0-59
  uint8_t type;   // 0=off, 1=on
  uint8_t enabled; // 1=enabled, 0=disabled
};

// Structure for timer pair (on + off) + enable flag
struct timer_pair {
  timer_slot on_time;   // when to turn on (brightness=100)
  timer_slot off_time;  // when to turn off (brightness=0)
  uint8_t pair_enabled; // 1=this pair active, 0=inactive
};

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

// 2 timer pairs for schedule (early_on/early_off, evening_on/evening_off)
timer_pair timers[2];

// Software RTC variables
uint32_t rtc_timestamp = 0;
unsigned long last_millis = 0;
uint32_t last_printed_second = 0;

// Replace with your network credentials
const char* ssid     = "ESP32-Weihnachten";
const char* password = "123456789";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// prototypes
void update_color_table();
void load_nvm_parameters();
void save_nvm_parameters();
void set_default_nvm_parameters();
void handle_wifi_client();
void update_rtc();
void set_rtc_time(uint32_t timestamp);
String get_rtc_string();
void load_timers();
void save_timers();
void check_timers();
void set_timer_slot(uint8_t slot, uint8_t hour, uint8_t minute, uint8_t type, uint8_t enabled);
void set_timer_pair_enabled(uint8_t pair, uint8_t enabled);



void setup() 
{
  Serial.begin(115200);
  pixels.begin();

  // Load persistent parameters
  load_nvm_parameters();

  // Load timers from NVS
  load_timers();

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
  // Check and apply timers
  check_timers();
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
  nvm_params.red = 255;
  nvm_params.green = 255;
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


void load_timers()
{
  preferences.begin(NVS_NAMESPACE, true); // read-only mode
  
  // Load all 2 timer pairs
  for (int i = 0; i < 2; i++) {
    String on_h_key = "t" + String(i) + "_on_h";
    String on_m_key = "t" + String(i) + "_on_m";
    String off_h_key = "t" + String(i) + "_off_h";
    String off_m_key = "t" + String(i) + "_off_m";
    String en_key = "t" + String(i) + "_en";
    
    timers[i].on_time.hour = preferences.getUChar(on_h_key.c_str(), 8);
    timers[i].on_time.minute = preferences.getUChar(on_m_key.c_str(), 0);
    timers[i].on_time.type = 1; // always on
    timers[i].on_time.enabled = 1;
    
    timers[i].off_time.hour = preferences.getUChar(off_h_key.c_str(), 22);
    timers[i].off_time.minute = preferences.getUChar(off_m_key.c_str(), 0);
    timers[i].off_time.type = 0; // always off
    timers[i].off_time.enabled = 1;
    
    timers[i].pair_enabled = preferences.getUChar(en_key.c_str(), (i == 0) ? 1 : 0);
  }
  
  preferences.end();
  
  Serial.println("Timers loaded from NVS");
}


void save_timers()
{
  preferences.begin(NVS_NAMESPACE, false); // write mode
  
  for (int i = 0; i < 2; i++) {
    String on_h_key = "t" + String(i) + "_on_h";
    String on_m_key = "t" + String(i) + "_on_m";
    String off_h_key = "t" + String(i) + "_off_h";
    String off_m_key = "t" + String(i) + "_off_m";
    String en_key = "t" + String(i) + "_en";
    
    preferences.putUChar(on_h_key.c_str(), timers[i].on_time.hour);
    preferences.putUChar(on_m_key.c_str(), timers[i].on_time.minute);
    preferences.putUChar(off_h_key.c_str(), timers[i].off_time.hour);
    preferences.putUChar(off_m_key.c_str(), timers[i].off_time.minute);
    preferences.putUChar(en_key.c_str(), timers[i].pair_enabled);
  }
  
  preferences.end();
  
  Serial.println("Timers saved to NVS");
}


void check_timers()
{
  // Get current hour and minute from RTC
  time_t now = rtc_timestamp + (int32_t)nvm_params.tz_offset_hours * 3600;
  struct tm* timeinfo = gmtime(&now);
  int cur_hour = timeinfo->tm_hour;
  int cur_min = timeinfo->tm_min;
  
  // Check each enabled timer pair
  for (int i = 0; i < 2; i++) {
    if (!timers[i].pair_enabled) continue;
    
    // Check if we match the ON time exactly
    if (cur_hour == timers[i].on_time.hour && cur_min == timers[i].on_time.minute) {
      nvm_params.brightness = 100;
      save_nvm_parameters();
      update_color_table();
      Serial.print("Timer ");
      Serial.print(i);
      Serial.println(" ON triggered");
      delay(30000); // prevent re-trigger within 30 seconds
    }
    
    // Check if we match the OFF time exactly
    if (cur_hour == timers[i].off_time.hour && cur_min == timers[i].off_time.minute) {
      nvm_params.brightness = 0;
      save_nvm_parameters();
      update_color_table();
      Serial.print("Timer ");
      Serial.print(i);
      Serial.println(" OFF triggered");
      delay(30000); // prevent re-trigger within 30 seconds
    }
  }
}


void set_timer_slot(uint8_t pair, uint8_t hour, uint8_t minute, uint8_t type, uint8_t enabled)
{
  if (pair >= 2) return;
  
  if (type == 1) { // ON time
    timers[pair].on_time.hour = hour;
    timers[pair].on_time.minute = minute;
    timers[pair].on_time.type = 1;
    timers[pair].on_time.enabled = enabled;
  } else { // OFF time
    timers[pair].off_time.hour = hour;
    timers[pair].off_time.minute = minute;
    timers[pair].off_time.type = 0;
    timers[pair].off_time.enabled = enabled;
  }
  
  save_timers();
}


void set_timer_pair_enabled(uint8_t pair, uint8_t enabled)
{
  if (pair >= 2) return;
  timers[pair].pair_enabled = (enabled != 0) ? 1 : 0;
  save_timers();
  
  Serial.print("Timer pair ");
  Serial.print(pair);
  Serial.print(" set to: ");
  Serial.println(timers[pair].pair_enabled);
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

            // Parse HTTP requests for brightness control
            if (header.indexOf("GET /brightness/") >= 0)
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
            // Parse HTTP requests for timer control (format: /settimer/pair/type/hour/minute)
            // type: 0=off time, 1=on time
            else if (header.indexOf("GET /settimer/") >= 0)
            {
              int startIdx = header.indexOf("GET /settimer/") + 14;
              // Parse: /settimer/pair/type/hour/minute
              int idx1 = header.indexOf("/", startIdx);
              int idx2 = header.indexOf("/", idx1 + 1);
              int idx3 = header.indexOf("/", idx2 + 1);
              int idx4 = header.indexOf(" ", idx3 + 1);
              
              String pair_str = header.substring(startIdx, idx1);
              String type_str = header.substring(idx1 + 1, idx2);
              String hour_str = header.substring(idx2 + 1, idx3);
              String minute_str = header.substring(idx3 + 1, idx4);
              
              uint8_t pair = pair_str.toInt();
              uint8_t type = type_str.toInt();
              uint8_t hour = hour_str.toInt();
              uint8_t minute = minute_str.toInt();
              
              set_timer_slot(pair, hour, minute, type, 1);
              
              Serial.print("Timer ");
              Serial.print(pair);
              Serial.print(" ");
              Serial.print(type ? "ON" : "OFF");
              Serial.print(" set to ");
              Serial.print(hour);
              Serial.print(":");
              if (minute < 10) Serial.print("0");
              Serial.println(minute);
            }
            // Parse HTTP requests for timer pair enable/disable (format: /settimeren/pair/0|1)
            else if (header.indexOf("GET /settimeren/") >= 0)
            {
              int startIdx = header.indexOf("GET /settimeren/") + 16;
              int idx1 = header.indexOf("/", startIdx);
              int idx2 = header.indexOf(" ", idx1 + 1);
              
              String pair_str = header.substring(startIdx, idx1);
              String enabled_str = header.substring(idx1 + 1, idx2);
              
              uint8_t pair = pair_str.toInt();
              uint8_t enabled = enabled_str.toInt();
              
              set_timer_pair_enabled(pair, enabled);
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
            
            // Timer functions
            client.println("function setTimer(pair, type) {");
            client.println("  var hour = document.getElementById('timer' + pair + '_' + type + '_h').value;");
            client.println("  var minute = document.getElementById('timer' + pair + '_' + type + '_m').value;");
            client.println("  window.location = '/settimer/' + pair + '/' + type + '/' + hour + '/' + minute;");
            client.println("}");
            client.println("function setTimerEnabled(pair) {");
            client.println("  var enabled = document.getElementById('timerCb' + pair).checked ? 1 : 0;");
            client.println("  window.location = '/settimeren/' + pair + '/' + enabled;");
            client.println("}");
            
            client.println("</script>");
            
            // Add Timer Schedule section before closing body
            client.println("<h2>Timer Schedule</h2>");
            
            for (int i = 0; i < 2; i++) {
              client.println("<div style=\"border:1px solid #ccc; margin:10px; padding:10px; border-radius:5px;\">");
              client.println("<label><input type=\"checkbox\" id=\"timerCb" + String(i) + "\" " + 
                String(timers[i].pair_enabled ? "checked" : "") + 
                " onchange=\"setTimerEnabled(" + String(i) + ")\" /> Pair " + String(i + 1) + " Enabled</label>");
              
              // ON time - button inline
              client.println("<p>Turn ON at: ");
              client.println("<input type=\"number\" id=\"timer" + String(i) + "_1_h\" min=\"0\" max=\"23\" value=\"" + String(timers[i].on_time.hour) + "\" style=\"width:50px;\"> : ");
              client.println("<input type=\"number\" id=\"timer" + String(i) + "_1_m\" min=\"0\" max=\"59\" value=\"" + String(timers[i].on_time.minute) + "\" style=\"width:50px;\"> ");
              client.println("<button style=\"padding:6px 12px; font-size:14px;\" onclick=\"setTimer(" + String(i) + ", 1)\">Set</button></p>");
              
              // OFF time - button inline
              client.println("<p>Turn OFF at: ");
              client.println("<input type=\"number\" id=\"timer" + String(i) + "_0_h\" min=\"0\" max=\"23\" value=\"" + String(timers[i].off_time.hour) + "\" style=\"width:50px;\"> : ");
              client.println("<input type=\"number\" id=\"timer" + String(i) + "_0_m\" min=\"0\" max=\"59\" value=\"" + String(timers[i].off_time.minute) + "\" style=\"width:50px;\"> ");
              client.println("<button style=\"padding:6px 12px; font-size:14px;\" onclick=\"setTimer(" + String(i) + ", 0)\">Set</button></p>");
              
              client.println("</div>");
            }
            
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
