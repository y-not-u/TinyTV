TinyTV::RequestVersion ntpVersion;
uint8_t ntpState = 0, ntpServerIndex = 0;
bool ntpResolved = false, ntpFailed = false;
ip_addr_t ntpAddress;
IPAddress ntpExpectedAddress;
uint32_t ntpStarted = 0, ntpLastAttempt = 0, ntpInterval = 0;
uint8_t ntpNonce[8] = {};
void Ntp_dns(const char*, const ip_addr_t* address, void* token) {
  if(ntpState != 1 || !ntpVersion.accepts((uintptr_t)token)) return;
  if(address) { ntpAddress = *address; ntpResolved = true; }
  else ntpFailed = true;
}
void Ntp_request() {
  ntpVersion.next(); ntpState = 0; ntpInterval = 0; ntpServerIndex = 0;
}
void Ntp_poll() {
  uint32_t nowMs = millis();
  if(WiFi.status() != WL_CONNECTED) return;
  if(ntpState == 0) {
    if(ntpInterval && nowMs - ntpLastAttempt < ntpInterval) return;
    ntpLastAttempt = ntpStarted = nowMs;
    ntpResolved = ntpFailed = false;
    ntpState = 1;
    uint32_t token = ntpVersion.next();
    err_t result = dns_gethostbyname(ntpServerNames[ntpServerIndex], &ntpAddress, Ntp_dns, (void*)(uintptr_t)token);
    if(result == ERR_OK) ntpResolved = true;
    else if(result != ERR_INPROGRESS) ntpFailed = true;
  }
  if(ntpState == 1 && ntpResolved && !ntpFailed) {
    ntpExpectedAddress = IPAddress(ip4_addr_get_u32(ip_2_ip4(&ntpAddress)));
    uint8_t packet[48] = {};
    packet[0] = 0x23; // NTPv4 client
    uint32_t token = ntpVersion.value, stamp = micros();
    memcpy(ntpNonce, &token, 4); memcpy(ntpNonce + 4, &stamp, 4);
    memcpy(packet + 40, ntpNonce, 8);
    // Flush at most one old packet per poll, keeping loop work bounded.
    if(Udp.parsePacket()) { Udp.flush(); return; }
    if(!Udp.beginPacket(ntpExpectedAddress, 123)) ntpFailed = true;
    else {
      Udp.write(packet, sizeof(packet));
      if(!Udp.endPacket()) ntpFailed = true;
      else { ntpState = 2; ntpStarted = nowMs; }
    }
  }
  if(ntpState == 2) {
    int size = Udp.parsePacket();
    if(size > 0) {
      uint8_t packet[48];
      bool source = Udp.remoteIP() == ntpExpectedAddress && Udp.remotePort() == 123;
      int count = Udp.read(packet, sizeof(packet)); Udp.flush();
      if(source && count == 48 && (packet[0] & 7) == 4 &&
        (packet[0] >> 6) != 3 && packet[1] > 0 && packet[1] < 16 &&
        memcmp(packet + 24, ntpNonce, 8) == 0) {
        uint32_t seconds = (uint32_t)packet[40] << 24 | (uint32_t)packet[41] << 16 |
          (uint32_t)packet[42] << 8 | packet[43];
        if(seconds >= 2208988800UL) {
          setTime(seconds - 2208988800UL + timeZone * SECS_PER_HOUR);
          ntpState = 0; ntpServerIndex = 0; ntpInterval = 300000;
          Serial.println(F("NTP synced"));
          return;
        }
      }
    }
  }
  if(ntpFailed || nowMs - ntpStarted >= (ntpState == 1 ? 3000UL : 1500UL)) {
    ntpVersion.next(); ntpState = 0;
    ntpServerIndex = (ntpServerIndex + 1) % (sizeof(ntpServerNames) / sizeof(ntpServerNames[0]));
    ntpInterval = ntpServerIndex ? 0 : 60000;
  }
}
