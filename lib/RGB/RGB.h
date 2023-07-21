struct BLEColorData
{
    char r;
    char g;
    char b;
};

class RGB
{
public:
    int r;
    int g;
    int b;
    RGB();
    RGB(int red, int green, int blue);
    bool equals(RGB *other);
    static RGB fromBLEColorData(BLEColorData bleData);
    static RGB interpolate(RGB *color1, RGB *color2, float t);
};
