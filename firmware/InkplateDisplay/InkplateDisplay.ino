// Inkplate 6COLOR display: clock, weather, schedule and photo gallery.
// Made by Inkplate 6COLOR Designer. Layout, colors, fonts and timing live in
// layout.h and fonts.h; WiFi and the weather key live in secrets.h. Photos are
// read from the microSD card (copy the sdcard/ folder onto it).
//
// The panel has no partial refresh: every change is a full refresh that takes
// several seconds. The board sleeps between updates and wakes just *before*
// each change (by the "lead", measured on every wake) so the new minute or
// photo lands on time. The onboard RTC keeps UTC and wakes it via GPIO39.
//
// Board: Tools -> Board -> Inkplate Boards -> "Soldered Inkplate 6COLOR"
// Libraries: InkplateLibrary (Soldered), ArduinoJson 7.x

#ifndef ARDUINO_INKPLATECOLOR
#error "Select Soldered Inkplate 6COLOR in Tools -> Board."
#endif

#include "Inkplate.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "display_types.h"
#include "fonts.h"
#include "layout.h"
#include "secrets.h"

Inkplate display;

static const int W = E_INK_WIDTH;  // 600
static const int H = E_INK_HEIGHT; // 448
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
RTC_DATA_ATTR uint32_t leadMs[2] = {15000, 17000}; // wake -> refresh done, for [main screen, photo]

// ---------------------------------------------------------------- text

// Colors are numbers into PATTERNS (layout.h): an 8x8 tile of inks, repeated
// from the screen's top-left. 0-6 are the solid inks, mixed colors follow.
inline uint8_t inkAt(uint8_t color, int x, int y)
{
    return PATTERNS[color][(y & 7) * 8 + (x & 7)];
}

void fillColor(int x, int y, int w, int h, uint8_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            display.drawPixel(xx, yy, inkAt(color, xx, yy));
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
                        display.drawPixel(px, py, inkAt(color, px, py));
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
    snprintf(v, n, "%.2f", display.readBattery());
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

void drawSchedule(const Element &el, const struct tm &now)
{
    if (el.box)
        fillColor(el.x - el.boxPad, el.y - el.boxPad, el.width + 2 * el.boxPad, el.boxHeight + 2 * el.boxPad,
                  el.boxColor);
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

void drawElement(const Element &el, const struct tm &now)
{
    char buf[160];
    switch (el.type)
    {
    case EL_SCHEDULE:
        drawSchedule(el, now);
        return;
    case EL_CLOCK:
        fmtTime(buf, sizeof(buf), now.tm_hour, now.tm_min, SHOW_AMPM);
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
    default:
        snprintf(buf, sizeof(buf), "%s", el.text);
    }
    drawText(el.font, buf, el.x, el.y, el.align, el.color);
}

// Draws a 600x448 image from the SD card: 4 bits per pixel (two per byte, high
// nibble first), values are the panel's ink numbers 0-6.
bool drawImageFile(const char *path)
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
            if (a < 7)
                display.drawPixel(x, y, a);
            if (b < 7)
                display.drawPixel(x + 1, y, b);
        }
    }
    f.close();
    return true;
}

void drawMain(const struct tm &now)
{
    display.clearDisplay();
    if (BG_INK != INKPLATE_WHITE)
        fillColor(0, 0, W, H, BG_INK);
    if (BG_IMAGE)
        drawImageFile(BG_IMAGE);
    for (int i = 0; i < ELEMENT_COUNT; i++)
        drawElement(ELEMENTS[i], now);
}

// Returns false if the photo couldn't be read, so the caller can fall back.
bool drawPhoto(int index, const struct tm &now)
{
    display.clearDisplay();
    if (GALLERY_FILL != INKPLATE_WHITE)
        fillColor(0, 0, W, H, GALLERY_FILL);
    if (!drawImageFile(PHOTOS[index].file))
        return false;
    if (OVERLAY_VISIBLE)
        drawElement(OVERLAY, now);
    return true;
}

void drawMessage(const char *line1, const char *line2)
{
    display.clearDisplay();
    display.setTextColor(INKPLATE_BLACK);
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

// Connects to WiFi, sets the RTC from NTP and fetches weather.
// Returns true if the RTC now holds a good time.
bool goOnline()
{
    if (!strlen(WIFI_SSID))
        return display.rtc.isSet();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000)
        delay(100);
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi failed");
        WiFi.mode(WIFI_OFF);
        nextFetchUtc = display.rtc.getEpoch() + 5 * 60; // try again in 5 minutes
        return display.rtc.isSet();
    }

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

uint32_t leadSec(int screen)
{
    return (leadMs[screen >= 0] + 999) / 1000;
}

void setup()
{
    Serial.begin(115200);
    display.begin();
    display.rtc.clearAlarmFlag();

    bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    bool onlineFirst = !woke || !display.rtc.isSet();
    if (!woke)
    {
        uint64_t mac = ESP.getEfuseMac(); // the WiFi station address; first byte is the lowest
        Serial.printf("WiFi MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", (int)(mac & 0xff), (int)(mac >> 8 & 0xff),
                      (int)(mac >> 16 & 0xff), (int)(mac >> 24 & 0xff), (int)(mac >> 32 & 0xff), (int)(mac >> 40 & 0xff));
    }

    // Power-up: get the time (and weather) before the first draw.
    if (onlineFirst && !goOnline())
    {
        drawMessage("No clock time yet", "Check WiFi settings in secrets.h");
        esp_sleep_enable_timer_wakeup(5ULL * 60 * 1000000); // retry in 5 minutes
        esp_deep_sleep_start();
    }

    // Show what will be current once the refresh finishes. The photo lead is
    // the longer one, so use it to look ahead, then settle on the screen.
    uint32_t now = display.rtc.getEpoch();
    uint32_t segEnd;
    uint32_t minute = (now + tzOffset + leadSec(0) + 10) / 60;
    int screen = screenAt(minute, &segEnd);
    if (screen >= 0)
    {
        minute = (now + tzOffset + leadSec(screen) + 10) / 60;
        screen = screenAt(minute, &segEnd);
    }

    time_t localT = (time_t)minute * 60;
    struct tm local;
    gmtime_r(&localT, &local);
    if (screen < 0 || !drawPhoto(screen, local))
    {
        screen = -1;
        drawMain(local);
    }
    display.display();

    // Learn how long wake + draw + refresh takes on clean wakes (no WiFi).
    if (!onlineFirst)
    {
        uint32_t &l = leadMs[screen >= 0];
        l = (l * 3 + millis() + 1000) / 4;
        Serial.printf("Refresh took %lu ms, lead now %lu ms\n", millis(), l);
    }

    // Weather and NTP happen after the refresh so they don't delay the clock.
    if (!onlineFirst && display.rtc.getEpoch() >= nextFetchUtc)
        goOnline();

    // Next change: every minute on the main screen or a photo with the clock
    // over it; otherwise when this photo's time is up.
    bool everyMinute = screen < 0 || OVERLAY_VISIBLE;
    uint32_t nextMinute = everyMinute ? minute + 1 : segEnd;
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
