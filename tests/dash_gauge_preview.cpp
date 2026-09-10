#include "DashGauge.h"
#include "DashDisplayLayout.h"
#include "DashUploadStatus.h"
#include <iostream>
int main(){
  using namespace DashGauge;
  Rule pressure;strcpy(pressure.id,"oil_pressure");strcpy(pressure.units,"bar");
  float p[]={0,2,6,8};memcpy(pressure.points,p,sizeof(p));pressure.low=1;pressure.high=7;
  Rule temp;strcpy(temp.id,"oil_temperature");strcpy(temp.units,"C");
  float t[]={0,70,110,140};memcpy(temp.points,t,sizeof(t));temp.high=125;temp.highEnabled=true;
  std::cout<<"[";
  for(unsigned scenario=0;scenario<4;++scenario){
    if(scenario)std::cout<<",";
    const float value=scenario==0?90:145;bool fresh=scenario!=2;temp.highEnabled=scenario!=3;
    std::cout<<"{\"name\":\""<<(scenario==0?"Normal":scenario==1?"High alarm":scenario==2?"Stale / held":"Alarm disabled")<<"\",\"alarm\":"<<(alarm(&temp,value,fresh)!=Alarm::None?"true":"false")<<",\"rows\":[";
    for(unsigned i=0;i<2;++i){if(i)std::cout<<",";const auto row=DashDisplayLayout::row(2,i);
      std::cout<<"{\"label\":\""<<(i?"Oil Temp":"Oil Pressure")<<"\",\"value\":\""<<(i?value:4)<<"\",\"units\":\""<<(!fresh?"Stale":i?"°C":"bar")<<"\",\"colour\":"<<arcColour(i?&temp:&pressure,i?value:4,fresh)<<",\"labelY\":"<<row.labelY<<",\"valueY\":"<<row.valueY<<",\"detailY\":"<<row.detailY<<",\"held\":"<<(!fresh?"true":"false")<<"}";
    }
    const auto upload=scenario==0?DashUploadStatus::View::Accepted:scenario==1?DashUploadStatus::View::Failed:scenario==2?DashUploadStatus::View::Stale:DashUploadStatus::View::Unconfirmed;
    std::cout<<"],\"uploadColour\":"<<DashUploadStatus::colour(upload)<<",\"uploadX\":"<<DashDisplayLayout::uploadDotX<<",\"uploadY\":"<<DashDisplayLayout::uploadDotY<<",\"uploadRadius\":"<<DashDisplayLayout::uploadDotRadius<<"}";
  }std::cout<<"]";
}
