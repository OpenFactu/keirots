/*
 * Keirost - lector vinculado ESP32 DevKit V1 y RC522.
 * Configuracion y alta de tarjetas por USB. Sin UID en los logs de diagnostico.
 * Monitor serie: 115200 baudios.
 */
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "src/DeviceConfig.h"
#include "src/PlatformClient.h"
#include "src/UsbSetup.h"
#include "src/FirmwareVersion.h"

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
bool configured = false;
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
  pinMode(0, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  Serial.setRxBufferSize(8192);
  Serial.begin(115200);
  delay(800);
  Serial.println("\nKEIROST - LECTOR VINCULADO " KEIROST_FIRMWARE_VERSION);
  Serial.println("Pines: SCK=18 MISO=19 MOSI=23 SDA/SS=21 RST=22");
  Serial.println("El destello indica tarjeta leida. Confirma el fichaje en la pantalla.");
  Serial.println("LED GPIO2: un destello por tarjeta detectada, si la placa incorpora ese LED.");
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  reader.PCD_Init();
  delay(10);
  readerReady = checkReader();
  lastHealthAt = millis();
  DeviceConfig config;
  configured = loadConfig(config);
  if (configured) startPlatform(config);
  else Serial.println("Configura el lector por USB desde Keirost > Dispositivos fisicos.");
}

void loop() {
  pollUsbSetup(configured);
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
        const bool enrolling = captureCardForUsb(reader.uid.uidByte, reader.uid.size);
        if (!enrolling && configured && !submitCard(reader.uid.uidByte, reader.uid.size)) {
          Serial.println("Lectura local sin enviar: lector ocupado o sin conexion. No se guarda para despues.");
        }
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
      Serial.printf("Tarjetas detectadas: %lu. Estado: %s.\n",
                    static_cast<unsigned long>(cardCount), platformState());
    }
  }
  delay(100);
}
