#if imgAst_EN
TinyTV::FrameSchedule animationSchedule;
uint16_t* animationPixels = nullptr;
bool Animation_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* pixels) {
  if(x < 0 || y < 0 || x + w > 70 || y + h > 70) return false;
  for(uint16_t row = 0; row < h; ++row)
    memcpy(animationPixels + (y + row) * 70 + x, pixels + row * w, w * sizeof(uint16_t));
  return true;
}
void Animation_reset() { animationSchedule.reset(); perf.lastFrameUs = 0; }
void imgAnim() {
  if(currentPage != PAGE_CLOCK || !animationSchedule.advance(millis())) return;
  uint32_t started = micros();
  const uint8_t* frames[] = {i0, i1, i2, i3, i4, i5, i6, i7, i8, i9};
  const uint32_t sizes[] = {sizeof(i0), sizeof(i1), sizeof(i2), sizeof(i3), sizeof(i4), sizeof(i5), sizeof(i6), sizeof(i7), sizeof(i8), sizeof(i9)};
  Anim = animationSchedule.frame;
  AprevTime = animationSchedule.last;
  animationPixels = reinterpret_cast<uint16_t*>(Runtime_workspace());
  if(animationPixels) {
    TJpgDec.setCallback(Animation_output);
    if(TJpgDec.drawJpg(0, 0, frames[Anim], sizes[Anim]) == JDR_OK)
      tft.pushImage(160, 160, 70, 70, animationPixels);
    TJpgDec.setCallback(tft_output);
  }
  else TJpgDec.drawJpg(160, 160, frames[Anim], sizes[Anim]);
  Perf_frame(started);
}
#endif
