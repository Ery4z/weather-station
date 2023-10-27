#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Arduino_JSON.h>
#include <Arduino.h>
#include <esp_now.h>
// Declaration of functions
void enterConfigMode();
void runningMode();
void refreshNetworks();

const int configPin = 4;

const char *ssid = "ESP32_Config";
const char *password = "";

int networksFound = 0;

// Global variables to control WiFi connection attempts
const int MAX_ATTEMPTS = 10; // Max number of times to attempt WiFi connection
int connectionAttempts = 0;
bool tryingToConnect = false;

typedef struct struct_message
{
    int id;
    float temp;
    float hum;
    unsigned int readingId;
} struct_message;

struct_message incomingReadings;

AsyncWebServer server(80);

AsyncEventSource events("/events");
JSONVar board;

String networkSSID;
String networkPASS;
bool networkConnected;

// Preferences object for EEPROM-like storage
Preferences preferences;

// Variables for software timer
unsigned long previousMillis = 0;
const long interval = 5000; // 5 seconds

bool configMode = false;

void setup()
{
    Serial.begin(115200);

    pinMode(configPin, INPUT_PULLDOWN);
    configMode = digitalRead(configPin) == HIGH ? true : false;

    // Open Preferences with the namespace "wifi"
    preferences.begin("wifi", false);

    networkSSID = preferences.getString("ssid", "");            // Get stored SSID, default to empty string
    networkPASS = preferences.getString("pass", "");            // Get stored password, default to empty string
    networkConnected = preferences.getBool("connected", false); // Get stored connection status, default to false
    if (configMode)
    {
        Serial.println("Pin is HIGH. Entering config mode...");
        enterConfigMode();
    }
    else
    {
        Serial.println("Pin is LOW. Entering running mode...");
        runningMode();
    }
}

void loop()
{
    if (configMode)
    {
        // Software timer for refreshing networks
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            previousMillis = currentMillis;
            if (WiFi.getMode() == WIFI_AP)
            { // Ensure we're in AP mode
                refreshNetworks();
            }
        }
        if (tryingToConnect)
        {
            // Non-blocking check for WiFi connection
            if (WiFi.status() == WL_CONNECTED)
            {
                preferences.putBool("connected", true);
                networkConnected = true;
                tryingToConnect = false;
                connectionAttempts = 0;
                Serial.println("Connected to WiFi!");
            }
            else
            {
                connectionAttempts++;
                if (connectionAttempts >= MAX_ATTEMPTS)
                {
                    preferences.putBool("connected", false);
                    networkConnected = false;
                    tryingToConnect = false;
                    connectionAttempts = 0;
                    Serial.println("Failed to connect to WiFi!");
                }
                else
                {
                    // Wait a bit before trying again
                    delay(1000);
                }
            }
        }
        return;
    }

    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 5000;
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
    {
        events.send("ping", NULL, millis());
        lastEventTime = millis();
    }
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
    // Copies the sender mac address to a string
    char macStr[18];
    Serial.print("Packet received from: ");
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.println(macStr);
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

    board["id"] = incomingReadings.id;
    board["temperature"] = incomingReadings.temp;
    board["humidity"] = incomingReadings.hum;
    board["readingId"] = String(incomingReadings.readingId);
    String jsonString = JSON.stringify(board);
    events.send(jsonString.c_str(), "new_readings", millis());

    Serial.printf("Board ID %u: %u bytes\n", incomingReadings.id, len);
    Serial.printf("t value: %4.2f \n", incomingReadings.temp);
    Serial.printf("h value: %4.2f \n", incomingReadings.hum);
    Serial.printf("readingID value: %d \n", incomingReadings.readingId);
    Serial.println();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP-NOW DASHBOARD</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #1 - TEMPERATURE</h4><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt1"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #1 - HUMIDITY</h4><p><span class="reading"><span id="h1"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh1"></span></p>
      </div>
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #2 - TEMPERATURE</h4><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt2"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #2 - HUMIDITY</h4><p><span class="reading"><span id="h2"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh2"></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('new_readings', function(e) {
  console.log("new_readings", e.data);
  var obj = JSON.parse(e.data);
  document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
  document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void enterConfigMode()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    String htmlPage = "<html><head><title>ESP32 Config</title></head>";
    htmlPage += "<body><h1>ESP32 Configuration</h1>";
    htmlPage += "<p>MAC Address: " + WiFi.macAddress() + "</p>";
    // Check if networkSSID is stored
    if (networkSSID != "") {
        // Add to your HTML code that a network is already configured with the SSID
        // Modify this to fit into your HTML structure
        htmlPage += "<p>Already configured for SSID: " + networkSSID + "</p>";
        if (networkConnected) {
            htmlPage += "<p>The credentials for this endpoints are correct.</p>";
        } else {
            if (tryingToConnect) {
                htmlPage += "<p>Attempting to connect to the network... \n Please refresh this page.</p>";
            } else {
                htmlPage += "<p>The credentials are probably incorrect, the connection tentative have failed</p>";
            }
        }
    }
    htmlPage += "<h2>Available networks:</h2>";
    
    if (networksFound == 0) {
      htmlPage += "<p>No networks found!</p>";
    } else {
      htmlPage += "<form action=\"/connect\" method=\"post\">";
      htmlPage += "<select name=\"ssid\">";
      for (int i = 0; i < networksFound; ++i) {
        //htmlPage += "<option value=\"False Network\"> False Network (9dBm)</option>";
        htmlPage += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
      }
      htmlPage += "</select><br>";
      htmlPage += "Password: <input type=\"password\" name=\"pass\"><br>";
      htmlPage += "<input type=\"submit\" value=\"Connect\">";
      htmlPage += "</form>";
    }
    
    htmlPage += "</body></html>";
    request->send(200, "text/html", htmlPage); });

    server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == "ssid") {
          networkSSID = p->value().c_str();
        }
        if (p->name() == "pass") {
          networkPASS = p->value().c_str();
        }
        // Save the SSID and password to Preferences after they are received
        preferences.putString("ssid", networkSSID);
        preferences.putString("pass", networkPASS);
        preferences.putBool("connected", false);
        networkConnected = false;
        

      }
    }
    // In a real-world application, save the SSID & password to EEPROM or SPIFFS here.
    // Start asynchronous connection attempts
    WiFi.begin(networkSSID.c_str(), networkPASS.c_str());
    tryingToConnect = true;

    request->send(200, "text/plain", "Attempting to connect... \n Refresh the config page to check the status."); });

    refreshNetworks(); // Initial call to populate the network list
    server.begin();
    Serial.println("Entered config mode!");
    // Add task to refresh every 5 seconds the list of available networks
}

void runningMode()
{
    // Placeholder function for running mode
    Serial.println("Entered running mode...");

    // Connect to the network using received credentials
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(networkSSID.c_str(), networkPASS.c_str());
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected!");
    preferences.putBool("connected", true);

    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info
    esp_now_register_recv_cb(OnDataRecv);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html); });

    events.onConnect([](AsyncEventSourceClient *client)
                     {
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000); });
    server.addHandler(&events);
    server.begin();
    Serial.println("Ready to receive data!");
}

void refreshNetworks()
{
    // Refresh the network list
    networksFound = WiFi.scanNetworks(false, true);
    Serial.println("Networks refreshed!");
}