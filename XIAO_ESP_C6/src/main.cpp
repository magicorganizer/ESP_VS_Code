#include <Arduino.h>
/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>      //Adiciona a biblioteca Adafruit NeoPixel
#include <Preferences.h>            //For NVS (Non-Volatile Storage)


#define D_in D10          // arduino pin to handle data line
#define led_count 15      // Count of leds of stripe
#define NVS_NAMESPACE "nvm_params"

// Structure for persistent parameters
struct nvm_parameters {
  uint8_t brightness;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

Adafruit_NeoPixel pixels(led_count, D_in);
Preferences preferences;
nvm_parameters nvm_params;

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
  
  preferences.end();
  
  Serial.println("NVM Parameters loaded:");
  Serial.print("  Brightness: ");
  Serial.println(nvm_params.brightness);
  Serial.print("  Red: ");
  Serial.println(nvm_params.red);
  Serial.print("  Green: ");
  Serial.println(nvm_params.green);
  Serial.print("  Blue: ");
  Serial.println(nvm_params.blue);
}


void save_nvm_parameters()
{
  preferences.begin(NVS_NAMESPACE, false); // write mode
  
  preferences.putUChar("brightness", nvm_params.brightness);
  preferences.putUChar("red", nvm_params.red);
  preferences.putUChar("green", nvm_params.green);
  preferences.putUChar("blue", nvm_params.blue);
  
  preferences.end();
  
  Serial.println("NVM Parameters saved!");
}


void set_default_nvm_parameters()
{
  nvm_params.brightness = 100;
  nvm_params.red = 0;
  nvm_params.green = 0;
  nvm_params.blue = 255;
  save_nvm_parameters();
  update_color_table();
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

            // JavaScript to handle slider inputs
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
