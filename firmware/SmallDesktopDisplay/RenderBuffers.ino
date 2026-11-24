// Reuse the HTTP workspace across page changes to avoid heap fragmentation.
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
  // Palette entries keep the panel and text colors identical to direct TFT drawing.
  valueStrip.setColorDepth(4);
  if(valueStrip.createSprite(130, 28)) {
    valueStrip.setPaletteColor(0, TinyTV::Theme::panel);
    valueStripReady = valueStrip.getPaletteColor(0) == TinyTV::Theme::panel;
    if(!valueStripReady) valueStrip.deleteSprite();
  }
}

void Render_valueLine(const String &text, int x, int y, int height, uint8_t font, uint16_t fg, uint16_t bg) {
  String fitted = Text_fit(text.c_str(), 122, font);
  if(valueStripReady) {
    valueStrip.setPaletteColor(0, bg);
    valueStrip.setPaletteColor(1, fg);
    valueStrip.fillSprite(0);
    valueStrip.setTextSize(1); valueStrip.setTextWrap(false, false);
    valueStrip.setTextDatum(MR_DATUM); valueStrip.setTextColor(1, 0);
    valueStrip.drawString(fitted, 122, height / 2, font);
    valueStrip.pushSprite(x, y, 0, 0, 130, height);
  }
  else {
    tft.fillRect(x, y, 130, height, bg);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(fg, bg);
    tft.drawString(fitted, x + 122, y + height / 2, font);
  }
}
