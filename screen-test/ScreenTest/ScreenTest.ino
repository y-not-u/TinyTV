#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

static const int BACKLIGHT_PIN = 5; // GPIO5 / D1 on ESP8266

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("TinyTV screen test start");

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWriteRange(1023);

  digitalWrite(BACKLIGHT_PIN, HIGH);
  delay(500);
  digitalWrite(BACKLIGHT_PIN, LOW);
  delay(500);
  analogWrite(BACKLIGHT_PIN, 512);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_RED);
  Serial.println("TFT red");
}

void loop() {
  static uint8_t state = 0;
  static const uint16_t colors[] = {
    TFT_RED,
    TFT_GREEN,
    TFT_BLUE,
    TFT_WHITE,
    TFT_BLACK,
  };
  static const char *names[] = {
    "red",
    "green",
    "blue",
    "white",
    "black",
  };

  analogWrite(BACKLIGHT_PIN, state == 4 ? 1023 : 512);
  tft.fillScreen(colors[state]);
  Serial.print("TFT ");
  Serial.println(names[state]);

  state = (state + 1) % 5;
  delay(1000);
}
