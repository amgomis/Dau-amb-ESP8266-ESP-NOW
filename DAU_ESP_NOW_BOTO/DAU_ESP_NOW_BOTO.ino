#include <ESP8266WiFi.h>
#include <espnow.h>

// Adreça MAC del dispositiu estació (receptor)
// IMPORTANT: Has de substituir això amb la MAC real del teu dispositiu estació
// Pots obtenir aquesta MAC des del monitor sèrie quan executis el codi de l'estació
uint8_t receiverMac[] = {0x18, 0xFE, 0x34, 0xFD, 0xFF, 0xD5}; // Canviar per la MAC real del receptor

// Estructura del missatge a enviar
typedef struct message {
  uint8_t mac[6];  // MAC del dispositiu emissor
} message_t;

message_t outgoingMsg;

// Variable per controlar si ja hem enviat el missatge
bool messageSent = false;

// Temps per a deep sleep (en microsegons)
// 0 = cap deep sleep, només apaga el WiFi
// 1000000 = 1 segon
const uint64_t SLEEP_TIME = 0; 

// Funció de callback que s'executa quan enviem dades
void onDataSent(uint8_t *mac_addr, uint8_t status) {
  Serial.print("Estat d'enviament: ");
  if (status == 0) {
    Serial.println("Èxit");
  } else {
    Serial.println("Error");
  }
  
  // Un cop enviat el missatge, esperem una mica i entrem en mode baix consum
  delay(100);
  
  // Apaguem el WiFi per estalviar energia
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  
  if (SLEEP_TIME > 0) {
    Serial.println("Entrant en deep sleep...");
    ESP.deepSleep(SLEEP_TIME); 
  } else {
    Serial.println("WiFi apagat. El dispositiu segueix funcionant amb consum reduït.");
    Serial.println("Per enviar un nou missatge, reinicia el dispositiu.");
  }
}

void setup() {
  // Iniciem la comunicació sèrie per debugging
  Serial.begin(115200);
  Serial.println("Dispositiu polsador inicialitzat");
  Serial.println("Un cop s'encén, envia la MAC i després s'apaga");
  
  // Configurem el WiFi en mode estació
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Inicialitzem la comunicació ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error a l'inicialitzar ESP-NOW");
    return;
  }
  
  // Definim el rol com a emissor
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  
  // Registrem la funció de callback d'enviament
  esp_now_register_send_cb(onDataSent);
  
  // Afegim el dispositiu receptor
  esp_now_add_peer(receiverMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  
  // Mostrem la nostra MAC i la guardem per enviar-la
  String macStr = WiFi.macAddress();
  Serial.print("MAC del dispositiu polsador: ");
  Serial.println(macStr);
  
  // Mostrem la MAC en format hexadecimal per facilitar la configuració
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.print("Format per copiar a registeredMACs: {");
  for (int i = 0; i < 6; i++) {
    Serial.print("0x");
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(", ");
  }
  Serial.println("}");
  
  // Guardem la nostra MAC a l'estructura del missatge
  memcpy(outgoingMsg.mac, mac, sizeof(mac));
  
  // Enviem la nostra MAC al dispositiu receptor immediatament
  Serial.println("Enviant MAC...");
  esp_now_send(receiverMac, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));
  messageSent = true;
  
  // No fem res més al setup - la resta es gestiona amb el callback d'enviament
}

void loop() {
  // El loop no fa res, només espera que es completi la funció onDataSent
  // Si per alguna raó el callback no s'executa, apaguem el WiFi després d'un temps
  static unsigned long startTime = millis();
  
  if (messageSent && (millis() - startTime > 5000)) {
    Serial.println("Temps d'espera esgotat. Apagant WiFi...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    
    if (SLEEP_TIME > 0) {
      Serial.println("Entrant en deep sleep...");
      ESP.deepSleep(SLEEP_TIME);
    } else {
      Serial.println("WiFi apagat. El dispositiu segueix funcionant amb consum reduït.");
      startTime = millis(); // Evitem que aquest missatge es repeteixi
    }
  }
  
  delay(10);
}
