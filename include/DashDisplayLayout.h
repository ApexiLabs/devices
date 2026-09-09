#pragma once
namespace DashDisplayLayout {
struct Row { int labelY, valueY, detailY, scale; };
constexpr int labelWidth=120;
constexpr int detailWidth=116;
constexpr int valueWidth(unsigned count){return count==1?180:160;}
constexpr Row row(unsigned count, unsigned index) {
  return count == 1 ? Row{58,118,178,2} :
         index == 0 ? Row{42,76,108,1} : Row{134,168,200,1};
}
} // namespace DashDisplayLayout
