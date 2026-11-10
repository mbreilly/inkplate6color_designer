// Layout made by Inkplate 6 Designer, 10/20/2026, 7:00:00 AM.
// Export again from the designer to change it.
#pragma once

#define HOUR24 false
#define SHOW_AMPM true
// true: the clock shows the time of day in words (DAY_PARTS below) and the screen
// only redraws when something on it changes.
#define CLOCK_WORDS false
#define WEATHER_UNITS "imperial"
static const uint16_t MAIN_MINUTES = 10;
static const uint16_t WEATHER_MINUTES = 10;
// The screen (and clock) is redrawn every UPDATE_MINUTES, on the hour and every N minutes after.
static const uint16_t UPDATE_MINUTES = 1;
// Quick (partial) updates between full refreshes, which clear ghosting.
static const uint16_t FULL_REFRESH_EVERY = 10;
// true: on each quick update, blank the clock area first, then draw the new time.
#define CLEAN_CLOCK true
static const uint8_t BG_INK = INK_WHITE;
static const char *const BG_IMAGE = nullptr;
static const uint8_t GALLERY_FILL = INK_WHITE;

// Schedule source: false = the SCHEDULE entries below, true = a text file read from SCHEDULE_URL.
#define SCHEDULE_FROM_URL false
#define SCHEDULE_URL ""
static const uint16_t SCHEDULE_REFRESH_MINUTES = 15;

// Every color is an 8x8 tile of ink numbers, repeated across the screen.
static const uint8_t PATTERNS[][64] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, // Black
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, // White
};

static const Element ELEMENTS[] = {
    {EL_CLOCK, 30, 22, AL_LEFT, &font_orbitron_96b, INK_BLACK, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Clock
    {EL_DATE, 34, 150, AL_LEFT, &font_sans_53, INK_BLACK, "{weekday}, {month} {day}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Date
    {EL_TEMP, 768, 27, AL_RIGHT, &font_oswald_75b, INK_BLACK, "{temp}{unit}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Temperature
    {EL_TEMP, 768, 126, AL_RIGHT, &font_sans_32, INK_BLACK, "{desc}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Conditions
    {EL_TEXT, 37, 236, AL_LEFT, &font_sans_29b, INK_BLACK, "My week", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Schedule heading
    {EL_SCHEDULE, 37, 284, AL_LEFT, &font_sans_35, INK_BLACK, "", "",
     INK_BLACK, INK_BLACK, 5, 46, 149, 320, 725, false, INK_BLACK, 11, 217, true}, // Schedule
    {EL_TEXT, 32, 560, AL_LEFT, &font_sans_24, INK_BLACK, "email : mbreilly@hep.upenn.edu", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Email
    {EL_WIFI, 539, 554, AL_LEFT, &font_orbitron_96b, INK_BLACK, "", "",
     0, INK_BLACK, 0, 0, 0, 0, 32, false, 0, 0, 0, false}, // WiFi status
    {EL_UPDATED, 768, 560, AL_RIGHT, &font_sans_24, INK_BLACK, "Updated {time}", "Updating...",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Last updated
};
static const int ELEMENT_COUNT = 9;

static const SchedEntry SCHEDULE[] = {
    {ENTRY_RANGE, 400, 820, 0x0f, "In DRL"},
    {ENTRY_ALL_DAY, 0, 0, 0x10, "Work from home"},
    {ENTRY_RANGE, 960, 1050, 0x0a, "Soccer practice"},
    {ENTRY_AT, 600, 0, 0x20, "Farmers market"},
};
static const int SCHEDULE_COUNT = 4;

static const Photo PHOTOS[] = {
    {"/gallery/photo01.bin", 2},
    {"/gallery/photo02.bin", 2},
};
static const int PHOTO_COUNT = 2;

// Times of day for CLOCK_WORDS: minutes after midnight it starts, name. Sorted by start.
static const DayPart DAY_PARTS[] = {
    {300, "Early morning"},
    {480, "Morning"},
    {720, "Midday"},
    {780, "Afternoon"},
    {1020, "Evening"},
    {1200, "Night"},
    {1380, "Late night"},
};
static const int DAY_PART_COUNT = 7;

// WiFi icon bitmaps (1 bit per pixel, rows of WIFI_ICON_SIZE bits, high bit first).
static const int WIFI_ICON_SIZE = 32;
static const uint8_t WIFI_ON_BITS[] = {
    0x00,0x00,0x00,0x00,
    0x00,0x3f,0xfc,0x00,
    0x03,0xff,0xff,0xc0,
    0x0f,0xff,0xff,0xf0,
    0x3f,0xfc,0x3f,0xfc,
    0x7f,0x00,0x00,0xfe,
    0xfc,0x00,0x00,0x3f,
    0xf0,0x00,0x00,0x0f,
    0xe0,0x00,0x00,0x07,
    0x80,0x1f,0xf8,0x01,
    0x00,0xff,0xff,0x00,
    0x03,0xff,0xff,0xc0,
    0x07,0xf8,0x1f,0xe0,
    0x0f,0xc0,0x03,0xf0,
    0x1f,0x00,0x00,0xf8,
    0x3e,0x00,0x00,0x7c,
    0x1c,0x00,0x00,0x38,
    0x00,0x0f,0xf0,0x00,
    0x00,0x3f,0xfc,0x00,
    0x00,0x7f,0xfe,0x00,
    0x00,0xfe,0x7f,0x00,
    0x00,0xf0,0x0f,0x00,
    0x00,0x60,0x06,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x03,0xc0,0x00,
    0x00,0x07,0xe0,0x00,
    0x00,0x07,0xe0,0x00,
    0x00,0x07,0xe0,0x00,
    0x00,0x03,0xc0,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00
};
static const uint8_t WIFI_OFF_BITS[] = {
    0x00,0x00,0x00,0x00,
    0x10,0x3f,0xfc,0x00,
    0x3c,0x7f,0xff,0xc0,
    0x3c,0x3f,0xff,0xf0,
    0x1e,0x1c,0x3f,0xfc,
    0x1f,0x00,0x00,0xfe,
    0x8f,0x80,0x00,0x3f,
    0xc7,0xc0,0x00,0x0f,
    0xc3,0xe0,0x00,0x07,
    0x81,0xf0,0xf8,0x01,
    0x00,0xf8,0x7f,0x00,
    0x00,0x7c,0x7f,0xc0,
    0x04,0x3e,0x1f,0xe0,
    0x0e,0x1e,0x03,0xf0,
    0x1f,0x0f,0x00,0xf8,
    0x3e,0x07,0x80,0x7c,
    0x1c,0x07,0xc0,0x38,
    0x00,0x03,0xe0,0x00,
    0x00,0x31,0xf0,0x00,
    0x00,0x70,0xf8,0x00,
    0x00,0xf8,0x7c,0x00,
    0x00,0xf0,0x3e,0x00,
    0x00,0x60,0x1f,0x00,
    0x00,0x00,0x0f,0x80,
    0x00,0x00,0x07,0x80,
    0x00,0x03,0xc3,0xc0,
    0x00,0x07,0xe1,0xe0,
    0x00,0x07,0xe1,0xf0,
    0x00,0x07,0xe0,0xf8,
    0x00,0x03,0xc0,0x7c,
    0x00,0x00,0x00,0x3c,
    0x00,0x00,0x00,0x18
};

static const bool OVERLAY_VISIBLE = false;
static const Element OVERLAY =
    {EL_CLOCK, 768, 514, AL_RIGHT, &font_orbitron_96b, INK_WHITE, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}; // Clock over photos
