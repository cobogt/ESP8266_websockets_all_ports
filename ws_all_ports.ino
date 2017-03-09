#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include "FS.h"
#include <WebSockets.h>
#include <Hash.h>
#include <WebSocketsClient.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ArduinoJson.h>
#include <PubSubClient.h>

ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WiFiClient wifiClient;

#include <WebSocketsServer.h>
#include <Hash.h>

boolean DEBUG_MODE = true;

const char *ssid = "WSALLPORTS";
const char *password = "";
WebSocketsServer webSocket = WebSocketsServer(81);

int salidas[8];

void Log(String who, String what) {
  if(DEBUG_MODE){
    Serial.print("[");
    Serial.print(who);
    Serial.print("] ");
    Serial.println(what);
  }
}

int getEntrada(int numero) {
  switch(numero) {
    case 1: return 0;
    case 2: return 2;
    case 3: return 4;
    case 4: return 5;
    case 5: return 12;
    case 6: return 13;
    case 7: return 14;
    case 8: return 16;
  }
  return 0;
}

boolean ejecutaRespuestaWS(char* WSResponse) {
  Serial.print("[Response] '");
  Serial.print(String(WSResponse));
  Serial.println("'");

  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& json = jsonBuffer.parseObject(WSResponse);

  // Test if parsing succeeds.
  if (!json.success()) {
    Serial.println("[JSON] parse failed");
    return false;
  }

  // ================= AQUI EMPIEZA LA INTERPRETACIÓN DE LOS COMANDOS ==================

  int entrada = getEntrada(json.get<int>("check"));
  int valor = json.get<int>("valor");
  Serial.print(entrada);
  Serial.print(" > ");
  Serial.println(json.get<int>("valor"));

  
  digitalWrite(entrada, valor);

  salidas[entrada - 1] = json["valor"];
  
  String bc = "{";
  bc += "\"id\":";
  bc += json.get<int>("check");
  bc += ",\"valor\":";
  bc += valor;
  bc += "}";

  webSocket.broadcastTXT(bc);
  
  return true;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
          IPAddress ip = webSocket.remoteIP(num);
          Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  
        String ini = "";
        for(int i = 0; i < 8; i++) {
          ini += String(salidas[i]);
        }

        // Enviamos el estado actual de las salidas al nuevo cliente conectado
        webSocket.sendTXT(num, ini);
      }
      break;
    case WStype_TEXT:
      char responseWS[lenght];

      sprintf(responseWS, "%s", payload);
      ejecutaRespuestaWS(responseWS);

      // send message to client
      // webSocket.sendTXT(num, "message here");

      // Replicamos a todos los clientes
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary lenght: %u\n", num, lenght);
      hexdump(payload, lenght);

      // send message to client
      // webSocket.sendBIN(num, payload, lenght);
      break;
  }

}

/**
 * Permite identificar los tipos de contenido para enviar por el servidor WEB
 */
String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

/**
 * Devuelve la IP actual del dispositivo
 */
String IP() {
  if (WiFi.localIP().toString() == "0.0.0.0") {
    return (WiFi.softAPIP().toString());
  } else {
    return (WiFi.localIP().toString());
  }  
}

void handleIndex() {
  Log("Srv","Entrando a /");
  String path = "/index.html";

  File file = SPIFFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "Recurso no encontrado");
    return;
  }
  size_t sent = server.streamFile(file, getContentType(path));
  file.close();
}


void handleNotFound() {
  String path = server.uri();
  File file = SPIFFS.open(path, "r");
  Serial.print("[Srv] Entrando a ");
  Serial.println(path);

  if (!file) {
    Log("Srv", "Recurso no encontrado");
    server.send(404, "text/plain", "Recurso no encontrado");
    return;
  }
  //if(getContentType(path) == "application/javascript")
  {// Habilitamos la cache de los archivos del servidor
    server.sendHeader("Cache-Control", "max-age=604800");
  }
  server.setContentLength(file.size());
  size_t sent = server.streamFile(file, getContentType(path));
  file.close();
}

void setup() {
  Serial.begin(115200);
  // Preparamos las salidas
  for(int i = 1; i < 9; i ++) {
    pinMode(getEntrada(i), OUTPUT);
    digitalWrite(getEntrada(i), HIGH);
  }
  
  if (SPIFFS.begin()) {
    Log("FS", "Montado");
  } else {
    Log("FS", "Error al montar");
  }

  WiFi.softAP(ssid, password);
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
/*
 * Preparamos los eventos de la red inalambrica
 */
  static WiFiEventHandler connected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& p) {
   Log("WL", "Cliente conectado");
  });

  static WiFiEventHandler disconnected = WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected& p) {
   Log("WL", "Cliente desconectado");
  });

  static WiFiEventHandler gotIP = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& p) {
    Log("IP", IP());
  });

  static WiFiEventHandler connectedWL = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected&) {
    Log("WL", "Conectado a la red");
  });
  
  static WiFiEventHandler stDisconnected = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected&) {
    Log("WL", "Desconectado de la red");
  });
  
  static WiFiEventHandler stAuthCh = WiFi.onStationModeAuthModeChanged([](const WiFiEventStationModeAuthModeChanged&) {
    Log("WL", "Cambio el modo de autenticacion");
  });

  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleIndex);
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
    server.handleClient();
    webSocket.loop();
}
