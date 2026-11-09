// Layout made by Inkplate 6COLOR Designer, 10/20/2026, 7:00:00 AM.
// Export again from the designer to change it.
#pragma once

#define HOUR24 false
#define SHOW_AMPM true
// true: the clock shows the time of day in words (DAY_PARTS below) and the screen
// only redraws when something on it changes.
#define CLOCK_WORDS true
#define WEATHER_UNITS "imperial"
static const uint16_t MAIN_MINUTES = 10;
static const uint16_t WEATHER_MINUTES = 10;
// The screen (and clock) is redrawn every UPDATE_MINUTES, on the hour and every N minutes after.
static const uint16_t UPDATE_MINUTES = 5;
static const uint8_t BG_INK = INKPLATE_WHITE;
static const char *const BG_IMAGE = nullptr;
static const uint8_t GALLERY_FILL = INKPLATE_WHITE;

// Schedule source: false = the SCHEDULE entries below, true = a text file read from SCHEDULE_URL.
#define SCHEDULE_FROM_URL false
#define SCHEDULE_URL ""
static const uint16_t SCHEDULE_REFRESH_MINUTES = 15;

// Every color is an 8x8 tile of ink numbers, repeated across the screen.
static const uint8_t PATTERNS[][64] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, // Black
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, // White
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}, // Green
    {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3}, // Blue
    {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4}, // Red
    {5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5}, // Yellow
    {6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6}, // Orange
};

static const Element ELEMENTS[] = {
    {EL_CLOCK, 26, 26, AL_LEFT, &font_sans_44b, INKPLATE_BLACK, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Clock
    {EL_DATE, 26, 120, AL_LEFT, &font_sans_40, INKPLATE_BLUE, "{weekday}, {month} {day}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Date
    {EL_TEMP, 576, 20, AL_RIGHT, &font_oswald_56b, INKPLATE_ORANGE, "{temp}{unit}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Temperature
    {EL_TEMP, 576, 94, AL_RIGHT, &font_sans_24, INKPLATE_BLUE, "{desc}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Conditions
    {EL_TEXT, 28, 180, AL_LEFT, &font_sans_22b, INKPLATE_BLUE, "My week", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Schedule heading
    {EL_SCHEDULE, 28, 216, AL_LEFT, &font_sans_26, INKPLATE_BLACK, "", "",
     INKPLATE_BLUE, INKPLATE_RED, 5, 35, 112, 240, 544, false, INKPLATE_YELLOW, 8, 165, true}, // Schedule
    {EL_TEXT, 24, 418, AL_LEFT, &font_sans_18, INKPLATE_BLUE, "email : mbreilly@hep.upenn.edu", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Email
    {EL_WIFI, 404, 414, AL_LEFT, &font_sans_40b, INKPLATE_BLACK, "", "",
     0, INKPLATE_RED, 0, 0, 0, 0, 24, false, 0, 0, 0, false}, // WiFi status
    {EL_UPDATED, 576, 418, AL_RIGHT, &font_sans_18, INKPLATE_BLACK, "Updated {time}", "Updating...",
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
static const int WIFI_ICON_SIZE = 24;
static const uint8_t WIFI_ON_BITS[] = {
    0x00,0x00,0x00,
    0x03,0xff,0xc0,
    0x1f,0xff,0xf8,
    0x3f,0x81,0xfc,
    0xfc,0x00,0x3f,
    0xf0,0x00,0x0f,
    0xc0,0x00,0x03,
    0x81,0xff,0x81,
    0x07,0xff,0xe0,
    0x1f,0xc3,0xf8,
    0x3e,0x00,0x7c,
    0x38,0x00,0x1c,
    0x30,0x00,0x0c,
    0x00,0xff,0x00,
    0x01,0xff,0x80,
    0x03,0xe7,0xc0,
    0x03,0x81,0xc0,
    0x00,0x00,0x00,
    0x00,0x00,0x00,
    0x00,0x3c,0x00,
    0x00,0x3c,0x00,
    0x00,0x3c,0x00,
    0x00,0x18,0x00,
    0x00,0x00,0x00
};
static const uint8_t WIFI_OFF_BITS[] = {
    0x00,0x00,0x00,
    0x31,0xff,0xc0,
    0x38,0xff,0xf8,
    0x3c,0x01,0xfc,
    0x1e,0x00,0x3f,
    0x8e,0x00,0x0f,
    0xc7,0x00,0x03,
    0x83,0x8f,0x81,
    0x01,0xc7,0xe0,
    0x18,0xe3,0xf8,
    0x3c,0xf0,0x7c,
    0x38,0x78,0x1c,
    0x30,0x3c,0x0c,
    0x00,0x1e,0x00,
    0x01,0x8f,0x00,
    0x03,0xc7,0x00,
    0x03,0x83,0x80,
    0x00,0x01,0xc0,
    0x00,0x00,0xe0,
    0x00,0x3c,0x70,
    0x00,0x3c,0x78,
    0x00,0x3c,0x3c,
    0x00,0x18,0x1c,
    0x00,0x00,0x0c
};

static const bool OVERLAY_VISIBLE = true;
static const Element OVERLAY =
    {EL_CLOCK, 576, 384, AL_RIGHT, &font_sans_40b, INKPLATE_WHITE, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}; // Clock over photos
