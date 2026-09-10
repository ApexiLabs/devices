#include "DashTelemetry.h"
#include "DashDisplayLayout.h"
#include "DashUploadStatus.h"
#include "DashBattery.h"
#include "DashLcdBitmap.h"
#include "DashDiagnostics.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace DashTelemetry;
int main() {
  assert(DashDisplayLayout::decimals("bar")==2);
  assert(DashDisplayLayout::decimals("C")==1);
  assert(DashDisplayLayout::decimals(nullptr)==1);
  {
    DashBattery::Model b;assert(b.due(0)&&!b.fresh(0));
    b.update(1350,1,0);assert(b.fresh(0));assert(std::abs(b.voltage-4.05f)<0.001f);assert(b.percent(0)==80);
    assert(!b.due(999)&&b.due(1000));assert(!b.fresh(5000)&&b.percent(5000)==-1);
    b.update(0,1,6000);assert(!b.valid);b.update(1500,1,7000);assert(!b.valid);
    b.update(1350,std::numeric_limits<float>::quiet_NaN(),8000);assert(!b.valid);
    b.update(1350,1,UINT32_MAX-99);assert(b.fresh(100));
    b.update(1300,1,200);assert(b.voltage>3.9f&&b.voltage<4.05f);
  }
  {
    using namespace DashUploadStatus;
    DashUploadStatus::Model u;assert(u.view(true,0)==View::Unknown);
    auto f=encode(3,1,0,1000);assert(u.accept(f,100));assert(u.view(true,100)==View::Accepted);
    assert(u.view(false,100)==View::Unknown);assert(u.view(true,5100)==View::Stale);
    assert(u.accept(encode(3,1,4900,1000),100));assert(u.view(true,200)==View::Stale);
    assert(u.accept(encode(3,1,20000,10000),100));assert(u.view(true,200)==View::Accepted);
    assert(u.accept(encode(4,2,0,1000),100));assert(u.view(true,100)==View::Unconfirmed);
    assert(u.accept(encode(2,1,30,1000),100));assert(u.view(true,100)==View::Failed);
    assert(u.accept(encode(1,0,kNever,1000),100));assert(u.view(true,100)==View::Disabled);
    assert(!u.accept(encode(3,2,0,1000),100));assert(!u.accept(encode(3,1,kNever,1000),100));
    assert(!u.accept(encode(3,1,0,0),100));f[0]=2;assert(!u.accept(f,100));
    f=encode(3,1,0,1000);f[12]=1;assert(!u.accept(f,100));
    assert(u.accept(encode(3,1,0,1000),UINT32_MAX-99));assert(u.view(true,100)==View::Accepted);assert(u.age(100)==200);
    assert(window(UINT32_MAX)==UINT32_MAX-1);
    u=DashUploadStatus::Model{};assert(u.view(true,100)==View::Unknown);
  }
  struct Canvas {
    int pixels[5][20]{};
    int readPixel(int x,int y){assert(x>=0&&x<20&&y>=0&&y<5);return pixels[y][x];}
    void drawPixel(int x,int y,int c){assert(x>=0&&x<20&&y>=0&&y<5);pixels[y][x]=c;}
  } canvas;
  for(int y=0;y<5;++y)for(int x=0;x<6;++x)canvas.pixels[y][2+x]=1+y*6+x;
  DashDisplayLayout::italicize(canvas,2,0,6,5);
  for(int y=0;y<5;++y){
    const int shift=8*(4-y)/4;
    for(int x=0;x<6;++x)assert(canvas.pixels[y][2+shift+x]==1+y*6+x);
    for(int x=0;x<shift;++x)assert(canvas.pixels[y][2+x]==0);
  }
  DashDisplayLayout::italicize(canvas,0,0,1,1); // Degenerate height is safe.
  DashDiagnostics::Log log;
  char logLine[180];
  DashDiagnostics::formatEvent(logLine,sizeof(logLine),{12345,3001,0,3});
  assert(std::string(logLine)=="[12.345] WARN apexi-dash sensor_state sensor=0 state=stale sample_age_ms=3001\n");
  DashDiagnostics::formatEvent(logLine,sizeof(logLine),{13000,0,1,5});
  assert(std::string(logLine).find("INFO apexi-dash")!=std::string::npos);
  for(unsigned i=0;i<70;++i)log.add(i,0,5,i);
  assert(log.size()==64 && log.at(0).ms==6 && log.at(63).ms==69);
  Sensor diagnosticSensor;
  assert(DashDiagnostics::state(diagnosticSensor,true,10)==0);
  diagnosticSensor.received=true;diagnosticSensor.receivedMs=1;
  assert(DashDiagnostics::state(diagnosticSensor,true,10)==1);
  diagnosticSensor.metadata=7;diagnosticSensor.valid=true;
  assert(DashDiagnostics::state(diagnosticSensor,true,10)==5);
  assert(DashDiagnostics::state(diagnosticSensor,true,3001)==3);
  assert(DashDiagnostics::state(diagnosticSensor,false,10)==2);
  diagnosticSensor.fault=SensorFault::AdcUnavailable;
  assert(DashDiagnostics::state(diagnosticSensor,true,10)==4);
  const auto bmp=DashLcdBitmap::header();
  auto word=[&](unsigned p){return uint32_t(bmp[p]) | uint32_t(bmp[p+1])<<8 | uint32_t(bmp[p+2])<<16 | uint32_t(bmp[p+3])<<24;};
  assert(bmp[0]=='B' && bmp[1]=='M');
  assert(word(2)==58678 && word(10)==1078 && word(14)==40);
  assert(word(18)==240 && word(22)==uint32_t(-240));
  assert(bmp[26]==1 && bmp[28]==8 && word(34)==57600 && word(46)==256);
  assert(word(54)==0 && word(54+255*4)==0x00ffffff);
  assert(word(54+0xe0*4)==0x00ff0000 && word(54+0x1c*4)==0x0000ff00);
  assert(word(54+3*4)==0x000000ff);
  constexpr auto single=DashDisplayLayout::row(1,0);
  static_assert(DashDisplayLayout::arcOuterRadius==240/2,"Arcs reach the screen edge");
  static_assert(DashDisplayLayout::arcInnerRadius==104,"Text clearance stays unchanged");
  static_assert(DashDisplayLayout::uploadDotX-DashDisplayLayout::uploadDotRadius>224,"Upload dot clears alarm band");
  static_assert(DashDisplayLayout::uploadDotX+DashDisplayLayout::uploadDotRadius<240,"Upload dot stays on screen");
  static_assert(DashDisplayLayout::uploadDotX-DashDisplayLayout::uploadDotBackingRadius>224,"Backing clears alarm");
  static_assert(DashDisplayLayout::uploadDotX+DashDisplayLayout::uploadDotBackingRadius<240,"Backing stays on screen");
  static_assert(50*50+90*90<104*104,"Top caption clears inner arc");
  static_assert(44*44+92*92<104*104,"Bottom detail clears inner arc");
  static_assert(32*32+96*96<104*104,"Enlarged units clear inner arc");
  static_assert(DashDisplayLayout::labelWidth==100 && DashDisplayLayout::detailWidth==88);
  static_assert(DashDisplayLayout::valueWidth(2)==124 && DashDisplayLayout::valueWidth(1)==170);
  static_assert(62*62+82*82<104*104,"Bottom large value clears inner arc");
  constexpr auto top=DashDisplayLayout::row(2,0);
  constexpr auto bottom=DashDisplayLayout::row(2,1);
  static_assert(single.valueY==118 && single.scale==2, "One value fills the centre");
  static_assert(top.scale==1 && bottom.scale==1, "Two values use 48-pixel digits");
  static_assert(top.detailY < bottom.labelY, "Two reading groups do not overlap");
  static_assert(top.detailY+8<=110 && bottom.labelY-8>130,"Alarm band clears both rows");
  static_assert(top.valueY+24<=top.detailY-8 && bottom.valueY+24<=bottom.detailY-8,"Digits clear enlarged units");
  static_assert(top.labelY+8<=top.valueY-24 && bottom.labelY+8<=bottom.valueY-24,"Labels clear digits");
  static_assert(single.labelY+8 < single.valueY-48, "Large digits clear the label");
  static_assert(single.valueY+48 < single.detailY-8, "Large digits clear the units");
  Model m;
  assert(m.accept(text(Id,0,2,"oil_pressure"),1));
  assert(m.accept(text(Name,0,2,"Oil Pressure"),2));
  assert(m.accept(text(Units,0,2,"bar"),3));
  assert(m.accept(sample(0,2,4.25f,true,SensorFault::None,false),4));
  assert(m.sensors[0].metadata==7 && m.sensors[0].value==4.25f);
  assert(m.sensors[0].hasLastGood && m.sensors[0].lastGoodValue==4.25f);
  assert(m.sensors[0].fresh(true,3003));
  assert(!m.sensors[0].fresh(true,3004));
  assert(!m.sensors[0].fresh(false,5));
  assert(m.accept(sample(0,2,0,false,SensorFault::AdcUnavailable,false),5));
  assert(!m.sensors[0].valid && m.sensors[0].fault==SensorFault::AdcUnavailable);
  assert(m.sensors[0].lastGoodValue==4.25f && m.sensors[0].lastGoodMs==4);
  assert(m.accept(sample(0,2,std::numeric_limits<float>::quiet_NaN(),true,SensorFault::None,false),6));
  assert(!m.sensors[0].valid);
  auto bad=text(Id,0,2,"bad"); bad[0]=2; assert(!m.accept(bad,7));
  bad=text(Id,2,2,"bad"); assert(!m.accept(bad,7));
  bad=text(Id,0,9,"bad"); assert(!m.accept(bad,7));
  bad=text(Id,0,2,"bad"); bad[19]='x'; assert(!m.accept(bad,7));
  bad=sample(0,2,3,true,SensorFault::None,false); bad[5]=255; assert(!m.accept(bad,7));
  assert(m.accept(text(Id,0,2,"new_id"),8));
  assert(m.sensors[0].metadata==1 && !m.sensors[0].received);
  assert(!m.sensors[0].hasLastGood);
  m.sensors[0].metadata=7;
  assert(m.accept(sample(0,2,1,true,SensorFault::None,true),0xfffffff0U));
  assert(m.sensors[0].fresh(true,15));
  assert(m.sensors[0].warning);
  m.clear(); assert(m.count==0 && !m.sensors[0].received);
  assert(validRefresh(250) && validRefresh(1000) && validRefresh(5000));
  assert(!validRefresh(0) && !validRefresh(251) && !validRefresh(5001));
  std::cout << "Dash telemetry tests passed\n";
}
