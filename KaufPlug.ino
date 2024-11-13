#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>   // For captive portal and Wi-Fi setup
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Define constants for pins and other settings
#define RELAY_PIN 13
#define BUTTON_PIN 3
#define BLUE_LED_PIN 12  // Blue LED for relay off
#define RED_LED_PIN 1    // Red LED for relay on
#define PING_INTERVAL 60000    // Ping every 60 seconds
#define PING_HOST "8.8.8.8"    // Google DNS server for pinging
#define RELAY_ON_DELAY 2 * PING_INTERVAL  // 2 failed pings before turning on relay

// Global variables for timing and relay state
WiFiManager wifiManager;
ESP8266WebServer server(80);
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", -5 * 3600, 60000); // EST Time (-5 UTC)

unsigned long lastPingTime = 0;
unsigned long lastSuccessPingTime = 0;
unsigned long lastFailPingTime = 0;
unsigned long lastRelayToggleTime = 0;
bool internetConnected = false;
bool relayState = false;

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Ensure initial states of the relay and LEDs
  digitalWrite(RELAY_PIN, LOW);  // Ensure relay starts off
  digitalWrite(BLUE_LED_PIN, LOW);    // Ensure Blue LED starts off
  digitalWrite(RED_LED_PIN, LOW);    // Ensure Red LED starts off
  
  // Start Wi-Fi Manager for captive portal or Wi-Fi setup
  wifiManager.autoConnect("Luli Fama Plug AP");

  // Initialize time client (sync with NTP server)
  timeClient.begin();
  timeClient.update();
  
  // Start OTA
  ArduinoOTA.setPassword("12345678");
  ArduinoOTA.begin();

  // Serve web page
  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggleRelay", HTTP_GET, toggleRelay);
  server.begin();
}

void loop() {
  // Handle OTA requests
  ArduinoOTA.handle();

  // Handle web server requests
  server.handleClient();

  // Periodically ping the internet connection every 60 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastPingTime >= PING_INTERVAL) {
    lastPingTime = currentMillis;
    internetConnected = pingInternet(PING_HOST);
    
    if (internetConnected) {
      lastSuccessPingTime = currentMillis;
      lastFailPingTime = 0;  // Reset fail counter if connection is successful
    } else {
      if (lastFailPingTime == 0) {  // First failure
        lastFailPingTime = currentMillis;
      }
    }
  }

  // If two consecutive pings failed (120 seconds), toggle the relay on
  if (!internetConnected && (currentMillis - lastFailPingTime >= RELAY_ON_DELAY)) {
    if (!relayState) {
      relayState = true;
      digitalWrite(RELAY_PIN, HIGH); // Turn on relay
    }
  }

  // Turn off the relay at 11:00 PM if the internet is back on
  if (internetConnected && relayState) {
    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    
    if (currentHour == 23 && currentMinute == 00) {
      relayState = false;
      digitalWrite(RELAY_PIN, LOW);  // Turn off relay
    }
  }

  // Update LEDs based on relay state
  if (relayState) {
    digitalWrite(RED_LED_PIN, HIGH);  // Turn on Red LED when relay is on
    digitalWrite(BLUE_LED_PIN, LOW);  // Ensure Blue LED is off
  } else {
    digitalWrite(RED_LED_PIN, LOW);   // Ensure Red LED is off
    digitalWrite(BLUE_LED_PIN, HIGH); // Turn on Blue LED when relay is off
  }
}

// Function to handle the root page (display the status)
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Luli Fama Plug Web Control</h1>";
  
  // Relay control
  html += "<p><a href='/toggleRelay'>Toggle Relay</a></p>";

  // Internet connection status
  html += "<p>Internet Connection: " + String(internetConnected ? "Connected" : "Disconnected") + "</p>";

  // Current time (synchronized with NTP)
  html += "<p>Current Time: " + timeClient.getFormattedTime() + "</p>";

  // Last ping time (show time since last ping)
  unsigned long timeSinceLastPing = millis() - lastPingTime;
  html += "<p>Last Ping Time: " + String(timeSinceLastPing / 1000) + " seconds ago</p>";
  
  // Time until next ping
  unsigned long timeUntilNextPing = PING_INTERVAL - (millis() - lastPingTime);
  html += "<p>Time Until Next Ping: " + String(timeUntilNextPing / 1000) + " seconds</p>";

  // Relay status
  html += "<p>Relay Status: " + String(relayState ? "ON" : "OFF") + "</p>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Function to toggle the relay
void toggleRelay() {
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

// Function to ping the internet and return true/false based on success
bool pingInternet(const char *host) {
  WiFiClient client;
  if (client.connect(host, 53)) {
    client.stop();
    return true;
  }
  return false;
}
