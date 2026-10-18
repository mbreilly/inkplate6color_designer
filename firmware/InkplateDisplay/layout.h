// Layout made by Inkplate 6COLOR Designer, 10/20/2026, 7:00:24 AM.
// Export again from the designer to change it.
#pragma once

#define HOUR24 false
#define SHOW_AMPM true
#define WEATHER_UNITS "imperial"
static const uint16_t MAIN_MINUTES = 10;
static const uint16_t WEATHER_MINUTES = 10;
static const uint8_t BG_INK = INKPLATE_WHITE;
static const char *const BG_IMAGE = nullptr;
static const uint8_t GALLERY_FILL = INKPLATE_WHITE;

static const Element ELEMENTS[] = {
    {EL_CLOCK, 24, 18, AL_LEFT, &font_orbitron_74b, INKPLATE_BLACK, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Clock
    {EL_DATE, 28, 110, AL_LEFT, &font_sans_28, INKPLATE_BLUE, "{weekday}, {month} {day}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Date
    {EL_TEMP, 576, 20, AL_RIGHT, &font_oswald_56b, INKPLATE_ORANGE, "{temp}{unit}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Temperature
    {EL_TEMP, 576, 94, AL_RIGHT, &font_sans_24, INKPLATE_BLUE, "{desc}", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Conditions
    {EL_SCHEDULE, 28, 172, AL_LEFT, &font_sans_26, INKPLATE_BLACK, "", "",
     INKPLATE_BLUE, INKPLATE_RED, 5, 35, 112, 240, 544, false, INKPLATE_YELLOW, 8, 165, true}, // Schedule
    {EL_TEXT, 24, 418, AL_LEFT, &font_sans_18, INKPLATE_BLUE, "email : mbreilly@hep.upenn.edu", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Email
    {EL_UPDATED, 576, 418, AL_RIGHT, &font_sans_18, INKPLATE_BLACK, "Updated {time}", "Updating...",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}, // Last updated
};
static const int ELEMENT_COUNT = 7;

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

static const bool OVERLAY_VISIBLE = true;
static const Element OVERLAY =
    {EL_CLOCK, 576, 384, AL_RIGHT, &font_sans_40b, INKPLATE_WHITE, "", "",
     0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false}; // Clock over photos
