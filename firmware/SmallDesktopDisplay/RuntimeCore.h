#ifndef TINYTV_RUNTIME_CORE_H
#define TINYTV_RUNTIME_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

namespace TinyTV {
struct FrameSchedule {
  uint32_t last = 0;
  uint8_t frame = 0;
  bool fresh = true;
  void reset() { fresh = true; }
  bool advance(uint32_t now) {
    if(fresh) { fresh = false; last = now; return true; }
    uint32_t steps = (now - last) / 37;
    if(!steps) return false;
    last += steps * 37;
    frame = (frame + steps % 10) % 10;
    return true;
  }
};

// Tokens are values, never pointers to contexts that DNS may outlive.
struct RequestVersion {
  uint32_t value = 1;
  uint32_t next() { if(++value == 0) ++value; return value; }
  bool accepts(uint32_t token) const { return token == value; }
};

inline uint32_t nextCodepoint(const char* &text) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  if(!*p) return 0;
  unsigned n = *p < 0x80 ? 1 : (*p >= 0xc2 && *p <= 0xdf ? 2 :
    (*p >= 0xe0 && *p <= 0xef ? 3 : (*p >= 0xf0 && *p <= 0xf4 ? 4 : 0)));
  if(!n) { ++text; return '?'; }
  uint32_t cp = *p & (n == 1 ? 0x7f : (0x7f >> n));
  for(unsigned i = 1; i < n; ++i) {
    if(!p[i] || (p[i] & 0xc0) != 0x80) { ++text; return '?'; }
    cp = (cp << 6) | (p[i] & 0x3f);
  }
  if((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
    (n == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
    ++text; return '?';
  }
  text += n;
  return cp;
}

inline void copyUtf8(char* destination, const char* source, size_t capacity) {
  if(!capacity) return;
  size_t used = 0;
  while(*source) {
    const char* next = source;
    nextCodepoint(next);
    size_t bytes = next - source;
    if(bytes > capacity - 1 - used) break;
    memcpy(destination + used, source, bytes);
    used += bytes; source = next;
  }
  destination[used] = 0;
}

template<class Width> size_t fittingPrefix(const char* text, int width, Width glyphWidth) {
  const char* cursor = text;
  int used = 0;
  while(*cursor) {
    const char* next = cursor;
    uint32_t cp = nextCodepoint(next);
    int advance = glyphWidth(cp);
    if(used + advance > width) break;
    used += advance;
    cursor = next;
  }
  return cursor - text;
}

// Incremental, bounded HTTP/1.x framing. No Arduino or network dependencies.
class HttpResponse {
public:
  enum State { Status, Headers, Body, ChunkSize, ChunkBody, ChunkCR, ChunkLF,
    Trailers, Complete, Failed } state = Status;
  char* body;
  size_t capacity, size = 0;
  unsigned status = 0;
  HttpResponse(char* storage, size_t limit) : body(storage), capacity(limit) { body[0] = 0; }
  bool done() const { return state == Complete || state == Failed; }
  bool ok() const { return state == Complete && status == 200; }
  void fail() { state = Failed; }
  void closed() {
    if(state == Body && !hasLength) state = Complete;
    else if(!done()) fail();
  }
  void feed(char c) {
    if(done()) return;
    if(state == Body || state == ChunkBody) {
      if(size >= capacity) { fail(); return; }
      body[size++] = c; body[size] = 0;
      if(state == Body && hasLength && size == length) state = Complete;
      else if(state == ChunkBody && --remaining == 0) state = ChunkCR;
      return;
    }
    if(state == ChunkCR) { state = c == '\r' ? ChunkLF : Failed; return; }
    if(state == ChunkLF) { state = c == '\n' ? ChunkSize : Failed; return; }
    if(++headerBytes > 4096 && state != ChunkSize) { fail(); return; }
    if(c == '\n') {
      if(!lineSize || line[lineSize-1] != '\r') { fail(); return; }
      line[--lineSize] = 0;
      processLine(); lineSize = 0;
    }
    else if(lineSize < sizeof(line)-1) line[lineSize++] = c;
    else fail();
  }
private:
  char line[256] = {};
  size_t lineSize = 0, headerBytes = 0, length = 0, remaining = 0;
  bool hasLength = false, chunked = false;
  static bool equal(const char* a, const char* b) {
    while(*a && *b) {
      char x = *a++, y = *b++;
      if(x >= 'A' && x <= 'Z') x += 'a'-'A';
      if(y >= 'A' && y <= 'Z') y += 'a'-'A';
      if(x != y) return false;
    }
    return !*a && !*b;
  }
  void processLine() {
    if(state == Status) {
      if(strncmp(line, "HTTP/1.", 7) || (line[7] != '0' && line[7] != '1') ||
        line[8] != ' ' || strlen(line) < 12) { fail(); return; }
      status = atoi(line+9);
      if(status != 200) { fail(); return; }
      state = Headers;
    }
    else if(state == Headers) {
      if(!line[0]) {
        if(chunked && hasLength) { fail(); return; }
        state = chunked ? ChunkSize : (hasLength && !length ? Complete : Body);
        return;
      }
      char* colon = strchr(line, ':');
      if(!colon) { fail(); return; }
      *colon++ = 0;
      while(*colon == ' ' || *colon == '\t') ++colon;
      char* end = colon + strlen(colon);
      while(end > colon && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
      if(equal(line, "Content-Length")) {
        if(!*colon) { fail(); return; }
        size_t parsed = 0;
        for(char* p = colon; *p; ++p) {
          if(*p < '0' || *p > '9' || parsed > capacity / 10) { fail(); return; }
          parsed = parsed * 10 + (*p - '0');
          if(parsed > capacity) { fail(); return; }
        }
        if(hasLength && parsed != length) { fail(); return; }
        length = parsed; hasLength = true;
      }
      else if(equal(line, "Transfer-Encoding")) {
        if(!equal(colon, "chunked")) { fail(); return; }
        chunked = true;
      }
      else if(equal(line, "Content-Encoding") && !equal(colon, "identity")) fail();
    }
    else if(state == ChunkSize) {
      char* end;
      if(!line[0] || line[0] == '-' || line[0] == '+' || line[0] == ' ') { fail(); return; }
      unsigned long n = strtoul(line, &end, 16);
      if(end == line || (*end && *end != ';') || n > capacity - size) { fail(); return; }
      remaining = n;
      state = n ? ChunkBody : Trailers;
      headerBytes = 0;
    }
    else if(state == Trailers && !line[0]) state = Complete;
  }
};
}
#endif
