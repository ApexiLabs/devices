#pragma once
namespace DashDisplayLayout {
struct Row { int labelY, valueY, detailY, scale; };
constexpr int labelWidth=100;
constexpr int detailWidth=88;
constexpr int valueWidth(unsigned count){return count==1?170:124;}
constexpr Row row(unsigned count, unsigned index) {
  return count == 1 ? Row{58,118,178,2} :
         index == 0 ? Row{38,72,102,1} : Row{146,178,208,1};
}
constexpr int alarmY(unsigned count){return count==1?204:120;}
} // namespace DashDisplayLayout
