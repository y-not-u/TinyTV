#if DHT_EN
void IndoorTem() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  tft.fillRect(160, 150, 80, 90, bgColor);
  tft.setTextSize(1); tft.setTextWrap(false, false);
  tft.setTextDatum(CC_DATUM); tft.setTextColor(TFT_WHITE, bgColor);
  tft.drawString("IN", 201, 166, 1);
  tft.drawString(isfinite(temperature) ? String(temperature, 1) + " C" : "-- C", 200, 197, 1);
  tft.drawString(isfinite(humidity) ? String(humidity, 1) + " %" : "-- %", 200, 227, 1);
}
#endif
