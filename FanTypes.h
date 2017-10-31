#ifndef FANDATA_INCLUDED
#define FANDATA_INCLUDED

// Types of Fans
constexpr uint8_t SP120Type = 0;
constexpr uint8_t HD120Type = 1;
constexpr uint8_t LL120Type = 2;

constexpr const char* FanNames[] = { "SP120", "HD120", "LL120" };

// Location of Fan
constexpr uint8_t FrontSide = 1;
constexpr uint8_t BackSide = 2;
constexpr uint8_t TopSide = 3;
constexpr uint8_t BottomSide = 4;

struct FanData
{
  constexpr FanData(uint8_t t, uint8_t n, uint8_t l, uint8_t o) : fanType_(t), numFanLeds_(n), numRings_(l), outerOffset_(o) {}
  
  constexpr uint8_t fanType() { return fanType_; }
  constexpr int numFanLeds() { return (int) numFanLeds_; }
  constexpr int numRings() { return (int)numRings_; }
  constexpr uint8_t outerOffset() { return outerOffset_; }
  
  uint8_t fanType_;
  uint8_t numFanLeds_;
  uint8_t numRings_;
  uint8_t outerOffset_;
};

//-- TheFans array of fans ---------------------------------------------------------------------------------
// constexpr of the fan array
struct TheFans
{
  template<int i>
  constexpr TheFans(const FanData(&arr)[i]): _theFans(arr), _sz(i) {}

  constexpr FanData operator[](int n) const { return _theFans[n]; }
  constexpr int size() const { return _sz; }
  
  const FanData* _theFans;
  int _sz;
};

constexpr int numFans(const TheFans fans, int i = 0, int count = 0)
{
    return fans.size();
//   return i >= fans.size() - 1 ? count + fans[i].numLoops() :
//                                       numFans(fans, i + 1, count + fans[i].numLoops());
}

constexpr const char* fanType(const TheFans fans, int i)
{
    return FanNames[fans[i].fanType()];
}

constexpr int ledsPerFan(const TheFans fans, int i)
{
    return fans[i].numFanLeds();
}

constexpr int ringsPerFan(const TheFans fans, int i)
{
    return fans[i].numRings();
}

constexpr int outerOffset(const TheFans fans, int i)
{
    return fans[i].outerOffset();
}

constexpr int numLeds(const TheFans fans, int i = 0, int count = 0)
{
    return i >= fans.size() - 1 ? count + fans[i].numFanLeds() :
                                  numLeds(fans, i + 1, count + fans[i].numFanLeds());
}

constexpr FanData SP120(SP120Type, 12, 1, 0);
constexpr FanData HD120(HD120Type, 12, 1, 0);
constexpr FanData LL120(LL120Type, 16, 2, 4);

#endif
