#include "SystemEvents.h"
#include <cassert>
int main() {
  SystemEvents::Queue<2> q; SystemEvents::Event e{}; e.seq=1;
  assert(q.push(e)); e.seq=2; assert(q.push(e)); assert(!q.push(e)&&q.dropped==1);
  assert(q.front()->seq==1); q.pop(); assert(q.front()->seq==2); q.pop(); assert(!q.front());
  assert(SystemEvents::safeName("logger-20260908-00000001.log"));
  assert(SystemEvents::safeName("dash-aabbcc-20260908-00000001.log"));
  assert(SystemEvents::safeName("logs-20260908.csv"));
  for(auto name:{"../logs-x.csv","/logs-x.csv","logs-../x.csv","logs-x.csv\n","secrets.log"}) assert(!SystemEvents::safeName(name));
  assert(SystemEvents::safeCode("http_get_settings")); assert(!SystemEvents::safeCode("request\nsecret"));
  SystemEvents::Receiver r; SystemEvents::Event received{};
  e.boot=0xaabbccdd; e.seq=1234; e.ms=2345; e.httpStatus=401; e.durationMs=42; strcpy(e.code,"http_settings");
  for(uint8_t part=0;part<4;++part) {
    const auto f=SystemEvents::frame(e,part);
    assert(r.accept(f.data(),f.size(),received)==(part==3));
  }
  assert(received.boot==e.boot && received.seq==e.seq && received.httpStatus==401 && received.durationMs==42);
  assert(!strcmp(received.code,e.code));
  auto zero=SystemEvents::frame(e,0),two=SystemEvents::frame(e,2),three=SystemEvents::frame(e,3);
  assert(!r.accept(zero.data(),20,received)); assert(!r.accept(two.data(),20,received)); assert(!r.accept(three.data(),20,received));
  assert(!r.accept(zero.data(),19,received));
}
