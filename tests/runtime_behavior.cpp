#include "RuntimeCore.h"
#include <assert.h>
#include <string>
using namespace TinyTV;

static void feed(HttpResponse &response, const std::string &bytes) {
  for(char byte : bytes) response.feed(byte);
}
static void animation() {
  FrameSchedule schedule;
  assert(schedule.advance(100)); assert(schedule.frame == 0);
  for(unsigned ms = 101; ms < 137; ++ms) assert(!schedule.advance(ms));
  assert(schedule.advance(137)); assert(schedule.frame == 1);
  assert(!schedule.advance(137));
  assert(schedule.advance(137 + 37 * 23 + 10)); assert(schedule.frame == 4);
  assert(!schedule.advance(137 + 37 * 23 + 11));
  schedule.reset(); assert(schedule.advance(0xfffffff0));
  assert(!schedule.advance(0x14)); assert(schedule.advance(0x15));
  schedule.reset(); assert(schedule.advance(50)); assert(!schedule.advance(51));
}
static void staleRequests() {
  RequestVersion version;
  uint32_t dnsA = version.next();
  assert(version.accepts(dnsA));
  version.next(); // Page exit / timeout cancels DNS A.
  assert(!version.accepts(dnsA));
  uint32_t dnsB = version.next();
  assert(!version.accepts(dnsA)); assert(version.accepts(dnsB));
  version.value = 0xffffffff; assert(version.next() == 1);
  assert(!version.accepts(0));
}
static void textBoundaries() {
  auto width = [](uint32_t cp) { return cp < 128 ? (cp == 'i' ? 4 : 8) : 16; };
  const char* mixed = u8"i中A文";
  assert(fittingPrefix(mixed, 3, width) == 0);
  assert(fittingPrefix(mixed, 4, width) == 1);
  assert(fittingPrefix(mixed, 19, width) == 1);
  assert(fittingPrefix(mixed, 20, width) == 4);
  assert(fittingPrefix(mixed, 28, width) == 5);
  assert(fittingPrefix(mixed, 44, width) == strlen(mixed));
  assert(fittingPrefix(mixed, -1, width) == 0);
  char name[6];
  copyUtf8(name, u8"中文名", sizeof(name)); assert(!strcmp(name, u8"中"));
  char exact[7]; copyUtf8(exact, u8"中文名", sizeof(exact)); assert(!strcmp(exact, u8"中文"));
  char empty[1]; copyUtf8(empty, u8"中", 1); assert(empty[0] == 0);
  const char* cursor = u8"中😀i";
  assert(nextCodepoint(cursor) == 0x4e2d);
  assert(nextCodepoint(cursor) == 0x1f600);
  assert(nextCodepoint(cursor) == 'i'); assert(nextCodepoint(cursor) == 0);
  for(const std::string &malformed : {std::string("\xe4"), std::string("\xe4\xb8"),
      std::string("\xc0\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80")}) {
    const char* p = malformed.c_str();
    unsigned steps = 0;
    while(*p) { nextCodepoint(p); assert(++steps <= malformed.size()); }
  }
}
static void httpFraming() {
  char storage[65];
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhe");
    assert(!r.done()); feed(r, "llo"); assert(r.ok()); assert(r.size == 5);
    assert(!strcmp(storage, "hello"));
  }
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.1 200 OK\r\ntransfer-encoding: Chunked\r\n\r\n3;ext=yes\r\nabc\r\n2\r\nde\r\n0\r\nX-Trailer: yes\r\n\r\n");
    assert(r.ok()); assert(!strcmp(storage, "abcde"));
  }
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.0 200 OK\r\n\r\nclose framed");
    assert(!r.done()); r.closed(); assert(r.ok());
  }
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"); assert(r.ok());
  }
  for(const std::string bad : {
      "HTTP/1.1 503 Unavailable\r\n\r\n", "garbage\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 65\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n41\r\n",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nxyz\r\n",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na!",
      "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nab",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX: trailer\r\n"}) {
    HttpResponse r(storage, 64); feed(r, bad); r.closed(); assert(!r.ok());
  }
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.0 200 OK\r\n\r\n" + std::string(65, 'a')); assert(!r.ok()); assert(r.done());
  }
  {
    HttpResponse r(storage, 64);
    feed(r, "HTTP/1.1 200 OK\r\nX: " + std::string(300, 'a')); assert(r.done()); assert(!r.ok());
  }
}
int main() { animation(); staleRequests(); textBoundaries(); httpFraming(); }
