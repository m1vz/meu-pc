// ESP32 PC Remote — Fase 4: Wi-Fi + Wake-on-LAN + MQTT (comando LIGAR por MQTT, Serial ou botão BOOT)

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "ca_cert.h"

const unsigned long WIFI_RETRY_MS = 10000;  // intervalo entre tentativas manuais de reconexão

// MQTT: comando recebido em TOPIC_COMANDO; TOPIC_STATUS é retido ("online"/"offline" via Last Will);
// TOPIC_RESPOSTA confirma cada comando recebido por MQTT
const char* TOPIC_COMANDO = "meu-pc/comando";
const char* TOPIC_STATUS = "meu-pc/status";
const char* TOPIC_RESPOSTA = "meu-pc/resposta";
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long LIGAR_COOLDOWN_MS = 5000;  // ignora LIGAR repetido em sequência (ex.: toque duplo no app)

// MAC da placa de rede do PC (B0-82-E2-4B-C2-9A)
const uint8_t PC_MAC[6] = {0xB0, 0x82, 0xE2, 0x4B, 0xC2, 0x9A};
const uint16_t WOL_PORT = 9;
const int WOL_REPEAT = 3;  // UDP não tem confirmação; repetir aumenta a chance de chegar

// Botão BOOT (GPIO0, ativo em LOW) como atalho para LIGAR; LED azul (GPIO2) dá o feedback
const int BUTTON_PIN = 0;
const int LED_PIN = 2;
const unsigned long DEBOUNCE_MS = 50;

unsigned long lastWifiAttempt = 0;
WiFiUDP udp;
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastButtonChange = 0;
String serialLine;
WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);
String mqttClientId;
unsigned long lastMqttAttempt = 0;
unsigned long lastLigar = 0;
bool hasLigado = false;

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] Conectado! IP: ");
      Serial.print(WiFi.localIP());
      Serial.print("  RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.print("[WiFi] Desconectado (motivo ");
      Serial.print(info.wifi_sta_disconnected.reason);
      Serial.println("), tentando reconectar...");
      break;
    default:
      break;
  }
}

void connectWifi() {
  Serial.print("[WiFi] Conectando em \"");
  Serial.print(WIFI_SSID);
  Serial.println("\"...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
}

// O driver já tenta reconectar sozinho (setAutoReconnect); isto é uma garantia extra
// caso ele desista (ex.: roteador ficou fora do ar por muito tempo).
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttempt < WIFI_RETRY_MS) return;
  Serial.println("[WiFi] Ainda sem conexão, reiniciando tentativa...");
  WiFi.disconnect();
  connectWifi();
}

// Magic Packet: 6 bytes 0xFF + MAC repetido 16 vezes (102 bytes)
bool sendMagicPacket() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WoL] ERRO: sem Wi-Fi, pacote não enviado");
    return false;
  }

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; i++) memcpy(packet + 6 + i * 6, PC_MAC, 6);

  // Broadcast da sub-rede (ex.: 192.168.1.255) e broadcast global
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress subnetBroadcast(ip[0] | ~mask[0], ip[1] | ~mask[1], ip[2] | ~mask[2], ip[3] | ~mask[3]);
  IPAddress targets[] = {subnetBroadcast, IPAddress(255, 255, 255, 255)};

  bool ok = true;
  for (int r = 0; r < WOL_REPEAT; r++) {
    for (IPAddress& target : targets) {
      udp.beginPacket(target, WOL_PORT);
      udp.write(packet, sizeof(packet));
      if (!udp.endPacket()) ok = false;
    }
    delay(50);
  }

  Serial.print("[WoL] Magic Packet enviado para B0:82:E2:4B:C2:9A via ");
  Serial.print(subnetBroadcast);
  Serial.print(" e 255.255.255.255, porta ");
  Serial.print(WOL_PORT);
  Serial.println(ok ? " (OK)" : " (houve falha em algum envio)");
  return ok;
}

void blinkLed(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

// LED: 3 piscadas rápidas = pacote enviado; 1 piscada longa = falhou (ex.: sem Wi-Fi)
// Retorna o resultado para ser repassado em TOPIC_RESPOSTA
const char* ligarPc() {
  if (hasLigado && millis() - lastLigar < LIGAR_COOLDOWN_MS) {
    Serial.println("[WoL] LIGAR repetido em menos de 5 s, ignorado");
    return "ignorado";
  }
  hasLigado = true;
  lastLigar = millis();
  if (sendMagicPacket()) {
    blinkLed(3, 100);
    return "wol_enviado";
  }
  blinkLed(1, 1000);
  return "falha";
}

void publishResposta(const char* comando, const char* resultado) {
  String msg = String("{\"comando\":\"") + comando + "\",\"resultado\":\"" + resultado +
               "\",\"uptime\":" + (millis() / 1000) + "}";
  mqtt.publish(TOPIC_RESPOSTA, msg.c_str());
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String cmd;
  for (unsigned int i = 0; i < length && i < 64; i++) cmd += (char)payload[i];
  cmd.trim();
  cmd.toUpperCase();
  Serial.println("[MQTT] Recebido em " + String(topic) + ": " + cmd);

  if (cmd == "LIGAR") {
    publishResposta("LIGAR", ligarPc());
  } else if (cmd == "STATUS") {
    publishResposta("STATUS", "online");
  } else {
    publishResposta(cmd.c_str(), "desconhecido");
  }
}

void setupMqtt() {
  tlsClient.setCACert(CA_CERT);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
  mqttClientId = "esp32-pc-remote-" + String((uint32_t)ESP.getEfuseMac(), HEX);
}

// Conecta ao broker com Last Will: se o ESP32 cair, o broker publica "offline" (retido) sozinho
void ensureMqtt() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (lastMqttAttempt != 0 && millis() - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = millis();

  Serial.print("[MQTT] Conectando em ");
  Serial.print(MQTT_HOST);
  Serial.println("...");
  if (mqtt.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD, TOPIC_STATUS, 1, true, "offline")) {
    Serial.println("[MQTT] Conectado!");
    mqtt.publish(TOPIC_STATUS, "online", true);
    mqtt.subscribe(TOPIC_COMANDO, 1);
  } else {
    // -2 = falha de rede/TLS (host, porta ou certificado); 4/5 = usuário/senha recusados
    Serial.print("[MQTT] Falhou, estado ");
    Serial.print(mqtt.state());
    Serial.println(", nova tentativa em 5 s");
  }
}

void readButton() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) lastButtonChange = millis();
  lastButtonReading = reading;

  if (millis() - lastButtonChange > DEBOUNCE_MS && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) {
      Serial.println("[Botão] BOOT pressionado -> LIGAR");
      ligarPc();
    }
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.isEmpty()) return;

  if (cmd == "LIGAR") {
    ligarPc();
  } else if (cmd == "STATUS") {
    Serial.print("[Status] Wi-Fi: ");
    Serial.print(WiFi.status() == WL_CONNECTED ? "conectado, IP " + WiFi.localIP().toString() : "desconectado");
    Serial.print(", MQTT: ");
    Serial.print(mqtt.connected() ? "conectado" : "desconectado (estado " + String(mqtt.state()) + ")");
    Serial.print(", uptime ");
    Serial.print(millis() / 1000);
    Serial.println(" s");
  } else {
    Serial.println("[Cmd] Comando desconhecido: " + cmd + " (use LIGAR ou STATUS)");
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 64) {
      serialLine += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== ESP32 PC Remote ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-pc-remote");
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // menor latência para receber comandos
  WiFi.onEvent(onWifiEvent);
  connectWifi();
  setupMqtt();
  Serial.println("Comandos: LIGAR (envia Wake-on-LAN), STATUS. Botao BOOT = LIGAR");
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqtt.loop();
  readSerial();
  readButton();
  digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);  // LED aceso = Wi-Fi conectado
  delay(10);
}
