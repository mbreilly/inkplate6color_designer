// Inkplate 6COLOR clock: time, date, weather and today's schedule.
// A port of the PyPortal Display Designer program to the 600x448 7-color e-paper.
//
// The screen redraws once a minute. The 6COLOR has no partial refresh, so every
// update is a full refresh that takes several seconds. To make the new minute
// land on time, the board wakes a little *before* each minute boundary (the
// "lead", measured on every wake) and draws the upcoming minute.
//
// Between updates the ESP32 is in deep sleep; the onboard RTC (PCF85063) keeps
// UTC time and wakes it with an alarm on GPIO39. WiFi only comes on every
// WEATHER_MINUTES to fetch weather and re-sync the RTC from NTP.
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

#include "config.h"
#include "src/fonts/FreeSans12pt7b.h"
#include "src/fonts/FreeSans18pt7b.h"
#include "src/fonts/FreeSans9pt7b.h"
#include "src/fonts/FreeSansBold12pt7b.h"
#include "src/fonts/FreeSansBold24pt7b.h"
#include "src/fonts/FreeSansBold48pt7b.h"

Inkplate display;

static const int W = E_INK_WIDTH;  // 600
static const int H = E_INK_HEIGHT; // 448
static const int SCHEDULE_COUNT = sizeof(SCHEDULE) / sizeof(SCHEDULE[0]);
static const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *MONTHS[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};

// ---------------------------------------------------------------- state kept through deep sleep
// RTC memory survives deep sleep but is reset on power-up or the reset button.

RTC_DATA_ATTR int32_t tzOffset = UTC_OFFSET_FALLBACK_SECONDS;
RTC_DATA_ATTR uint32_t nextFetchUtc = 0;
RTC_DATA_ATTR uint32_t lastWeatherUtc = 0; // 0 = never fetched
RTC_DATA_ATTR int weatherTemp = 0;
RTC_DATA_ATTR char weatherDesc[48] = "";
RTC_DATA_ATTR uint32_t leadMs = 15000; // wake -> image fully on screen, refined each wake

// ---------------------------------------------------------------- drawing helpers

enum Align
{
    LEFT,
    CENTER,
    RIGHT
};

int textWidth(const char *text)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return x1 + w;
}

// y is the text baseline. Returns the x where the text ends.
int drawText(int x, int y, Align align, const GFXfont *font, uint8_t color, const char *text)
{
    display.setFont(font);
    display.setTextColor(color);
    int w = textWidth(text);
    if (align == CENTER)
        x -= w / 2;
    else if (align == RIGHT)
        x -= w;
    display.setCursor(x, y);
    display.print(text);
    return x + w;
}

// Shorten text with "..." until it fits in maxWidth (uses the current font).
void fitText(char *text, int maxWidth)
{
    if (textWidth(text) <= maxWidth)
        return;
    int len = strlen(text);
    char buf[96];
    while (len > 0)
    {
        len--;
        while (len > 0 && text[len - 1] == ' ')
            len--;
        snprintf(buf, sizeof(buf), "%.*s...", len, text);
        if (textWidth(buf) <= maxWidth)
            break;
    }
    strcpy(text, buf);
}

void fmtTime(char *out, size_t n, int hour, int minute, bool hour24, bool ampm)
{
    if (hour24)
        snprintf(out, n, "%02d:%02d", hour, minute);
    else
        snprintf(out, n, "%d:%02d%s", (hour % 12) ? hour % 12 : 12, minute, ampm ? (hour < 12 ? " AM" : " PM") : "");
}

struct tm localFromUtc(uint32_t utc)
{
    time_t t = (time_t)utc + tzOffset;
    struct tm lt;
    gmtime_r(&t, &lt);
    return lt;
}

// ---------------------------------------------------------------- screen

void drawScreen(uint32_t shownUtc)
{
    struct tm now = localFromUtc(shownUtc);
    char buf[96];

    display.clearDisplay();
    display.setTextWrap(false);
    if (C_BACKGROUND != INKPLATE_WHITE)
        display.fillRect(0, 0, W, H, C_BACKGROUND);

    // Clock, with AM/PM in a smaller size so the digits can be big.
    if (HOUR_24)
        snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
    else
        snprintf(buf, sizeof(buf), "%d:%02d", (now.tm_hour % 12) ? now.tm_hour % 12 : 12, now.tm_min);
    int end = drawText(18, 92, LEFT, &FreeSansBold48pt7b, C_CLOCK, buf);
    if (!HOUR_24 && SHOW_AMPM)
        drawText(end + 10, 92, LEFT, &FreeSansBold24pt7b, C_CLOCK, now.tm_hour < 12 ? "AM" : "PM");

    // Date
    snprintf(buf, sizeof(buf), "%s, %s %d", WEEKDAYS[now.tm_wday], MONTHS[now.tm_mon], now.tm_mday);
    drawText(22, 136, LEFT, &FreeSans18pt7b, C_DATE, buf);

    // Weather, top right
    if (lastWeatherUtc)
    {
        // The bundled fonts have no degree sign, so draw one.
        const char *unit = strcmp(WEATHER_UNITS, "imperial") == 0 ? "F" : "C";
        display.setFont(&FreeSansBold24pt7b);
        int unitW = textWidth(unit);
        snprintf(buf, sizeof(buf), "%d", weatherTemp);
        int numEnd = drawText(582 - unitW - 16, 62, RIGHT, &FreeSansBold24pt7b, C_TEMP, buf);
        display.drawCircle(numEnd + 7, 32, 5, C_TEMP);
        display.drawCircle(numEnd + 7, 32, 4, C_TEMP);
        drawText(582, 62, RIGHT, &FreeSansBold24pt7b, C_TEMP, unit);

        display.setFont(&FreeSans12pt7b);
        strncpy(buf, weatherDesc, sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
        fitText(buf, 220);
        drawText(582, 96, RIGHT, &FreeSans12pt7b, C_CONDITIONS, buf);
    }
    else if (strlen(OPENWEATHER_KEY))
    {
        drawText(582, 62, RIGHT, &FreeSans12pt7b, C_CONDITIONS, "Weather...");
    }

    display.drawFastHLine(18, 156, W - 36, C_HEADING);
    display.drawFastHLine(18, 157, W - 36, C_HEADING);

    // Schedule
    drawText(22, 188, LEFT, &FreeSansBold12pt7b, C_HEADING, "TODAY");

    int nowMin = now.tm_hour * 60 + now.tm_min;
    int current = -1;
    for (int i = 0; i < SCHEDULE_COUNT; i++)
    {
        int h = 0, m = 0;
        sscanf(SCHEDULE[i].time, "%d:%d", &h, &m);
        if (h * 60 + m <= nowMin)
            current = i;
    }
    const int ROWS = 5, ROW_H = 42, TOP = 200, TEXT_X = 170;
    int start = (SCHEDULE_HIDE_PAST && current > 0) ? current : 0;
    for (int r = 0; r < ROWS && start + r < SCHEDULE_COUNT; r++)
    {
        const ScheduleEntry &e = SCHEDULE[start + r];
        int y = TOP + r * ROW_H;
        int base = y + 29;
        bool hot = SCHEDULE_HIGHLIGHT && start + r == current;
        if (hot)
            display.fillRect(14, y + 2, W - 28, ROW_H - 4, C_HIGHLIGHT);

        int h = 0, m = 0;
        sscanf(e.time, "%d:%d", &h, &m);
        fmtTime(buf, sizeof(buf), h, m, HOUR_24, SHOW_AMPM);
        drawText(24, base, LEFT, &FreeSansBold12pt7b, hot ? INKPLATE_BLACK : C_SCHED_TIME, buf);

        display.setFont(&FreeSans12pt7b);
        strncpy(buf, e.text, sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
        fitText(buf, W - 24 - TEXT_X);
        drawText(TEXT_X, base, LEFT, &FreeSans12pt7b, hot ? INKPLATE_BLACK : C_SCHED_TEXT, buf);
    }

    // Footer
    if (lastWeatherUtc)
    {
        struct tm u = localFromUtc(lastWeatherUtc);
        char t[16];
        fmtTime(t, sizeof(t), u.tm_hour, u.tm_min, HOUR_24, true);
        snprintf(buf, sizeof(buf), "Updated %s", t);
        drawText(582, 436, RIGHT, &FreeSans9pt7b, C_FOOTER, buf);
    }
    if (SHOW_BATTERY)
    {
        snprintf(buf, sizeof(buf), "%.2f V", display.readBattery());
        drawText(18, 436, LEFT, &FreeSans9pt7b, C_FOOTER, buf);
    }
}

void drawMessage(const char *line1, const char *line2)
{
    display.clearDisplay();
    drawText(W / 2, 200, CENTER, &FreeSansBold24pt7b, INKPLATE_BLACK, line1);
    drawText(W / 2, 250, CENTER, &FreeSans12pt7b, INKPLATE_BLUE, line2);
    display.display();
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
    if (code != HTTP_CODE_OK)
    {
        Serial.printf("Weather HTTP %d\n", code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        Serial.printf("Weather JSON: %s\n", err.c_str());
        return false;
    }

    tzOffset = doc["timezone"] | tzOffset;
    weatherTemp = lround(doc["main"]["temp"].as<float>());
    strlcpy(weatherDesc, doc["weather"][0]["description"] | "", sizeof(weatherDesc));
    weatherDesc[0] = toupper(weatherDesc[0]);
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
    configTime(0, 0, NTP_SERVER);
    t0 = millis();
    while (time(nullptr) < 1700000000 && millis() - t0 < 10000)
        delay(100);
    if (time(nullptr) >= 1700000000)
        display.rtc.setEpoch(time(nullptr));

    uint32_t now = display.rtc.getEpoch();
    if (strlen(OPENWEATHER_KEY))
    {
        if (fetchWeather())
        {
            lastWeatherUtc = now;
            nextFetchUtc = now + WEATHER_MINUTES * 60;
        }
        else
        {
            nextFetchUtc = now + 5 * 60;
        }
    }
    else
    {
        nextFetchUtc = now + WEATHER_MINUTES * 60;
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return display.rtc.isSet();
}

// ---------------------------------------------------------------- main

void sleepUntil(uint32_t wakeUtc)
{
    display.rtc.clearAlarmFlag();
    display.rtc.setAlarmEpoch(wakeUtc, RTC_ALARM_MATCH_DHHMMSS);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // RTC alarm pin, active low
    Serial.flush();
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    display.begin();
    display.rtc.clearAlarmFlag();

    bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    bool onlineFirst = !woke || !display.rtc.isSet();

    // Power-up: get the time (and weather) before the first draw.
    if (onlineFirst && !goOnline())
    {
        drawMessage("No clock time yet", "Check WIFI_SSID / WIFI_PASSWORD in config.h");
        esp_sleep_enable_timer_wakeup(5ULL * 60 * 1000000); // retry in 5 minutes
        esp_deep_sleep_start();
    }

    // Draw the minute that will be current by the time the refresh finishes.
    uint32_t leadSec = (leadMs + 999) / 1000;
    uint32_t now = display.rtc.getEpoch();
    uint32_t shown = (now + leadSec + 10) / 60 * 60;

    drawScreen(shown);
    display.display();

    // Learn how long wake + draw + refresh really takes (only on clean wakes,
    // where WiFi didn't add to it), and lead the next minute by that much.
    if (!onlineFirst)
    {
        leadMs = (leadMs * 3 + millis() + 1000) / 4;
        Serial.printf("Refresh took %lu ms, lead now %lu ms\n", millis(), leadMs);
    }

    // Weather and NTP happen after the refresh so they don't delay the clock.
    if (!onlineFirst && display.rtc.getEpoch() >= nextFetchUtc)
        goOnline();

    leadSec = (leadMs + 999) / 1000;
    uint32_t wake = shown + 60 - leadSec;
    now = display.rtc.getEpoch();
    while (wake <= now + 2)
        wake += 60;
    sleepUntil(wake);
}

void loop()
{
    // Never reached: the board restarts in setup() after each deep sleep.
}
