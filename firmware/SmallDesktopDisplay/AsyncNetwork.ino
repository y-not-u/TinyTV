// lwIP callbacks only retain packets / signal state. Work happens in Network_poll.
const uint8_t NET_STOCK = 1, NET_WEATHER = 2, NET_CODEX = 3, NET_CITY = 4;
const size_t NET_BODY_LIMIT = 16384;
struct NetworkContext {
  TinyTV::RequestVersion version;
  tcp_pcb* pcb = nullptr;
  pbuf* pending = nullptr;
  uint16_t offset = 0;
  uint8_t owner = 0, index = 0;
  bool active = false, resolved = false, connected = false, closed = false, failed = false, sent = false;
  ip_addr_t address;
  uint32_t started = 0;
  String request, key;
  char* body = nullptr;
  TinyTV::HttpResponse* response = nullptr;
} network;

void Network_dns(const char*, const ip_addr_t* address, void* token) {
  if(!network.active || !network.version.accepts((uintptr_t)token)) return;
  if(address) { network.address = *address; network.resolved = true; }
  else network.failed = true;
}
err_t Network_connected(void*, tcp_pcb*, err_t error) {
  if(error != ERR_OK) network.failed = true;
  else network.connected = true;
  return ERR_OK;
}
void Network_error(void*, err_t) {
  network.pcb = nullptr; // lwIP has already freed it.
  network.failed = true;
}
err_t Network_receive(void*, tcp_pcb* pcb, pbuf* packet, err_t error) {
  if(!packet) { network.closed = true; return ERR_OK; }
  if(network.pending) return ERR_MEM; // Backpressure: lwIP retains this packet.
  if(error != ERR_OK) network.failed = true;
  network.pending = packet;
  network.offset = 0;
  (void)pcb;
  return ERR_OK;
}
void Network_release() {
  network.active = false;
  network.version.next();
  if(network.pcb) {
    tcp_arg(network.pcb, nullptr);
    tcp_recv(network.pcb, nullptr);
    tcp_err(network.pcb, nullptr);
    tcp_pcb* pcb = network.pcb;
    network.pcb = nullptr;
    // Abort also works during DNS/connect/send timeout; no lingering FIN queue.
    tcp_abort(pcb);
  }
  if(network.pending) { pbuf_free(network.pending); network.pending = nullptr; }
  delete network.response; network.response = nullptr;
  network.body = nullptr;
  network.request = ""; network.key = "";
}
void Network_cancel() { if(network.active) ++perf.canceled; Network_release(); }
void Network_cancelOwner(uint8_t owner) { if(network.active && network.owner == owner) Network_cancel(); }
uint8_t Network_owner() { return network.active ? network.owner : 0; }
bool Network_busy() { return network.active; }
bool Network_start(uint8_t owner, uint8_t index, const String &url, const String &key, const String &referer) {
  if(owner < 1 || owner > 4 || network.active || WiFi.status() != WL_CONNECTED || !url.startsWith("http://")) return false;
  int slash = url.indexOf('/', 7);
  String host = slash < 0 ? url.substring(7) : url.substring(7, slash);
  if(host.length() == 0 || host.length() > 120) return false;
  network.body = (char*)Runtime_workspace();
  if(!network.body) return false;
  network.response = new(std::nothrow) TinyTV::HttpResponse(network.body, NET_BODY_LIMIT);
  if(!network.response) { Network_release(); return false; }
  network.owner = owner; network.index = index; network.key = key;
  network.request = "GET " + (slash < 0 ? String("/") : url.substring(slash)) +
    " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: Mozilla/5.0 TinyTV/1.0\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n";
  if(referer.length()) network.request += "Referer: " + referer + "\r\n";
  network.request += "\r\n";
  network.started = millis();
  network.resolved = network.connected = network.closed = network.failed = network.sent = false;
  network.active = true;
  uint32_t token = network.version.next();
  err_t result = dns_gethostbyname(host.c_str(), &network.address, Network_dns, (void*)(uintptr_t)token);
  if(result == ERR_OK) network.resolved = true;
  else if(result != ERR_INPROGRESS) network.failed = true;
  return true;
}
void Network_poll() {
  if(!network.active) return;
  if(millis() - network.started >= 5000 || WiFi.status() != WL_CONNECTED) network.failed = true;
  if(!network.failed && network.resolved && !network.pcb && !network.closed) {
    network.pcb = tcp_new();
    if(!network.pcb) network.failed = true;
    else {
      tcp_arg(network.pcb, nullptr);
      tcp_recv(network.pcb, Network_receive);
      tcp_err(network.pcb, Network_error);
      if(tcp_connect(network.pcb, &network.address, 80, Network_connected) != ERR_OK) network.failed = true;
    }
  }
  if(!network.failed && network.connected && !network.sent) {
    err_t result = tcp_write(network.pcb, network.request.c_str(), network.request.length(), TCP_WRITE_FLAG_COPY);
    if(result == ERR_OK) { network.sent = true; tcp_output(network.pcb); network.request = ""; }
    else if(result != ERR_MEM) network.failed = true;
  }
  if(network.pending && !network.failed) {
    // Bound work per loop; TCP acknowledges only bytes consumed here.
    char bytes[512];
    uint16_t count = pbuf_copy_partial(network.pending, bytes, sizeof(bytes), network.offset);
    for(uint16_t i = 0; i < count; ++i) network.response->feed(bytes[i]);
    network.offset += count;
    if(network.pcb) tcp_recved(network.pcb, count);
    if(network.offset == network.pending->tot_len) {
      pbuf_free(network.pending); network.pending = nullptr;
    }
  }
  if(network.closed && !network.pending) network.response->closed();
  if(network.failed) network.response->fail();
  if(!network.response->done()) return;
  bool ok = network.response->ok();
  if(ok) ++perf.netOk[network.owner - 1]; else ++perf.netFail[network.owner - 1];
  Serial.printf("NET owner=%u ok=%u status=%u bytes=%u ms=%lu\n", network.owner, ok, network.response->status, network.response->size, millis()-network.started);
  // Dispatch only on the main loop, with page/config identity checked again.
  if(network.owner == NET_STOCK) Stock_result(network.index, network.key, ok, network.body);
  else if(network.owner == NET_WEATHER) Weather_result(network.key, ok, network.body);
  else if(network.owner == NET_CODEX) Codex_result(ok, network.body);
  else if(network.owner == NET_CITY) Weather_cityResult(ok, network.body);
  Network_release();
}
