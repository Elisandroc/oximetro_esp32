#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

// === OLED SPI CONFIG ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_CS     5
#define OLED_DC     4
#define OLED_RESET  16

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// === SENSOR CONFIG ===
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
int rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

int32_t beatsPerMinute;
int beatAvg;
int32_t spo2;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("Erro no display OLED"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println(F("Iniciando sensor..."));
  display.display();

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(F("Sensor nao encontrado"));
    display.display();
    while (true);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x3F);
  particleSensor.setPulseAmplitudeIR(0x3F);
}

void loop() {
  const uint8_t bufferLength = 100;
  uint32_t irBuffer[bufferLength];
  uint32_t redBuffer[bufferLength];
  bool dedoDetectado = false;

  // Coleta 100 amostras (~4s)
  for (int i = 0; i < bufferLength; i++) {
    while (!particleSensor.available()) {
      particleSensor.check();
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // Detecta presença do dedo com base no IR
  for (int i = 0; i < bufferLength; i++) {
    // Ajuste esse limiar se necessário, dependendo do ambiente e do sensor
    if (irBuffer[i] > 30000) {
      dedoDetectado = true;
      break;
    }
  }

  // Se o dedo estiver detectado, tenta calcular os parâmetros
  if (dedoDetectado) {
    int8_t spo2Valid, hrValid;
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer,
      bufferLength,
      redBuffer,
      &spo2,
      &spo2Valid,
      &beatsPerMinute,
      &hrValid
    );
    
    // Se os dados forem válidos (hrValid = 1, beatsPerMinute não for -999, e dentro do intervalo esperado)
    if (hrValid && beatsPerMinute != -999 && beatsPerMinute > 40 && beatsPerMinute < 120) {
      rates[rateSpot++] = beatsPerMinute;
      rateSpot %= RATE_SIZE;

      int soma = 0;
      int cont = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        if (rates[x] > 40 && rates[x] < 120) {
          soma += rates[x];
          cont++;
        }
      }
      if (cont > 0) {
        beatAvg = soma / cont;
        Serial.print("Contador válido: ");
Serial.println(cont);
      }
    }
    // Se os dados não são válidos, não atualizamos; mantemos o último beatAvg.
  } else {
    spo2 = 0;
    beatAvg = 0;
  }

  // Atualiza o display
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("BPM: ");
if (beatAvg > 0)
  display.println(beatAvg);
else
  display.println("--");

  display.setCursor(0, 30);
  display.print("SpO2: ");
  // Exibimos o SpO2 somente se for um valor plausível
if (spo2 > 0 && spo2 <= 100)
  display.print(spo2);
else
  display.print("--");
  display.println("%");

  display.display();

  // Debug no Serial Monitor
  Serial.print("Dedo: ");
  Serial.print(dedoDetectado ? "Sim" : "Nao");
  Serial.print(" | Raw BPM: ");
  Serial.print(beatsPerMinute);
  Serial.print(" | Media: ");
  Serial.print(beatAvg);
  Serial.print(" | SpO2: ");
  Serial.println(spo2);

  delay(1000);
}
