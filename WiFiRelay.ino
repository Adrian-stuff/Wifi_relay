#include <ETH.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// *********** relayPins ***********
int relayPins[] = { 26, 27 };

// *********** WIFI CONFIG ***********
const char* ssid = "WAG KAMI MABAGAL NET!!";
const char* password = "binatog143";

// *********** RELAY STATES ***********
bool relayStates[] = { false, false };
int arrLength = sizeof(relayPins) / sizeof(int);

String relayJSON;
// *********** SERVER CONFIG ***********
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

IPAddress local_IP(192, 168, 1, 69); //nice
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

// *********** WEBPAGE ***********
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>WiFi Relay</title>
  </head>
  <style>
    html {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen,
        Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif;
    }
    .main {
      display: flex;
      justify-content: center;
      align-items: center;
      flex-direction: column;
      height: 100vh;
      font-size: xx-large;
    }
    .button {
      font-size: larger;
      min-height: 35px;
      border: none;
      border-radius: 10px;
      background-color: lightblue;
    }
    .button:hover {
      background-color: gray;
    }
    .state-container {
      display: flex;
      flex-direction: column;
      justify-content: center;
    }
  </style>
  <body>
    <div class="main">
      <h1>WiFi Relay</h1>
      <div class="state-container">
        <h2 id="state1">%STATE%</h2>
        <button id="button1" class="button">Toggle</button>
      </div>
      <div class="state-container">
        <h2 id="state2">%STATE2%</h2>
        <button id="button2" class="button">Toggle</button>
      </div>
    </div>
    <script>
      var gateway = `ws://${window.location.hostname}/ws`;
      var websocket;
      window.addEventListener("load", onLoad);
      function initWebSocket() {
        console.log("Trying to open a WebSocket connection...");
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
      }
      function onOpen(event) {
        console.log("Connection opened");
      }
      function onClose(event) {
        console.log("Connection closed");
        setTimeout(initWebSocket, 2000);
      }
      function onMessage(event) {
        var res = JSON.parse(event.data);
        var state1 = res.relay1 == "1" ? "ON" : "OFF";
        var state2 = res.relay2 == "1" ? "ON" : "OFF";
        document.getElementById("state1").innerHTML = state1;
        document.getElementById("button1").textContent = state1;
        document.getElementById("state2").innerHTML = state2;
        document.getElementById("button2").textContent = state2;
      }
      function onLoad(event) {
        initWebSocket();
        initButton();
      }
      function initButton() {
        document
          .getElementById("button1")
          .addEventListener("click", () => websocket.send("toggle1"));
        document
          .getElementById("button2")
          .addEventListener("click", () => websocket.send("toggle2"));
      }
    </script>
  </body>
</html>
)rawliteral";

void notifyClients(){
  relayJSON = "{\"relay1\":\"" + String(relayStates[0]) + "\", \"relay2\":\"" + String(relayStates[1]) + "\"}";
  Serial.println(relayJSON);
  ws.textAll(relayJSON);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle1") == 0) {
      relayStates[0] = !relayStates[0];
      notifyClients();
    }
    if (strcmp((char*)data, "toggle2") == 0) {
      relayStates[1] = !relayStates[1];
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE") return relayStates[0] ? "ON" : "OFF";
  else if(var == "STATE2") return relayStates[1] ? "ON" : "OFF";
  return String();
}

void setup() {
  Serial.begin(115200);

  for(int i=0; i < arrLength; i++){
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  if (!WiFi.config(local_IP, gateway, subnet))
      Serial.println("STA Failed to configure");

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi");  
  }

  Serial.println(WiFi.localIP());

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html, processor);
  });
  server.begin();
}

void loop() {
  ws.cleanupClients();
  for(int j=0; j<arrLength; j++){
    digitalWrite(relayPins[j], !relayStates[j]);
  }
}
