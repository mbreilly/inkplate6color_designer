#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cstdlib>
#define PROGMEM
// Build with -DBW for the black-and-white Inkplate 6 sketch (see check.sh).
#ifdef BW
#define BLACK 1
#define WHITE 0
#define INKPLATE_1BIT 0
#define INKPLATE_3BIT 1
static const int W = 800, H = 600;
#else
enum { INKPLATE_BLACK, INKPLATE_WHITE, INKPLATE_GREEN, INKPLATE_BLUE, INKPLATE_RED, INKPLATE_YELLOW, INKPLATE_ORANGE };
static const int W = 600, H = 448;
#endif
struct FakeDisplay {
  uint8_t px[H][W];
#ifdef BW
  void clearDisplay() { memset(px, WHITE, sizeof(px)); }
  int getDisplayMode() { return INKPLATE_1BIT; }
#else
  void clearDisplay() { memset(px, INKPLATE_WHITE, sizeof(px)); }
#endif
  void drawPixel(int x, int y, int c) { if (x >= 0 && y >= 0 && x < W && y < H) px[y][x] = c; }
  void fillRect(int x, int y, int w, int h, int c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c); }
  double readBattery() { return 4.05; }
} display;
// The sketch folder is on the include path (-I firmware/<sketch>).
#ifdef BW
#define INK_BLACK 0
#define INK_WHITE 1
#endif
#include "display_types.h"
#include "fonts.h"
#include "layout.h"
static const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *MONTHS[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
struct Weather { int temp, feels, hi, lo, humidity, wind; char desc[48], main[24], city[32]; };
int32_t tzOffset = 0; uint32_t lastWeatherUtc = 0; Weather wx;
static const size_t SCHEDULE_MAX_BYTES = 4096; char scheduleText[SCHEDULE_MAX_BYTES + 1]; bool haveSchedule = false, scheduleError = false;
bool wifiOk = true; float shownVolts = 4.05f; // WIFI_OFF=1 in the environment draws the no-WiFi icon
bool drawImageFile(const char *, ...) { return false; }
#include "draw_part.inc"
int main(int argc, char **argv) {
  struct tm t = {}; t.tm_year = 126; t.tm_mon = 9; t.tm_mday = 20; t.tm_hour = 7; t.tm_min = 0;
  if (getenv("HM")) sscanf(getenv("HM"), "%d:%d", &t.tm_hour, &t.tm_min); // e.g. HM=11:38
  time_t e = timegm(&t); gmtime_r(&e, &t);
  lastWeatherUtc = e;
  if (getenv("WIFI_OFF")) wifiOk = false;
  wx.temp = 68; strcpy(wx.desc, "Partly cloudy");
  if (argc > 2) { // optional: a schedule file, as SCHEDULE_URL would return it
    static uint8_t raw[SCHEDULE_MAX_BYTES]; FILE *sf = fopen(argv[2], "rb"); size_t n = fread(raw, 1, sizeof(raw), sf); fclose(sf);
    size_t skip = n >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF ? 3 : 0;
    cleanScheduleText(raw + skip, n - skip, scheduleText, SCHEDULE_MAX_BYTES); haveSchedule = true;
  }
  drawMain(t);
#ifdef BW
  // Store ink numbers (0 black, 1 white) like the designer's preview.
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) display.px[y][x] = display.px[y][x] == BLACK ? 0 : 1;
#endif
  FILE *f = fopen(argv[1], "wb"); fwrite(display.px, 1, sizeof(display.px), f); fclose(f);
}
