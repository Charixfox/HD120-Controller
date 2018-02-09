#ifndef FANDATA_INCLUDED
#define FANDATA_INCLUDED

// Types of Fans - This isn't really needed and can be removed unless mode will never be different for inner and outer LL120 fans
constexpr uint8_t SP120Type = 0;
constexpr uint8_t HD120Type = 1;
constexpr uint8_t LL120InnerType = 2;
constexpr uint8_t LL120OuterType = 3;

constexpr const char* FanNames[] = { "SP120", "HD120", "LL120Inner", "LL120Outer" };

//-- FanData type ---------------------------------------------------------------------------------
// Defines the attributes of a fan connected to the chasis
struct FanData
{
  constexpr FanData(uint8_t t, uint8_t n, uint8_t s) : fanType_(t), numFanLeds_(n), shiftCCW_(s) {}
  
  constexpr uint8_t fanType()         { return fanType_; }
  constexpr uint8_t ledsPerFan()      { return numFanLeds_; }
  constexpr uint8_t shiftCCW()        { return shiftCCW_; }
  
  uint8_t fanType_;
  uint8_t numFanLeds_;
  uint8_t shiftCCW_;
};

//-- TheFans array of fans ---------------------------------------------------------------------------------
// constexpr representation of the FanData array
struct TheFans
{
  template<int i>
  constexpr TheFans(const FanData(&arr)[i]): _theFans(arr), _sz(i) {}

  constexpr FanData operator[](int n) const { return _theFans[n]; }
  constexpr int size() const { return _sz; }
  
  const FanData* _theFans;
  int _sz;
};

constexpr int numFans(const TheFans fans, int i = 0, int count = 0) { return fans.size(); }

// These work like fancy #define macros
constexpr uint8_t fanType(const TheFans fans, int i)          { return fans[i].fanType(); }
constexpr const char* fanTypeName(const TheFans fans, int i)  { return FanNames[fans[i].fanType()]; }
constexpr uint8_t ledsPerFan(const TheFans fans, int i)           { return fans[i].ledsPerFan(); }
constexpr int8_t shiftCCW(const TheFans fans, int i)          { return fans[i].shiftCCW(); }

// This counts the led's by fan for allocating static arrays at compile time (and iterating)
constexpr int num_Leds(const TheFans fans, int i = 0, int count = 0)
{
    return i >= fans.size() - 1 ? count + fans[i].ledsPerFan() :
                                  num_Leds(fans, i + 1, count + fans[i].ledsPerFan());
}

// Descriptions of fans
constexpr FanData SP120(SP120Type, 12, 0);
constexpr FanData HD120(HD120Type, 12, 2);
constexpr FanData LL120Inner(LL120InnerType, 4, 1);
constexpr FanData LL120Outer(LL120OuterType, 12, 10);
#define LL120 LL120Inner, LL120Outer

#endif
