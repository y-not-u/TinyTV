#if DHT_EN
void IndoorTem() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  String temperatureText = isfinite(temperature) ? String(temperature, 1) + " C" : "-- C";
  String humidityText = isfinite(humidity) ? String(humidity, 1) + " %" : "-- %";
  tft.setTextSize(1); tft.setTextWrap(false, false);
  tft.setTextDatum(CC_DATUM); tft.setTextColor(TinyTV::Theme::muted, bgColor);
  tft.drawString("IN  " + temperatureText + "   " + humidityText, 120, 220, 1);
}
#endif
