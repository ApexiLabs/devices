#pragma once
#include <array>
#include <stdint.h>
namespace DashLcdBitmap {
inline constexpr uint32_t kPixels = 240 * 240;
inline constexpr uint32_t kHeaderSize = 54 + 256 * 4;
inline constexpr uint32_t kFileSize = kHeaderSize + kPixels;
// Top-down, indexed RGB332 BMP. Palette expansion matches TFT_eSprite::readPixel.
inline std::array<uint8_t,kHeaderSize> header() {
  std::array<uint8_t,kHeaderSize> h{};
  auto put=[&](unsigned offset,uint32_t v){for(int j=0;j<4;++j)h[offset+j]=(v>>(8*j))&255;};
  h[0]='B'; h[1]='M'; put(2,kFileSize); put(10,kHeaderSize); put(14,40);
  put(18,240); put(22,uint32_t(-240)); h[26]=1; h[28]=8;
  put(34,kPixels); put(46,256);
  constexpr uint8_t blue[]={0,11,21,31};
  for(unsigned c=0;c<256;++c) {
    uint16_t rgb=(c&0xe0)<<8 | (c&0xc0)<<5 | (c&0x1c)<<6 | (c&0x1c)<<3 | blue[c&3];
    uint8_t r=(rgb>>8)&0xf8,g=(rgb>>3)&0xfc,b=(rgb<<3)&0xf8;
    h[54+c*4]=b|(b>>5); h[55+c*4]=g|(g>>6); h[56+c*4]=r|(r>>5);
  }
  return h;
}
} // namespace DashLcdBitmap
