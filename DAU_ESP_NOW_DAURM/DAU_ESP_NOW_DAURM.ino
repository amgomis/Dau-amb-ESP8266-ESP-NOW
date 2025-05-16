#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Adafruit_NeoPixel.h>

// Definició dels pins i configuració dels LEDs
#define LED_PIN     2  // Pin on es connectaran els LEDs WS2812
#define NUM_LEDS    7   // Utilitzem 7 LEDs (1 central + 6 pels valors del dau)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Estructura del missatge que rebrem
typedef struct message {
  uint8_t mac[6];  // MAC del dispositiu emissor
} message_t;

message_t incomingMsg;

bool ran_dom = false;

// Configuració de les MACs i colors dels emissors (polsadors)
// Màxim 5 dispositius emissors
// IMPORTANT: Hauràs de canviar aquestes MACs pels valors reals dels teus dispositius polsadors
// Pots obtenir la MAC de cada dispositiu des del monitor sèrie quan executis el seu codi
#define MAX_DEVICES 5
uint8_t registeredMACs[MAX_DEVICES][6] = {
  {0x18, 0xFE, 0x34, 0xD4, 0xB1, 0xCC}, // MAC del polsador 1
  {0x5C, 0xCF, 0x7F, 0x80, 0x3D, 0xBA}, // MAC del polsador 2
  {0x5C, 0xCF, 0x7F, 0x80, 0xCB, 0xC2}, // MAC del polsador 3
  {0x5C, 0xCF, 0x7F, 0x81, 0x11, 0x29}, // MAC del polsador 4
  {0xBC, 0xFF, 0x4D, 0x19, 0x67, 0x05}  // MAC del polsador 5
};

// Colors pels diferents dispositius (format: R, G, B)
uint32_t deviceColors[MAX_DEVICES] = {
  strip.Color(255, 0, 0),      // Vermell
  strip.Color(0, 255, 0),      // Verd
  strip.Color(0, 0, 255),      // Blau
  strip.Color(255, 255, 0),    // Groc
  strip.Color(255, 0, 255)     // Magenta
};

// Posicions dels LEDs per cada valor del dau (1-6)
// Assumim que tenim 7 LEDs, amb el LED 0 al centre i LEDs 1-6 al voltant
const uint8_t dicePatterns[6][7] = {
  {0, 0, 0, 0, 0, 0, 1},  // 1: només el LED central
  {1, 0, 0, 1, 0, 0, 0},  // 2: dos LEDs en diagonal
  {1, 0, 0, 1, 0, 0, 1},  // 3: tres LEDs en diagonal
  {1, 0, 1, 1, 0, 1, 0},  // 4: quatre LEDs en les cantonades
  {1, 0, 1, 1, 0, 1, 1},  // 5: quatre LEDs en les cantonades i un al centre
  {1, 1, 1, 1, 1, 1, 0}   // 6: tots els LEDs excepte el central
};

// Funció per comprovar si una MAC està registrada
int findMACIndex(const uint8_t *mac) {
  for (int i = 0; i < MAX_DEVICES; i++) {
    bool match = true;
    for (int j = 0; j < 6; j++) {
      if (mac[j] != registeredMACs[i][j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }
  return -1; // No s'ha trobat
}

// Funció per mostrar un valor del dau amb el color corresponent
void showDiceValue(int value, uint32_t color) {
  // Assegurem-nos que el valor està dins del rang 1-6
  if (value < 1) value = 1;
  if (value > 6) value = 6;
  
  // Índex del patró (0-5 per valors 1-6)
  int patternIndex = value - 1;
  
  // Netegem tots els LEDs
  strip.clear();
  
  // Activem els LEDs segons el patró
  for (int i = 0; i < 7; i++) {
    if (dicePatterns[patternIndex][i]) {
      strip.setPixelColor(i, color);
    }
  }
  
  strip.show();
}

// Funció que s'executa quan rebem un paquet per ESP-NOW
void onDataReceived(uint8_t *mac, uint8_t *data, uint8_t len) {
  // Comprovem que la longitud sigui correcta
  if (len == sizeof(message_t)) {
    // Copiem el contingut a la nostra estructura
    memcpy(&incomingMsg, data, sizeof(message_t));
    
    // Mostrem la MAC rebuda per debugging i ajudar en la configuració
    Serial.print("MAC rebuda: ");
    for (int i = 0; i < 6; i++) {
      Serial.print("0x");
      if (incomingMsg.mac[i] < 0x10) Serial.print("0");
      Serial.print(incomingMsg.mac[i], HEX);
      if (i < 5) Serial.print(", ");
    }
    Serial.println();
    if (ran_dom == false) {randomSeed(millis()); ran_dom = true;}
    // Busquem l'índex del dispositiu a la nostra llista
    int deviceIndex = findMACIndex(incomingMsg.mac);
    
    if (deviceIndex >= 0) {
      // Generem un valor aleatori entre 1 i 6
      int diceValue = random(1, 7);
      
      // Mostrem el valor amb el color corresponent al dispositiu
      showDiceValue(diceValue, deviceColors[deviceIndex]);
      
      Serial.print("Rebut del dispositiu #");
      Serial.print(deviceIndex + 1);
      Serial.print(": Valor dau = ");
      Serial.println(diceValue);
    } else {
      Serial.println("Dispositiu no registrat! Afegeix aquesta MAC a la llista registeredMACs.");
    }
  }
}

void setup() {
  // Iniciem la comunicació sèrie per debugging
  Serial.begin(115200);
  Serial.println("Estació del dau inicialitzada");
  
  // Inicialitzem els LEDs
  strip.begin();
  strip.clear();
  strip.show();
  strip.setBrightness(128);

  // Configurem el WiFi en mode estació
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Inicialitzem la comunicació ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error a l'inicialitzar ESP-NOW");
    return;
  }
  
  // Definim el rol com a receptor
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  
  // Registrem la funció de callback per rebre dades
  esp_now_register_recv_cb(onDataReceived);
  
  // Mostrem la nostra MAC
  Serial.print("MAC de l'estació: ");
  Serial.println(WiFi.macAddress());
  
  // Inicialitzem el generador de nombres aleatoris
  randomSeed(analogRead(A0));
  
  // Mostrem una seqüència d'inici per verificar que els LEDs funcionen
  for (int i = 0; i < 7; i++) {
    strip.clear();
    strip.setPixelColor(i, strip.Color(255, 255, 255));
    strip.show();
    delay(200);
  }
  strip.clear();
  strip.show();
}

void loop() {
  // No fem res al loop principal, tot es gestiona amb callbacks
  delay(10);
}
