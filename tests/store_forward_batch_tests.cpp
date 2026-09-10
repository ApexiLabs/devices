#include "StoreForwardQueue.h"
#include "LittleFS.h"
#include <cassert>
#include <iostream>

int main(){
  LittleFS.reset();StoreForwardQueue queue;assert(queue.begin(true,128));
  for(int i=0;i<8;++i)assert(queue.enqueue(String("r"+std::to_string(i))));
  assert(queue.pendingRecords()==8 && queue.droppedRecords()==0);
  std::vector<String> batch;
  assert(queue.peekBatch(batch,8,100));assert(batch.size()==8);
  for(size_t i=0;i<batch.size();++i)assert(batch[i]==String("r"+std::to_string(i)));
  assert(queue.pendingRecords()==8); // selection never acknowledges
  assert(queue.peekBatch(batch,200,100) && batch.size()==8);
  assert(queue.peekBatch(batch,8,5) && batch.size()==1 && batch[0]=="r0");
  assert(!queue.peekBatch(batch,8,1) && batch.empty());
  assert(queue.pop());assert(queue.peekBatch(batch,8,100) && batch.size()==7 && batch[0]=="r1");
  StoreForwardQueue restarted;assert(restarted.begin(true,128));
  assert(restarted.peekBatch(batch,8,100) && batch.size()==7 && batch[0]=="r1");
  // A corrupt later record cannot be submitted or silently popped.
  LittleFS.corruptByte("/sfq-1.log",12);
  assert(restarted.peekBatch(batch,8,100) && batch.size()==3);
  assert(restarted.pendingRecords()==7);
  for(size_t i=0;i<batch.size();++i)assert(batch[i]==String("r"+std::to_string(i+1)));
  std::cout<<"Store-forward batch selection tests passed\n";
}
