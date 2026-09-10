#pragma once
#include <cstring>
namespace DashDisplayLayout {
inline unsigned decimals(const char *units){return units && std::strcmp(units,"bar")==0?2:1;}
struct Row { int labelY, valueY, detailY, scale; };
constexpr int labelWidth=100;
constexpr int detailWidth=88;
constexpr int unitWidth=64;
constexpr int digitSlant=8;
constexpr int arcOuterRadius=120;
constexpr int arcInnerRadius=104;
constexpr int uploadDotX=231,uploadDotY=120,uploadDotRadius=4;
constexpr int uploadDotBackingRadius=6;
// Copy right-to-left so overlapping source pixels survive the in-place shear.
// The caller reserves extra width; no second framebuffer is required.
template<class Canvas> void italicize(Canvas& canvas,int x,int y,int width,int height) {
  for(int row=0;row<height;++row){
    const int shift=height>1 ? digitSlant*(height-1-row)/(height-1) : 0;
    for(int col=width-1;col>=0;--col)
      canvas.drawPixel(x+col+shift,y+row,canvas.readPixel(x+col,y+row));
    for(int col=0;col<shift;++col)canvas.drawPixel(x+col,y+row,0);
  }
}
constexpr int valueWidth(unsigned count){return count==1?170:124;}
constexpr Row row(unsigned count, unsigned index) {
  return count == 1 ? Row{58,118,178,2} :
         index == 0 ? Row{38,70,102,1} : Row{144,176,208,1};
}
constexpr int alarmY(unsigned count){return count==1?204:120;}
} // namespace DashDisplayLayout
