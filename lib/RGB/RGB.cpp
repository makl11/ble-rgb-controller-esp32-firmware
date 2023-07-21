#include "RGB.h"

RGB::RGB() {}

RGB::RGB(int red, int green, int blue)
{
  r = red;
  g = green;
  b = blue;
}

bool RGB::equals(RGB *other)
{
  return r == other->r &&
         g == other->g &&
         b == other->b;
}

RGB RGB::fromBLEColorData(BLEColorData bleData)
{
  return RGB(bleData.r, bleData.g, bleData.b);
}

RGB RGB::interpolate(RGB *color1, RGB *color2, float t)
{
  return RGB(
      color1->r + ((color2->r - color1->r) * t),
      color1->g + ((color2->g - color1->g) * t),
      color1->b + ((color2->b - color1->b) * t));
}
