void Perf_frame(uint32_t started) {
  uint32_t elapsed=micros()-started;
  perf.frames++; perf.decodeUs+=elapsed;
  if(elapsed>perf.decodeMaxUs) perf.decodeMaxUs=elapsed;
  if(perf.lastFrameUs && started-perf.lastFrameUs>perf.frameMaxUs) perf.frameMaxUs=started-perf.lastFrameUs;
  perf.lastFrameUs=started;
}
void Perf_report() {
  static uint32_t last=0;
  if(millis()-last<10000) return;
  last=millis();
  Serial.printf("PERF ms=%lu frames=%lu decode_avg_us=%lu decode_max_us=%lu frame_max_us=%lu loop_max_us=%lu page_max_us=%lu first_max_us=%lu heap=%u block=%u\n",
    millis(),perf.frames,(uint32_t)(perf.frames?perf.decodeUs/perf.frames:0),perf.decodeMaxUs,perf.frameMaxUs,perf.loopMaxUs,perf.pageMaxUs,perf.firstMaxUs,ESP.getFreeHeap(),ESP.getMaxFreeBlockSize());
}

void Perf_send() {
  char buffer[640];
  snprintf(buffer, sizeof(buffer),
    "{\"uptime_ms\":%lu,\"page\":%u,\"frames\":%lu,\"decode_avg_us\":%lu,\"frame_max_us\":%lu,\"loop_max_us\":%lu,\"page_max_us\":%lu,\"first_max_us\":%lu,\"heap\":%u,\"block\":%u,\"net_owner\":%u,\"net_ok\":[%lu,%lu,%lu,%lu],\"net_fail\":[%lu,%lu,%lu,%lu],\"canceled\":%lu,\"stock_valid\":%u,\"weather_valid\":%u,\"codex_valid\":%u}",
    millis(),currentPage,perf.frames,(uint32_t)(perf.frames?perf.decodeUs/perf.frames:0),perf.frameMaxUs,
    perf.loopMaxUs,perf.pageMaxUs,perf.firstMaxUs,ESP.getFreeHeap(),ESP.getMaxFreeBlockSize(),Network_owner(),
    perf.netOk[0],perf.netOk[1],perf.netOk[2],perf.netOk[3],perf.netFail[0],perf.netFail[1],perf.netFail[2],perf.netFail[3],perf.canceled,
    Stock_validCount(),Weather_valid(),Codex_valid());
  server.send(200, "application/json", buffer);
}
