// Settings for the Inkplate 6COLOR clock. Everything you normally change lives here.
#pragma once

// ---------------------------------------------------------------- WiFi + weather
// Same values the PyPortal kept in settings.toml.
#define WIFI_SSID        ""
#define WIFI_PASSWORD    ""
#define OPENWEATHER_KEY  ""          // free key from openweathermap.org/api
#define WEATHER_ZIP      "19104"
#define WEATHER_COUNTRY  "US"
#define WEATHER_UNITS    "imperial"  // "imperial" (°F) or "metric" (°C)

// How often to go online for weather and to re-sync the clock.
#define WEATHER_MINUTES  10

// Time zone comes from the weather response (it includes the zip code's UTC
// offset, DST included), exactly like the PyPortal. This offset is only used
// until the first weather fetch succeeds, or if OPENWEATHER_KEY is blank.
#define UTC_OFFSET_FALLBACK_SECONDS (-4 * 3600)

#define NTP_SERVER "pool.ntp.org"

// ---------------------------------------------------------------- time format
#define HOUR_24    false
#define SHOW_AMPM  true

// ---------------------------------------------------------------- schedule
// "HH:MM" in 24-hour time, then the text. Keep them in time order.
struct ScheduleEntry
{
    const char *time;
    const char *text;
};

static const ScheduleEntry SCHEDULE[] = {
    {"07:00", "Breakfast"},
    {"08:15", "School drop-off"},
    {"09:30", "Team standup"},
    {"12:00", "Lunch"},
    {"15:30", "Soccer practice"},
    {"18:00", "Dinner"},
    {"20:30", "Reading time"},
};

#define SCHEDULE_HIDE_PAST  true  // earlier entries drop off as the day goes on
#define SCHEDULE_HIGHLIGHT  true  // current entry gets a yellow bar

// ---------------------------------------------------------------- colors
// The panel has 7 inks: BLACK, WHITE, GREEN, BLUE, RED, YELLOW, ORANGE.
#define C_BACKGROUND  INKPLATE_WHITE
#define C_CLOCK       INKPLATE_BLACK
#define C_DATE        INKPLATE_BLUE
#define C_TEMP        INKPLATE_ORANGE
#define C_CONDITIONS  INKPLATE_BLUE
#define C_HEADING     INKPLATE_GREEN
#define C_SCHED_TIME  INKPLATE_BLUE
#define C_SCHED_TEXT  INKPLATE_BLACK
#define C_HIGHLIGHT   INKPLATE_YELLOW
#define C_FOOTER      INKPLATE_BLACK

#define SHOW_BATTERY  true  // battery voltage in the bottom-left corner
