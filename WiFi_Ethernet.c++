#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>
#include <EthernetENC.h>
#include <SPI.h>

// WiFi Configuration
const char* ssid = "DSAP";
const char* password = "12345678901";

// MQTT Configuration
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
#define CLIENT_ID       "JEZIEL_78233"
#define INTERVAL        3000 // 3 sec delay between publishing

// Ethernet Configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 3, 177);
IPAddress myDns(192, 168, 3, 1);

// Ethernet or WiFi client
WiFiClient wifiClient;
EthernetClient ethClient;
PubSubClient mqttClient;
WebServer server(80);

String mqttTopic = "/ESPWIFITEAM"; // Default MQTT Topic
String inputString = ""; // String to store incoming serial message
bool stringComplete = false;  // Indicator for complete string

long previousMillis;
unsigned long lastPingTime = 0;
#define PING_INTERVAL 30000 // 30 seconds

bool useEthernet = false; // Current connection method
bool useWifi = false;

// Callback function to handle incoming MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received in topic [");
  Serial.print(topic);
  Serial.print("]: ");
  String message;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();
}

// WiFi setup function
bool setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed");
    return false;
  }
}

// Ethernet setup function
bool setup_ethernet() {
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
    return false;
  } else {
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Initialize Ethernet with DHCP:");
      Serial.println("Failed to configure Ethernet using DHCP");
      if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield was not found.");
      } else {
        return false;
      }
    } else {
      Serial.print("DHCP assigned IP ");
      Serial.println(Ethernet.localIP());
      return true;
    }
  }
}

// Connect to network (either Ethernet or WiFi)
bool connectToNetwork() {
  if (setup_ethernet()) {
    useEthernet = true;
    useWifi = false;
    mqttClient.setClient(ethClient);
    return true;
  } else if (setup_wifi()) {
    useEthernet = false;
    useWifi = true;
    mqttClient.setClient(wifiClient);
    return true;
  } else {
    Serial.println("Failed to connect to both Ethernet and WiFi");
    return false;
  }
}

// Reconnect to MQTT
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe(mqttTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

// Send serial message to MQTT
void sendSerialMessageToMQTT() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }

  if (stringComplete) {
    Serial.print("Sending message to MQTT: ");
    Serial.println(inputString);
    inputString.trim();
    
    if (inputString.startsWith("topic:")) {
      mqttTopic = inputString.substring(6);
      Serial.print("Changing MQTT topic to: ");
      Serial.println(mqttTopic);
    } else {
      mqttClient.publish(mqttTopic.c_str(), inputString.c_str());
    }
    
    inputString = "";
    stringComplete = false;
  }
}

void setup() {
  Serial.begin(115200);

  if (!connectToNetwork()) {
    // Handle initial connection failure if needed
  }

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Hello from ESP32!");
  });

  server.begin();

  previousMillis = millis();
}

void loop() {
  server.handleClient();
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  sendSerialMessageToMQTT();

  unsigned long now = millis();
  if (now - lastPingTime > PING_INTERVAL) {
    lastPingTime = now;
    if (Ping.ping("www.google.com")) {
      mqttClient.publish(mqttTopic.c_str(), "Ping successful!");
    } else {
      mqttClient.publish(mqttTopic.c_str(), "Ping failed.");
    }
  }

  if ((useEthernet && Ethernet.linkStatus() == LinkOFF) || 
      (useWifi && WiFi.status() != WL_CONNECTED)) {
    Serial.println("Connection lost, trying to reconnect...");
    connectToNetwork();
  } else if (useWifi && Ethernet.linkStatus() != LinkOFF) {
    Serial.println("Ethernet detected, switching to Ethernet...");
    connectToNetwork();
  }

  if (now - previousMillis > INTERVAL) {
    previousMillis = now;
    // Add any periodic tasks here
  }
}