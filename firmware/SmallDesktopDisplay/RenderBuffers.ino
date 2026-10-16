// The clock animation and HTTP pages never run together. Reuse their largest
// allocation, retaining it across page changes to avoid heap fragmentation.
uint8_t* runtimeWorkspace = nullptr;
bool runtimeWorkspaceAttempted = false;
uint8_t* Runtime_workspace() {
  if(!runtimeWorkspaceAttempted) {
    runtimeWorkspaceAttempted = true;
    runtimeWorkspace = (uint8_t*)malloc(16386);
  }
  return runtimeWorkspace;
}
TFT_eSprite valueStrip(&tft);
bool valueStripReady = false;
void Render_init() {
  Runtime_workspace();
  valueStrip.setColorDepth(8);
  valueStripReady = valueStrip.createSprite(130, 28) != nullptr;
}

void Render_valueLine(const String &text, int x, int y, int height, uint8_t font, uint16_t fg, uint16_t bg) {
  String fitted = Text_fit(text.c_str(), 122, font);
  if(valueStripReady) {
    valueStrip.fillSprite(bg);
    valueStrip.setTextSize(1); valueStrip.setTextWrap(false, false);
    valueStrip.setTextDatum(MR_DATUM); valueStrip.setTextColor(fg, bg);
    valueStrip.drawString(fitted, 122, height / 2, font);
    valueStrip.pushSprite(x, y, 0, 0, 130, height);
  }
  else {
    tft.fillRect(x, y, 130, height, bg);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(fg, bg);
    tft.drawString(fitted, x + 122, y + height / 2, font);
  }
}

void Render_clearCorners(int x, int y, int width, int height, uint16_t color) {
  tft.fillRect(x, y, 4, 4, color); tft.fillRect(x + width - 4, y, 4, 4, color);
  tft.fillRect(x, y + height - 4, 4, 4, color); tft.fillRect(x + width - 4, y + height - 4, 4, 4, color);
}
