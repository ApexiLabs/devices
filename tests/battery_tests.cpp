#include "BatteryEstimate.h"
#include <cassert>
#include <cstring>
#include <limits>
int main() {
  using namespace BatteryEstimate;
  assert(percent(0)==-1 && percent(4.5f)==-1);
  assert(percent(std::numeric_limits<float>::quiet_NaN())==-1);
  assert(percent(3.3f)==0 && percent(4.2f)==100 && percent(3.85f)==50);
  for(float v=3.3f;v<4.2f;v+=0.01f) assert(percent(v)<=percent(v+0.01f));
  Trend t;
  for(uint32_t n=0;n<=120000;n+=1000) t.update(3.8f+n/120000.0f*0.05f,true,n);
  assert(std::strcmp(t.value(),"rising")==0);
  t.update(3.8f,false,121000); assert(std::strcmp(t.value(),"collecting")==0);
  for(uint32_t n=122000;n<=241000;n+=1000) t.update(3.7f,false,n);
  assert(std::strcmp(t.value(),"falling")==0);
  t.update(3.7f,false,260000); assert(std::strcmp(t.value(),"collecting")==0);
  t.update(0,false,261000); assert(std::strcmp(t.value(),"unavailable")==0);
  Trend wrap;
  for(uint32_t n=0;n<=120000;n+=1000) wrap.update(3.8f,true,UINT32_MAX-60000+n);
  assert(std::strcmp(wrap.value(),"steady")==0);
}
