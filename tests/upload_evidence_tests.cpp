#include "UploadEvidence.h"
#include "DashUploadStatus.h"
#include "BlankPartition.h"
#include <cstring>
#include <cassert>
#include <string>
#ifdef TEST_UPLOAD_JSON
#include "UploadAcknowledgement.h"
#include "BatchAcknowledgement.h"
#endif
int main() {
  for(size_t size : {size_t(1),size_t(256),size_t(513)}) {
    assert(BlankPartition::verify(size,[](size_t,uint8_t *p,size_t n){std::memset(p,255,n);return true;}));
    assert(!BlankPartition::verify(size,[](size_t,uint8_t *,size_t){return false;}));
    assert(!BlankPartition::verify(size,[&](size_t offset,uint8_t *p,size_t n){
      std::memset(p,255,n);if(offset+n==size)p[n-1]=0;return true;}));
  }
  assert(!BlankPartition::verify(0,[](size_t,uint8_t *,size_t){return true;}));
  using namespace UploadEvidence;
  Tracker t;
  assert(t.status(true,true,true,0,1000).state==Unknown);
  assert(t.status(false,false,true,0,1000).state==Disabled);
  assert(t.status(true,false,true,0,1000).state==Failed);
  assert(t.status(true,true,true,0,1000).ageMs==UINT32_MAX);
  t.record(true,0);
  assert(t.status(true,true,true,100,1000).state==ServerAccepted);
  assert(t.status(true,true,false,100,1000).state==TransportSent);
  assert(t.status(true,true,true,100,1000).ageMs==100);
  t.record(false,110);
  assert(t.status(true,true,true,120,1000).state==Failed);
  assert(t.status(true,true,true,120,1000).ageMs==120);
  t.record(true,UINT32_MAX-10);
  assert(t.status(true,true,true,9,1000).ageMs==20);
  assert(t.status(true,true,true,9,1000).intervalMs==1000);
  auto checkWire=[&](bool enabled,bool available,bool https,DashUploadStatus::View expected) {
    const auto s=t.status(enabled,available,https,9,250);
    DashUploadStatus::Model receiver;
    assert(receiver.accept(DashUploadStatus::encode(s.state,s.transport,s.ageMs,s.intervalMs),100));
    assert(receiver.view(true,100)==expected);
  };
  checkWire(true,true,true,DashUploadStatus::View::Accepted);
  checkWire(true,true,false,DashUploadStatus::View::Unconfirmed);
  checkWire(true,false,true,DashUploadStatus::View::Failed);
  checkWire(false,false,true,DashUploadStatus::View::Disabled);
#ifdef TEST_UPLOAD_JSON
  assert(acceptedResponse(200,R"({"status":"ok","accepted":true,"desired_config":null})"));
  const std::string large=std::string("{\"status\":\"ok\",\"accepted\":true,\"desired_config\":{\"padding\":\"")+std::string(1800,'x')+"\"}}";
  assert(acceptedResponse(200,large.c_str()));
  const char *id="0123456789abcdef0123456789abcdef";
  const char *batch=R"({"status":"ok","accepted":true,"batch_id":"0123456789abcdef0123456789abcdef","accepted_count":8})";
  assert(BatchAcknowledgement::accepted(200,batch,id,8));
  assert(!BatchAcknowledgement::accepted(200,batch,id,7));
  assert(!BatchAcknowledgement::accepted(202,batch,id,8));
  assert(!BatchAcknowledgement::accepted(200,batch,"ffffffffffffffffffffffffffffffff",8));
  for(const auto *count:{"true","\"8\"","8.5","-1","0"}) {
    const std::string invalid=std::string("{\"status\":\"ok\",\"accepted\":true,\"batch_id\":\"")+id+"\",\"accepted_count\":"+count+"}";
    assert(!BatchAcknowledgement::accepted(200,invalid.c_str(),id,8));
  }
  assert(!BatchAcknowledgement::accepted(200,R"({"status":"partial","accepted":true})",id,8));
  for(const char *body : {"{}", "[]", "<html>login</html>",
      R"({"status":"ok","accepted":false})", R"({"status":"ok","accepted":1})",
      R"({"status":"ok","accepted":"true"})", R"({"status":"error","accepted":true})"})
    assert(!acceptedResponse(200,body));
  assert(!acceptedResponse(202,R"({"status":"ok","accepted":true})"));
  assert(!acceptedResponse(500,R"({"status":"ok","accepted":true})"));
#endif
}
