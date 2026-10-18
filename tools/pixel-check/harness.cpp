#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#define PROGMEM
enum { INKPLATE_BLACK, INKPLATE_WHITE, INKPLATE_GREEN, INKPLATE_BLUE, INKPLATE_RED, INKPLATE_YELLOW, INKPLATE_ORANGE };
static const int W = 600, H = 448;
struct FakeDisplay {
  uint8_t px[H][W];
  void clearDisplay() { memset(px, INKPLATE_WHITE, sizeof(px)); }
  void drawPixel(int x, int y, int c) { if (x >= 0 && y >= 0 && x < W && y < H) px[y][x] = c; }
  void fillRect(int x, int y, int w, int h, int c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c); }
  double readBattery() { return 4.05; }
} display;
#include "../../firmware/InkplateDisplay/display_types.h"
#include "../../firmware/InkplateDisplay/fonts.h"
#include "../../firmware/InkplateDisplay/layout.h"
static const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *MONTHS[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
struct Weather { int temp, feels, hi, lo, humidity, wind; char desc[48], main[24], city[32]; };
int32_t tzOffset = 0; uint32_t lastWeatherUtc = 0; Weather wx;
bool drawImageFile(const char *) { return false; }
#include "draw_part.inc"
int main(int argc, char **argv) {
  struct tm t = {}; t.tm_year = 126; t.tm_mon = 9; t.tm_mday = 20; t.tm_hour = 7; t.tm_min = 0;
  time_t e = timegm(&t); gmtime_r(&e, &t);
  lastWeatherUtc = e;
  wx.temp = 68; strcpy(wx.desc, "Partly cloudy");
  drawMain(t);
  FILE *f = fopen(argv[1], "wb"); fwrite(display.px, 1, sizeof(display.px), f); fclose(f);
}
