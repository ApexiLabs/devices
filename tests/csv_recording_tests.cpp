#include "CsvLogger.h"
#include "HttpsExchange.h"
#include "UploadRecovery.h"
#include <cassert>
#include <iostream>

String Timekeeper::dateStamp(){return "20260924";}
String Timekeeper::logTimestamp(uint32_t ms){return String(ms);}

int main(){
  SPIClass spi;Timekeeper clock;CsvLogger csv;
  assert(csv.begin(18,spi));
  std::array<SensorSnapshot,AppConfig::kSensorCount> sensors{};
  for(size_t i=0;i<sensors.size();++i){sensors[i].id=AppConfig::kSensorConfigs[i].id;sensors[i].filteredValue=12.5;sensors[i].loopCurrentmA=4;}
  HttpsExchange exchange;UploadRecovery recovery;
  assert(exchange.submit("https://invalid", "{}", "test", "", ""));
  // Keep the real HTTP exchange pending while exercising the real CSV writer.
  for(uint32_t i=1;i<=100;++i){
    assert(exchange.result()==nullptr);
    assert(csv.logRow(clock,i*250,sensors));csv.flushIfNeeded(i*250);
  }
  for(int status:{422,400,413,401,403,429,500,-1}){
    exchange.workerResult().status=status;exchange.complete();
    if(UploadRecovery::payloadRejected(status))recovery.batchRejected(8);
    // Unavailable/full rejection archive must retain replay, never gate recording.
    assert(recovery.resolveRejected(status,true,[]{return false;},[]{assert(false);return false;})!=UploadRecovery::Resolution::Archived);
    assert(csv.logRow(clock,30000+csv.rowsWritten(),sensors));
    exchange.release();assert(exchange.submit("https://invalid","{}","test","",""));
  }
  assert(csv.rowsWritten()==108 && csv.lastError().isEmpty() && csvFlushes>1);
  const auto before=csv.rowsWritten();
  nextPrintLimit=5;
  assert(!csv.logRow(clock,40000,sensors));
  assert(csv.rowsWritten()==before && csv.lastError()=="CSV write failed");
  assert(csv.logRow(clock,40250,sensors));
  assert(csv.rowsWritten()==before+1 && csv.lastWriteAgeMs(40300)==50);
  assert(csv.lastError().isEmpty());
  const auto &bytes=*SD.files.at("/logs-20260924.csv");
  assert(bytes.find("\n40250,")!=std::string::npos); // Partial row cannot swallow next row.
  csv.disable();assert(!csv.logRow(clock,40500,sensors));
  std::cout<<"CSV recording continues through pending, rejected and failed uploads\n";
}
