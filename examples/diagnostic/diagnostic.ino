/*
 * Keirost - prueba fisica del ESP32 DevKit V1 y RC522.
 * No realiza fichajes, no usa Wi-Fi y no imprime UID ni credenciales.
 * Monitor serie: 115200 baudios.
 */
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t PIN_SCK = 18;
constexpr uint8_t PIN_MISO = 19;
constexpr uint8_t PIN_MOSI = 23;
constexpr uint8_t PIN_SS = 21;  // SDA/SS del RC522.
constexpr uint8_t PIN_RST = 22;
constexpr uint8_t PIN_LED = LED_BUILTIN;  // GPIO2 en DOIT ESP32 DEVKIT V1.
constexpr uint32_t LED_PULSE_MS = 250;
constexpr uint32_t REMOVAL_MS = 500;

MFRC522 reader(PIN_SS, PIN_RST);
bool readerReady = false;
bool cardPresent = false;
bool ledOn = false;
uint32_t ledStartedAt = 0;
uint32_t lastSeenAt = 0;
uint32_t lastHealthAt = 0;
uint32_t lastHeartbeatAt = 0;
uint32_t cardCount = 0;

bool checkReader() {
  const uint8_t version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("ERROR: RC522 sin respuesta. Desconecta el USB y revisa alimentacion y cables.");
    return false;
  }
  // An unknown response is diagnostic evidence, not proof of a healthy reader.
  if (version != 0x90 && version != 0x91 && version != 0x92 && version != 0x88) {
    Serial.printf("AVISO: version RC522 no reconocida (0x%02X). Pendiente de probar tarjeta.\n", version);
  } else {
    Serial.printf("RC522 responde (version 0x%02X).\n", version);
  }
  Serial.println("Acerca una tarjeta al lector. No se mostrara su UID.");
  return true;
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  Serial.begin(115200);
  delay(800);
  Serial.println("\nKEIROST - DIAGNOSTICO RFID 0.1.1");
  Serial.println("Pines: SCK=18 MISO=19 MOSI=23 SDA/SS=21 RST=22");
  Serial.println("Solo prueba de lectura: no registra fichajes.");
  Serial.println("LED GPIO2: un destello por tarjeta detectada, si la placa incorpora ese LED.");
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  reader.PCD_Init();
  delay(10);
  readerReady = checkReader();
  lastHealthAt = millis();
}

void loop() {
  const uint32_t now = millis();
  if (ledOn && now - ledStartedAt >= LED_PULSE_MS) {
    digitalWrite(PIN_LED, LOW);
    ledOn = false;
  }
  if (!readerReady) {
    if (now - lastHealthAt >= 3000) {
      lastHealthAt = now;
      reader.PCD_Init();
      delay(10);
      readerReady = checkReader();
    }
    delay(50);
    return;
  }

  // Wake halted cards as well: IsNewCardPresent alone would mistake a halted
  // card for a removed card and could count the same presentation repeatedly.
  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  const MFRC522::StatusCode status = reader.PICC_WakeupA(atqa, &atqaSize);
  if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
    lastSeenAt = now;
    if (reader.PICC_ReadCardSerial()) {
      if (!cardPresent) {
        cardPresent = true;
        ++cardCount;
        digitalWrite(PIN_LED, HIGH);
        ledStartedAt = millis();
        ledOn = true;
        Serial.printf("TARJETA DETECTADA #%lu. Retirala antes de volver a acercarla.\n",
                      static_cast<unsigned long>(cardCount));
      }
      reader.PICC_HaltA();
      reader.PCD_StopCrypto1();
    }
  } else if (status == MFRC522::STATUS_TIMEOUT && cardPresent && now - lastSeenAt >= REMOVAL_MS) {
    cardPresent = false;
    Serial.println("Tarjeta retirada. Listo para otra lectura.");
  }

  if (now - lastHeartbeatAt >= 10000) {
    lastHeartbeatAt = now;
    const uint8_t version = reader.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
      readerReady = false;
      cardPresent = false;
      Serial.println("ERROR: se ha perdido la comunicacion con el RC522.");
    } else {
      Serial.printf("Diagnostico activo. Tarjetas detectadas: %lu.\n",
                    static_cast<unsigned long>(cardCount));
    }
  }
  delay(100);
}
