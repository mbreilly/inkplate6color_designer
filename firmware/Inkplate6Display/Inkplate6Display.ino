// Inkplate 6 display (black and white, 800x600): clock, weather, schedule and
// photo gallery. Made by Inkplate 6 Designer. Layout, fonts and timing live in
// layout.h and fonts.h; WiFi and the weather key live in secrets.h. Photos are
// read from the microSD card (copy the sdcard/ folder onto it).
//
// The main screen uses the panel's black-and-white mode, so most changes are
// quick partial updates: only the pixels that differ change, with no flashing.
// The board deep-sleeps between updates, which wipes the library's copy of what
// is on the panel, so on each wake the sketch redraws the previous screen from
// what it kept in RTC memory (shownMinute and the weather etc.), hands that to
// the library with preloadScreen(), then draws the new screen. Every
// FULL_REFRESH_EVERY quick updates it does a full refresh to clear ghosting.
// Photos use the 8-gray mode, which only does full refreshes.
// The onboard RTC keeps UTC and wakes the board via GPIO39.
//
// Board: Tools -> Board -> Inkplate Boards -> "Soldered Inkplate6"
// Libraries: InkplateLibrary (Soldered), ArduinoJson 7.x

#if !defined(ARDUINO_INKPLATE6V2) && !defined(ARDUINO_INKPLATE6)
#error "Select Soldered Inkplate6 in Tools -> Board."
#endif

// Ink numbers used in layout.h (PATTERNS holds these).
#define INK_BLACK 0
#define INK_WHITE 1

#include "Inkplate.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "display_types.h"
#include "fonts.h"
#include "layout.h"
#include "secrets.h"

Inkplate display(INKPLATE_1BIT);

static const int W = E_INK_WIDTH;  // 800
static const int H = E_INK_HEIGHT; // 600
static const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *MONTHS[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};

// ---------------------------------------------------------------- state kept through deep sleep
// RTC memory survives deep sleep but is reset on power-up or the reset button.

struct Weather
{
    int temp, feels, hi, lo, humidity, wind;
    char desc[48], main[24], city[32];
};

RTC_DATA_ATTR int32_t tzOffset = UTC_OFFSET_FALLBACK_SECONDS;
RTC_DATA_ATTR uint32_t nextFetchUtc = 0;
RTC_DATA_ATTR uint32_t lastWeatherUtc = 0; // 0 = never fetched
RTC_DATA_ATTR Weather wx;
RTC_DATA_ATTR uint32_t leadMs[2] = {3000, 8000};   // wake -> update done, for [main screen, photo]
RTC_DATA_ATTR uint32_t nextScheduleUtc = 0;        // next read of SCHEDULE_URL
RTC_DATA_ATTR bool scheduleError = false;          // the last read of SCHEDULE_URL failed
RTC_DATA_ATTR bool wifiOk = false;                 // the last WiFi connection worked (for the WiFi icon)
// What is on the panel now, so the next wake can redraw it for a partial update.
RTC_DATA_ATTR int8_t shownScreen = -2;     // -1 main screen, 0+ photo, -2 unknown
RTC_DATA_ATTR uint32_t shownMinute = 0;    // local minute (since 1970) it was drawn for
RTC_DATA_ATTR float shownVolts = 0;        // battery reading it shows
RTC_DATA_ATTR uint16_t quickUpdates = 0;   // partial updates since the last full refresh

// Schedule read from SCHEDULE_URL, already cleaned to printable ASCII (cleanScheduleText).
// The last good copy is kept in flash (loadSchedule / fetchSchedule), so it survives
// deep sleep and power cuts and a failed read keeps showing it.
static const size_t SCHEDULE_MAX_BYTES = 4096;
char scheduleText[SCHEDULE_MAX_BYTES + 1];
bool haveSchedule = false;

// ---------------------------------------------------------------- text

// Colors are numbers into PATTERNS (layout.h): an 8x8 tile of inks, repeated
// from the screen's top-left. 0-6 are the solid inks, mixed colors follow.
inline uint8_t inkAt(uint8_t color, int x, int y)
{
    return PATTERNS[color][(y & 7) * 8 + (x & 7)];
}

// Sets one pixel to an ink, in whichever mode the display is in
// (black and white: BLACK / WHITE; 8 grays: 0 = black, 7 = white).
inline void putInk(int x, int y, uint8_t ink)
{
    if (display.getDisplayMode() == INKPLATE_3BIT)
        display.drawPixel(x, y, ink == INK_BLACK ? 0 : 7);
    else
        display.drawPixel(x, y, ink == INK_BLACK ? BLACK : WHITE);
}

void fillColor(int x, int y, int w, int h, uint8_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            putInk(xx, yy, inkAt(color, xx, yy));
}

const DGlyph &glyphOf(const DFont *f, char ch)
{
    uint8_t c = (uint8_t)ch;
    if (c < 32 || c > 127)
        c = '?';
    return f->glyphs[c - 32];
}

// Same widths the designer uses, so alignment matches the preview.
int textWidth(const DFont *f, const char *s)
{
    int pen = 0, right = 0;
    for (; *s; s++)
    {
        const DGlyph &g = glyphOf(f, *s);
        int r = pen + (g.w ? g.w + g.xoff : 1);
        if (r > right)
            right = r;
        pen += g.adv;
        if (pen > right)
            right = pen;
    }
    return right;
}

int advanceWidth(const DFont *f, const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += glyphOf(f, *s).adv;
    return w;
}

// x, y: anchor point and top of the ascent, as in the designer.
void drawText(const DFont *f, const char *s, int x, int y, uint8_t align, uint8_t color)
{
    int w = textWidth(f, s);
    int pen = x - (align == AL_CENTER ? w / 2 : align == AL_RIGHT ? w : 0);
    int base = y + f->ascent;
    for (; *s; s++)
    {
        const DGlyph &g = glyphOf(f, *s);
        const uint8_t *bits = f->bits + g.offset;
        int top = base - g.yoff - g.h;
        uint32_t i = 0;
        for (int yy = 0; yy < g.h; yy++)
            for (int xx = 0; xx < g.w; xx++, i++)
                if (bits[i >> 3] & (0x80 >> (i & 7)))
                {
                    int px = pen + g.xoff + xx, py = top + yy;
                    if (px >= 0 && py >= 0 && px < W && py < H)
                        putInk(px, py, inkAt(color, px, py));
                }
        pen += g.adv;
    }
}

// Shorten with "..." until it fits, exactly like the designer's preview.
void fitText(const DFont *f, char *text, size_t n, int maxWidth)
{
    if (maxWidth <= 0 || advanceWidth(f, text) <= maxWidth)
        return;
    char buf[128];
    int len = strlen(text);
    snprintf(buf, sizeof(buf), "%.*s...", len, text);
    while (len > 0 && advanceWidth(f, buf) > maxWidth)
    {
        len--;
        snprintf(buf, sizeof(buf), "%.*s...", len, text);
    }
    while (len > 0 && text[len - 1] == ' ')
        len--;
    text[len] = 0;
    strncat(text, "...", n - len - 1);
}

void fmtTime(char *out, size_t n, int hour, int minute, bool ampm)
{
    if (HOUR24)
        snprintf(out, n, "%02d:%02d", hour, minute);
    else
        snprintf(out, n, "%d:%02d%s", (hour % 12) ? hour % 12 : 12, minute, ampm ? (hour < 12 ? " AM" : " PM") : "");
}

// Replaces {token} in fmt using lookup(); unknown tokens are left as they are.
void fill(char *out, size_t n, const char *fmt, Lookup lookup, const struct tm &now)
{
    size_t o = 0;
    while (*fmt && o + 1 < n)
    {
        const char *close = *fmt == '{' ? strchr(fmt, '}') : nullptr;
        if (close && close - fmt < 20)
        {
            char name[20], val[64];
            snprintf(name, sizeof(name), "%.*s", (int)(close - fmt - 1), fmt + 1);
            if (lookup(name, val, sizeof(val), now))
            {
                for (const char *v = val; *v && o + 1 < n; v++)
                    out[o++] = *v;
                fmt = close + 1;
                continue;
            }
        }
        out[o++] = *fmt++;
    }
    out[o] = 0;
}

bool dateToken(const char *k, char *v, size_t n, const struct tm &t)
{
    if (!strcmp(k, "weekday"))
        snprintf(v, n, "%s", WEEKDAYS[t.tm_wday]);
    else if (!strcmp(k, "wkd"))
        snprintf(v, n, "%.3s", WEEKDAYS[t.tm_wday]);
    else if (!strcmp(k, "month"))
        snprintf(v, n, "%s", MONTHS[t.tm_mon]);
    else if (!strcmp(k, "mon"))
        snprintf(v, n, "%.3s", MONTHS[t.tm_mon]);
    else if (!strcmp(k, "day"))
        snprintf(v, n, "%d", t.tm_mday);
    else if (!strcmp(k, "dd"))
        snprintf(v, n, "%02d", t.tm_mday);
    else if (!strcmp(k, "mm"))
        snprintf(v, n, "%02d", t.tm_mon + 1);
    else if (!strcmp(k, "year"))
        snprintf(v, n, "%d", t.tm_year + 1900);
    else
        return false;
    return true;
}

bool weatherToken(const char *k, char *v, size_t n, const struct tm &)
{
    bool have = lastWeatherUtc != 0;
    const char *imperial = strcmp(WEATHER_UNITS, "imperial") == 0 ? "\177F" : "\177C";
    if (!strcmp(k, "unit"))
        snprintf(v, n, "%s", imperial);
    else if (!strcmp(k, "temp"))
        have ? snprintf(v, n, "%d", wx.temp) : snprintf(v, n, "--");
    else if (!strcmp(k, "feels"))
        have ? snprintf(v, n, "%d", wx.feels) : snprintf(v, n, "--");
    else if (!strcmp(k, "hi"))
        have ? snprintf(v, n, "%d", wx.hi) : snprintf(v, n, "--");
    else if (!strcmp(k, "lo"))
        have ? snprintf(v, n, "%d", wx.lo) : snprintf(v, n, "--");
    else if (!strcmp(k, "humidity"))
        have ? snprintf(v, n, "%d", wx.humidity) : snprintf(v, n, "--");
    else if (!strcmp(k, "wind"))
        have ? snprintf(v, n, "%d", wx.wind) : snprintf(v, n, "--");
    else if (!strcmp(k, "desc"))
        snprintf(v, n, "%s", wx.desc);
    else if (!strcmp(k, "main"))
        snprintf(v, n, "%s", wx.main);
    else if (!strcmp(k, "city"))
        snprintf(v, n, "%s", wx.city);
    else
        return false;
    return true;
}

bool updatedToken(const char *k, char *v, size_t n, const struct tm &)
{
    if (strcmp(k, "time"))
        return false;
    time_t t = (time_t)lastWeatherUtc + tzOffset;
    struct tm u;
    gmtime_r(&t, &u);
    fmtTime(v, n, u.tm_hour, u.tm_min, true);
    return true;
}

bool batteryToken(const char *k, char *v, size_t n, const struct tm &)
{
    if (strcmp(k, "volts"))
        return false;
    snprintf(v, n, "%.2f", shownVolts);
    return true;
}

// ---------------------------------------------------------------- screens

// Weekly schedule, one line per entry: "MTWT  6:40am  In DRL until 1:40pm".
// Entries that span a window or last all day light up while they're happening.
void shortTime(char *out, size_t n, int minute)
{
    int hour = (minute / 60) % 24, m = minute % 60;
    if (HOUR24)
        snprintf(out, n, "%02d:%02d", hour, m);
    else
        snprintf(out, n, "%d:%02d%s", (hour % 12) ? hour % 12 : 12, m, hour < 12 ? "am" : "pm");
}

// Turns a schedule file (UTF-8) into what the fonts can draw, like cleanText() in the
// designer: curly quotes, dashes, "..." and no-break spaces become plain characters, a
// tab becomes "|", the degree sign becomes the fonts' code 127 and anything else
// outside printable ASCII is dropped. Writes at most cap characters plus a 0 to out.
// Returns the cleaned length.
size_t cleanScheduleText(const uint8_t *in, size_t n, char *out, size_t cap)
{
    size_t o = 0;
    auto put = [&](const char *t) {
        for (; *t && o < cap; t++)
            out[o++] = *t;
    };
    for (size_t i = 0; i < n; i++)
    {
        uint8_t c = in[i];
        if (c == '\t')
            put("|");
        else if (c == '\n' || (c >= 32 && c < 127))
        {
            char t[2] = {(char)c, 0};
            put(t);
        }
        else if (c == 0xC2 && i + 1 < n && (in[i + 1] == 0xA0 || in[i + 1] == 0xB0))
            put(in[++i] == 0xA0 ? " " : "\177");
        else if (c == 0xE2 && i + 2 < n && in[i + 1] == 0x80)
        {
            uint8_t d = in[i + 2];
            i += 2;
            if (d == 0x98 || d == 0x99)
                put("'");
            else if (d == 0x9C || d == 0x9D)
                put("\"");
            else if (d == 0x93 || d == 0x94)
                put("-");
            else if (d == 0xA6)
                put("...");
        }
        else if (c >= 0xC0)
        {
            // skip the rest of any other multi-byte character
            while (i + 1 < n && (in[i + 1] & 0xC0) == 0x80)
                i++;
        }
    }
    out[o] = 0;
    return o;
}

void trimCopy(char *out, size_t n, const char *from, const char *to)
{
    while (from < to && *from == ' ')
        from++;
    while (to > from && to[-1] == ' ')
        to--;
    snprintf(out, n, "%.*s", (int)(to - from), from);
}

// Reads the line starting at *p into line and moves *p past it. Returns false when the
// line is blank or a comment (the caller skips it).
bool nextFileLine(const char **p, FileLine &line)
{
    const char *start = *p, *end = strchr(start, '\n');
    if (!end)
        end = start + strlen(start);
    *p = *end ? end + 1 : end;
    while (start < end && *start == ' ')
        start++;
    while (end > start && end[-1] == ' ')
        end--;
    if (start == end || *start == '#')
        return false;
    line.hot = *start == '!';
    if (line.hot)
        start++;
    const char *bar1 = (const char *)memchr(start, '|', end - start);
    const char *bar2 = bar1 ? (const char *)memchr(bar1 + 1, '|', end - bar1 - 1) : nullptr;
    line.days[0] = line.when[0] = line.what[0] = 0;
    if (!bar1)
    {
        line.cols = 1;
        trimCopy(line.what, sizeof(line.what), start, end);
    }
    else if (!bar2)
    {
        line.cols = 2;
        trimCopy(line.days, sizeof(line.days), start, bar1);
        trimCopy(line.what, sizeof(line.what), bar1 + 1, end);
    }
    else
    {
        line.cols = 3;
        trimCopy(line.days, sizeof(line.days), start, bar1);
        trimCopy(line.when, sizeof(line.when), bar1 + 1, bar2);
        // Any further parts join the text, separated by single spaces; empty ones drop out.
        size_t o = 0;
        for (const char *seg = bar2 + 1; seg <= end;)
        {
            const char *stop = (const char *)memchr(seg, '|', end - seg);
            if (!stop)
                stop = end;
            char part[128];
            trimCopy(part, sizeof(part), seg, stop);
            if (*part)
                o += snprintf(line.what + o, o < sizeof(line.what) ? sizeof(line.what) - o : 0, "%s%s", o ? " " : "", part);
            if (o >= sizeof(line.what))
                break;
            seg = stop + 1;
        }
    }
    return true;
}

// Schedule from SCHEDULE_URL, one screen line per file line (mirrors the designer).
void drawScheduleFile(const Element &el)
{
    const char *msg = nullptr;
    if (!strlen(SCHEDULE_URL))
        msg = "No schedule web address set";
    else if (!haveSchedule)
        msg = scheduleError ? "Couldn't load the schedule" : "Loading schedule...";
    if (msg)
    {
        char t[64];
        snprintf(t, sizeof(t), "%s", msg);
        fitText(el.font, t, sizeof(t), el.width);
        drawText(el.font, t, el.x, el.y, AL_LEFT, el.color);
        return;
    }
    const char *p = scheduleText;
    FileLine line;
    for (int r = 0; r < el.rows && *p;)
    {
        if (!nextFileLine(&p, line))
            continue;
        int y = el.y + r * el.rowHeight;
        uint8_t tc = line.hot ? el.highlightColor : el.timeColor, xc = line.hot ? el.highlightColor : el.color;
        if (line.cols == 1)
        {
            fitText(el.font, line.what, sizeof(line.what), el.width);
            drawText(el.font, line.what, el.x, y, AL_LEFT, xc);
        }
        else
        {
            if (line.cols == 2)
            {
                fitText(el.font, line.days, sizeof(line.days), el.timeWidth - 4);
                drawText(el.font, line.days, el.x, y, AL_LEFT, tc);
            }
            else
            {
                fitText(el.font, line.days, sizeof(line.days), el.daysWidth - 4);
                drawText(el.font, line.days, el.x, y, AL_LEFT, tc);
                fitText(el.font, line.when, sizeof(line.when), el.timeWidth - el.daysWidth - 4);
                drawText(el.font, line.when, el.x + el.daysWidth, y, AL_LEFT, tc);
            }
            fitText(el.font, line.what, sizeof(line.what), el.width - el.timeWidth);
            drawText(el.font, line.what, el.x + el.timeWidth, y, AL_LEFT, xc);
        }
        r++;
    }
}

void drawSchedule(const Element &el, const struct tm &now)
{
    if (el.box)
        fillColor(el.x - el.boxPad, el.y - el.boxPad, el.width + 2 * el.boxPad, el.boxHeight + 2 * el.boxPad,
                  el.boxColor);
    if (SCHEDULE_FROM_URL)
    {
        drawScheduleFile(el);
        return;
    }
    static const char DAY_LETTERS[] = "MTWTFSS";
    int wday = (now.tm_wday + 6) % 7; // 0 = Monday
    int nowMin = now.tm_hour * 60 + now.tm_min;
    char days[12], when[16], what[128], t[16];
    for (int r = 0; r < el.rows && r < SCHEDULE_COUNT; r++)
    {
        const SchedEntry &e = SCHEDULE[r];
        int y = el.y + r * el.rowHeight;

        if (e.days == 0x7F)
            snprintf(days, sizeof(days), "Daily");
        else
        {
            int n = 0;
            for (int d = 0; d < 7; d++)
                if (e.days & (1 << d))
                    days[n++] = DAY_LETTERS[d];
            days[n] = 0;
        }
        if (e.kind == ENTRY_ALL_DAY)
            snprintf(when, sizeof(when), "All day");
        else
            shortTime(when, sizeof(when), e.start);
        if (e.kind == ENTRY_RANGE)
        {
            shortTime(t, sizeof(t), e.end);
            snprintf(what, sizeof(what), "%s until %s", e.text, t);
        }
        else
            snprintf(what, sizeof(what), "%s", e.text);

        bool hot = false;
        if (el.highlight && (e.days & (1 << wday)))
        {
            if (e.kind == ENTRY_ALL_DAY)
                hot = true;
            else if (e.kind == ENTRY_RANGE)
                hot = e.end > e.start ? (e.start <= nowMin && nowMin < e.end)
                                      : (nowMin >= e.start || nowMin < e.end); // runs past midnight
        }
        uint8_t timeInk = hot ? el.highlightColor : el.timeColor;
        drawText(el.font, days, el.x, y, AL_LEFT, timeInk);
        drawText(el.font, when, el.x + el.daysWidth, y, AL_LEFT, timeInk);
        fitText(el.font, what, sizeof(what), el.width - el.timeWidth);
        drawText(el.font, what, el.x + el.timeWidth, y, AL_LEFT, hot ? el.highlightColor : el.color);
    }
}

// The time of day in words for a minute after midnight (DAY_PARTS is sorted;
// before the first start, the last one is still running from the day before).
const char *dayPartAt(int minuteOfDay)
{
    if (DAY_PART_COUNT == 0)
        return "";
    const char *t = DAY_PARTS[DAY_PART_COUNT - 1].text;
    for (int i = 0; i < DAY_PART_COUNT; i++)
        if (DAY_PARTS[i].start <= minuteOfDay)
            t = DAY_PARTS[i].text;
    return t;
}

void clockText(char *buf, size_t n, const struct tm &now)
{
    if (CLOCK_WORDS)
        snprintf(buf, n, "%s", dayPartAt(now.tm_hour * 60 + now.tm_min));
    else
        fmtTime(buf, n, now.tm_hour, now.tm_min, SHOW_AMPM);
}

void drawElement(const Element &el, const struct tm &now)
{
    char buf[160];
    switch (el.type)
    {
    case EL_SCHEDULE:
        drawSchedule(el, now);
        return;
    case EL_CLOCK:
        clockText(buf, sizeof(buf), now);
        break;
    case EL_DATE:
        fill(buf, sizeof(buf), el.text, dateToken, now);
        break;
    case EL_TEMP:
        fill(buf, sizeof(buf), el.text, weatherToken, now);
        break;
    case EL_UPDATED:
        if (lastWeatherUtc)
            fill(buf, sizeof(buf), el.text, updatedToken, now);
        else
            snprintf(buf, sizeof(buf), "%s", el.pending);
        break;
    case EL_BATTERY:
        fill(buf, sizeof(buf), el.text, batteryToken, now);
        break;
    case EL_WIFI:
    {
        // 1-bit icon from layout.h, rows padded to whole bytes.
        const uint8_t *bits = wifiOk ? WIFI_ON_BITS : WIFI_OFF_BITS;
        uint8_t ink = wifiOk ? el.color : el.highlightColor;
        int s = WIFI_ICON_SIZE, stride = (s + 7) / 8;
        for (int j = 0; j < s; j++)
            for (int i = 0; i < s; i++)
                if (bits[j * stride + i / 8] & (0x80 >> (i & 7)))
                    putInk(el.x + i, el.y + j, inkAt(ink, el.x + i, el.y + j));
        return;
    }
    default:
        snprintf(buf, sizeof(buf), "%s", el.text);
    }
    drawText(el.font, buf, el.x, el.y, el.align, el.color);
}

// Draws an 800x600 image from the SD card: 4 bits per pixel (two per byte, high
// nibble first). gray: values are grays 0 (black) - 7 (white), for the 8-gray
// mode; otherwise ink numbers (INK_BLACK / INK_WHITE).
bool drawImageFile(const char *path, bool gray)
{
    if (!display.getSdCardOk() && !display.sdCardInit())
        return false;
    SdFile f;
    if (!f.open(path, O_RDONLY))
        return false;
    static uint8_t row[W / 2];
    for (int y = 0; y < H; y++)
    {
        if (f.read(row, sizeof(row)) != (int)sizeof(row))
            break;
        for (int x = 0; x < W; x += 2)
        {
            uint8_t a = row[x / 2] >> 4, b = row[x / 2] & 15;
            if (gray)
            {
                display.drawPixel(x, y, a & 7);
                display.drawPixel(x + 1, y, b & 7);
            }
            else
            {
                putInk(x, y, a);
                putInk(x + 1, y, b);
            }
        }
    }
    f.close();
    return true;
}

void drawMain(const struct tm &now)
{
    display.clearDisplay();
    if (BG_INK != INK_WHITE)
        fillColor(0, 0, W, H, BG_INK);
    if (BG_IMAGE)
        drawImageFile(BG_IMAGE, false);
    for (int i = 0; i < ELEMENT_COUNT; i++)
        drawElement(ELEMENTS[i], now);
}

// Returns false if the photo couldn't be read, so the caller can fall back.
bool drawPhoto(int index, const struct tm &now)
{
    display.clearDisplay();
    if (GALLERY_FILL != INK_WHITE)
        fillColor(0, 0, W, H, GALLERY_FILL);
    if (!drawImageFile(PHOTOS[index].file, true))
        return false;
    if (OVERLAY_VISIBLE)
        drawElement(OVERLAY, now);
    return true;
}

void drawMessage(const char *line1, const char *line2)
{
    display.selectDisplayMode(INKPLATE_1BIT);
    display.clearDisplay();
    display.setTextColor(BLACK);
    display.setTextSize(3);
    display.setCursor(30, 180);
    display.print(line1);
    display.setTextSize(2);
    display.setCursor(30, 230);
    display.print(line2);
    display.display();
}

// ---------------------------------------------------------------- rotation
// The rotation is worked out from the clock alone: main screen for
// MAIN_MINUTES, then each photo for its minutes, repeating from midnight.
// Returns -1 for the main screen or a photo index, and the local minute where
// that stretch ends.

int screenAt(uint32_t localMinute, uint32_t *segmentEnd)
{
    uint32_t cycle = MAIN_MINUTES;
    for (int i = 0; i < PHOTO_COUNT; i++)
        cycle += PHOTOS[i].minutes;
    if (PHOTO_COUNT == 0 || cycle == 0)
    {
        *segmentEnd = localMinute + 1;
        return -1;
    }
    uint32_t dayStart = localMinute - localMinute % 1440;
    uint32_t pos = (localMinute - dayStart) % cycle;
    uint32_t segStart = localMinute - pos;
    uint32_t end = segStart + MAIN_MINUTES;
    if (pos < MAIN_MINUTES)
    {
        *segmentEnd = end;
        return -1;
    }
    for (int i = 0; i < PHOTO_COUNT; i++)
    {
        uint32_t s = end;
        end += PHOTOS[i].minutes;
        if (localMinute >= s && localMinute < end)
        {
            *segmentEnd = end;
            return i;
        }
    }
    *segmentEnd = localMinute + 1;
    return -1;
}

// ---------------------------------------------------------------- network

bool fetchWeather()
{
    WiFiClientSecure client;
    client.setInsecure(); // no certificate check; fine for public weather data
    HTTPClient http;
    String url = String("https://api.openweathermap.org/data/2.5/weather?zip=") + WEATHER_ZIP + "," +
                 WEATHER_COUNTRY + "&units=" + WEATHER_UNITS + "&appid=" + OPENWEATHER_KEY;
    http.setTimeout(20000);
    if (!http.begin(client, url))
        return false;
    int code = http.GET();
    String body = http.getString();
    http.end();
    if (code != HTTP_CODE_OK)
    {
        Serial.printf("Weather HTTP %d: %s\n", code, body.c_str());
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body))
        return false;

    // Weather also sets the time zone (the zip code's UTC offset, DST included).
    tzOffset = doc["timezone"] | tzOffset;
    JsonObject m = doc["main"];
    wx.temp = lround(m["temp"].as<float>());
    wx.feels = lround((m["feels_like"] | m["temp"].as<float>()));
    wx.hi = lround((m["temp_max"] | m["temp"].as<float>()));
    wx.lo = lround((m["temp_min"] | m["temp"].as<float>()));
    wx.humidity = m["humidity"] | 0;
    wx.wind = lround(doc["wind"]["speed"] | 0.0f);
    strlcpy(wx.desc, doc["weather"][0]["description"] | "", sizeof(wx.desc));
    wx.desc[0] = toupper(wx.desc[0]);
    strlcpy(wx.main, doc["weather"][0]["main"] | "", sizeof(wx.main));
    strlcpy(wx.city, doc["name"] | "", sizeof(wx.city));
    return true;
}

// Collects a download, keeping the first SCHEDULE_MAX_BYTES (HTTPClient handles chunked replies).
struct CappedBuffer : Stream
{
    uint8_t *buf;
    size_t n = 0, cap;
    CappedBuffer(uint8_t *b, size_t c) : buf(b), cap(c) {}
    size_t write(uint8_t c) override
    {
        if (n < cap)
            buf[n++] = c;
        return 1;
    }
    size_t write(const uint8_t *b, size_t len) override
    {
        size_t k = len < cap - n ? len : cap - n;
        memcpy(buf + n, b, k);
        n += k;
        return len;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};

void loadSchedule()
{
    Preferences prefs;
    prefs.begin("inkplate", true);
    haveSchedule = prefs.getBool("schedOk", false);
    size_t n = haveSchedule ? prefs.getBytes("sched", scheduleText, SCHEDULE_MAX_BYTES) : 0;
    scheduleText[n] = 0;
    prefs.end();
}

// Reads SCHEDULE_URL and keeps it (in RAM and flash) if the read worked.
bool fetchSchedule()
{
    WiFiClientSecure secure;
    secure.setInsecure();
    WiFiClient plain;
    bool https = strncmp(SCHEDULE_URL, "https:", 6) == 0;
    HTTPClient http;
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!(https ? http.begin(secure, SCHEDULE_URL) : http.begin(plain, SCHEDULE_URL)))
        return false;
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        Serial.printf("Schedule update failed: HTTP %d\n", code);
        http.end();
        return false;
    }
    static uint8_t raw[SCHEDULE_MAX_BYTES];
    CappedBuffer body(raw, sizeof(raw));
    int got = http.writeToStream(&body);
    http.end();
    if (got < 0 && body.n == 0)
    {
        Serial.printf("Schedule update failed: %s\n", HTTPClient::errorToString(got).c_str());
        return false;
    }
    const uint8_t *start = raw;
    size_t len = body.n;
    if (len >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) // byte order mark
        start += 3, len -= 3;
    size_t n = cleanScheduleText(start, len, scheduleText, SCHEDULE_MAX_BYTES);
    haveSchedule = true;
    Preferences prefs;
    prefs.begin("inkplate", false);
    prefs.putBytes("sched", scheduleText, n);
    prefs.putBool("schedOk", true);
    prefs.end();
    return true;
}

bool scheduleDue(uint32_t slack)
{
    return SCHEDULE_FROM_URL && strlen(SCHEDULE_URL) && display.rtc.getEpoch() + slack >= nextScheduleUtc;
}

// Connects to WiFi; sets the RTC from NTP and fetches weather when that's due (or
// first is set), and reads the schedule file when that's due.
// Returns true if the RTC now holds a good time.
// slack: also count things due within that many seconds as due.
bool goOnline(bool first, uint32_t slack)
{
    if (!strlen(WIFI_SSID))
    {
        wifiOk = false;
        return display.rtc.isSet();
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000)
        delay(100);
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi failed");
        wifiOk = false;
        WiFi.mode(WIFI_OFF);
        uint32_t now = display.rtc.getEpoch();
        if (first || now + slack >= nextFetchUtc)
            nextFetchUtc = now + 5 * 60; // try again in 5 minutes
        if (scheduleDue(slack))
            nextScheduleUtc = now + 5 * 60;
        return display.rtc.isSet();
    }

    wifiOk = true;
    if (first || display.rtc.getEpoch() + slack >= nextFetchUtc)
    {
        // NTP gives UTC; the RTC keeps UTC and the zone offset is applied when drawing.
        configTime(0, 0, "pool.ntp.org");
        t0 = millis();
        while (time(nullptr) < 1700000000 && millis() - t0 < 10000)
            delay(100);
        if (time(nullptr) >= 1700000000)
            display.rtc.setEpoch(time(nullptr));

        uint32_t now = display.rtc.getEpoch();
        nextFetchUtc = now + WEATHER_MINUTES * 60;
        if (strlen(OPENWEATHER_KEY))
        {
            if (fetchWeather())
                lastWeatherUtc = now;
            else
                nextFetchUtc = now + 5 * 60;
        }
    }

    if (scheduleDue(slack))
    {
        // A failed read keeps the last schedule on screen and tries again in 5 minutes.
        bool ok = fetchSchedule();
        scheduleError = !ok;
        nextScheduleUtc = display.rtc.getEpoch() + (ok ? SCHEDULE_REFRESH_MINUTES * 60 : 5 * 60);
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return display.rtc.isSet();
}

// ---------------------------------------------------------------- main

void sleepUntil(uint32_t wakeUtc)
{
    display.sdCardSleep();
    display.rtc.clearAlarmFlag();
    display.rtc.setAlarmEpoch(wakeUtc, RTC_ALARM_MATCH_DHHMMSS);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // RTC alarm pin, active low
    Serial.flush();
    esp_deep_sleep_start();
}

// Words clock: the next local minute (since 1970) when the screen needs redrawing:
// a new time of day, midnight (the date), a schedule entry starting or ending
// today, or weather / the schedule file coming due.
uint32_t nextChangeMinute(uint32_t minute)
{
    uint32_t day = minute - minute % 1440, m = minute % 1440;
    uint32_t best = day + 1440;
    for (int i = 0; i < DAY_PART_COUNT; i++)
        if ((uint32_t)DAY_PARTS[i].start > m && day + DAY_PARTS[i].start < best)
            best = day + DAY_PARTS[i].start;
    if (!SCHEDULE_FROM_URL)
    {
        int wday = (day / 1440 + 3) % 7; // 1970-01-01 was a Thursday; 0 = Monday
        for (int i = 0; i < SCHEDULE_COUNT; i++)
        {
            const SchedEntry &e = SCHEDULE[i];
            if (e.kind != ENTRY_RANGE || !(e.days & (1 << wday)))
                continue;
            if ((uint32_t)e.start > m && day + e.start < best)
                best = day + e.start;
            if ((uint32_t)e.end > m && day + e.end < best)
                best = day + e.end;
        }
    }
    uint32_t due[2] = {nextFetchUtc, SCHEDULE_FROM_URL && strlen(SCHEDULE_URL) ? nextScheduleUtc : 0};
    for (uint32_t u : due)
    {
        uint32_t dm = (uint32_t)(((int64_t)u + tzOffset + 59) / 60);
        if (u && dm > minute && dm < best)
            best = dm;
    }
    return best;
}

uint32_t leadSec(int screen)
{
    return (leadMs[screen >= 0] + 999) / 1000;
}

// Clean pass for quick updates: fills the box around each clock (old and new
// text) with the background ink. Showing that first, then the new time, drives
// every clock pixel twice and leaves far less ghosting. Returns false if no
// clock text changed.
bool blankClocks(const struct tm &before, const struct tm &after)
{
    bool any = false;
    char a[64], b[64];
    clockText(a, sizeof(a), before);
    clockText(b, sizeof(b), after);
    if (!strcmp(a, b))
        return false;
    for (int i = 0; i < ELEMENT_COUNT; i++)
    {
        const Element &el = ELEMENTS[i];
        if (el.type != EL_CLOCK)
            continue;
        int wa = textWidth(el.font, a), wb = textWidth(el.font, b);
        int la = el.x - (el.align == AL_CENTER ? wa / 2 : el.align == AL_RIGHT ? wa : 0);
        int lb = el.x - (el.align == AL_CENTER ? wb / 2 : el.align == AL_RIGHT ? wb : 0);
        int x0 = (la < lb ? la : lb) - 6, x1 = (la + wa > lb + wb ? la + wa : lb + wb) + 6;
        fillColor(x0, el.y - 4, x1 - x0, el.font->ascent + el.font->descent + 8, BG_INK);
        any = true;
    }
    return any;
}

// Draws the screen for a stretch into the display buffer, switching to the
// 8-gray mode for a photo. Returns the screen actually drawn (-1 if the photo
// couldn't be read and the main screen was drawn instead).
int drawScreen(int screen, const struct tm &local)
{
    if (screen >= 0)
    {
        display.selectDisplayMode(INKPLATE_3BIT);
        if (drawPhoto(screen, local))
            return screen;
    }
    display.selectDisplayMode(INKPLATE_1BIT);
    drawMain(local);
    return -1;
}

struct tm localTm(uint32_t minute)
{
    time_t t = (time_t)minute * 60;
    struct tm out;
    gmtime_r(&t, &out);
    return out;
}

void setup()
{
    Serial.begin(115200);
    display.begin();
    display.rtc.clearAlarmFlag();
    if (SCHEDULE_FROM_URL)
        loadSchedule();

    bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    bool onlineFirst = !woke || !display.rtc.isSet();
    if (!woke)
    {
        shownScreen = -2; // power-up or reset: we don't know what's on the panel
        uint64_t mac = ESP.getEfuseMac(); // the WiFi station address; first byte is the lowest
        Serial.printf("WiFi MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", (int)(mac & 0xff), (int)(mac >> 8 & 0xff),
                      (int)(mac >> 16 & 0xff), (int)(mac >> 24 & 0xff), (int)(mac >> 32 & 0xff), (int)(mac >> 40 & 0xff));
    }

    // Power-up: get the time (and weather) before the first draw.
    if (onlineFirst && !goOnline(true, 0))
    {
        drawMessage("No clock time yet", "Check WiFi settings in secrets.h");
        shownScreen = -2;
        esp_sleep_enable_timer_wakeup(5ULL * 60 * 1000000); // retry in 5 minutes
        esp_deep_sleep_start();
    }

    // Show what will be current once the update finishes. The photo lead is
    // the longer one, so use it to look ahead, then settle on the screen.
    uint32_t now = display.rtc.getEpoch();
    uint32_t segEnd;
    uint32_t minute = (now + tzOffset + leadSec(0) + 2) / 60;
    int screen = screenAt(minute, &segEnd);
    if (screen >= 0)
    {
        minute = (now + tzOffset + leadSec(screen) + 2) / 60;
        screen = screenAt(minute, &segEnd);
    }
    struct tm local = localTm(minute);

    // A quick (partial) update needs the main screen both before and after,
    // and a count under FULL_REFRESH_EVERY. Redraw what's on the panel first
    // and hand it to the library as the "before" picture.
    bool quick = woke && shownScreen == -1 && screen < 0 && quickUpdates < FULL_REFRESH_EVERY;
    if (quick)
    {
        display.selectDisplayMode(INKPLATE_1BIT);
        drawMain(localTm(shownMinute));
        display.preloadScreen();
    }
    shownVolts = display.readBattery();
    int shown = drawScreen(screen, local);
    if (quick && shown < 0)
    {
        if (CLEAN_CLOCK && blankClocks(localTm(shownMinute), local))
        {
            display.partialUpdate(true); // everything new, with the clocks blanked
            drawMain(local);
        }
        display.partialUpdate(true);
        quickUpdates++;
    }
    else
    {
        display.display();
        quickUpdates = 0;
    }
    shownScreen = shown;
    shownMinute = minute;

    // Learn how long wake + draw + update takes on clean wakes (no WiFi).
    if (!onlineFirst)
    {
        uint32_t &l = leadMs[shown >= 0];
        l = (l * 3 + millis() + 500) / 4;
        Serial.printf("%s took %lu ms, lead now %lu ms\n", quick && shown < 0 ? "Quick update" : "Full refresh", millis(), l);
    }

    // Weather, NTP and the schedule file happen after the update so they don't
    // delay the clock; then the main screen is quickly updated to show them.
    // (The words clock wakes right at the due time, so allow a little slack.)
    uint32_t slack = CLOCK_WORDS ? 120 : 0;
    if (!onlineFirst && (display.rtc.getEpoch() + slack >= nextFetchUtc || scheduleDue(slack)))
    {
        goOnline(false, slack);
        if (shown < 0)
        {
            display.preloadScreen(); // the buffer still holds what's on the panel
            drawMain(local);
            display.partialUpdate(true);
            quickUpdates++;
        }
    }

    // Next change: on the main screen or a photo with the clock over it, the
    // next clock change (or the end of this stretch, if sooner); otherwise when
    // this photo's time is up.
    bool clockShown = shown < 0 || OVERLAY_VISIBLE;
    uint32_t step = UPDATE_MINUTES ? UPDATE_MINUTES : 1;
    uint32_t nextMinute = segEnd;
    if (clockShown)
    {
        nextMinute = CLOCK_WORDS ? nextChangeMinute(minute) : (minute / step + 1) * step;
        if (PHOTO_COUNT > 0 && segEnd > minute && segEnd < nextMinute)
            nextMinute = segEnd;
    }
    uint32_t dummy;
    int nextScreen = screenAt(nextMinute, &dummy);
    uint32_t wake = nextMinute * 60 - tzOffset - leadSec(nextScreen);
    now = display.rtc.getEpoch();
    while (wake <= now + 2)
        wake += 60;
    sleepUntil(wake);
}

void loop()
{
    // Never reached: the board restarts in setup() after each deep sleep.
}
