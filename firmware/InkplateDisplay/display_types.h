// Types shared by the generated layout.h and fonts.h. Made by Inkplate 6COLOR Designer.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Looks up one {token} for fill(). Declared here rather than in the .ino because
// the Arduino build puts function prototypes above anything typedef'd in the sketch.
typedef bool (*Lookup)(const char *name, char *out, size_t n, const struct tm &now);

// One glyph of a designer font. Pixels are packed row by row, MSB first,
// starting at byte `offset` of the font's bitmap. The bottom row sits `yoff`
// pixels above the baseline, the left column `xoff` pixels right of the pen.
struct DGlyph
{
    uint32_t offset;
    uint16_t w, h;
    int16_t xoff, yoff, adv;
};

// Character codes 32..127; 127 is the degree sign.
struct DFont
{
    const uint8_t *bits;
    const DGlyph *glyphs;
    int16_t ascent, descent;
};

enum ElementType
{
    EL_CLOCK,
    EL_DATE,
    EL_TEMP,
    EL_UPDATED,
    EL_TEXT,
    EL_SCHEDULE,
    EL_BATTERY
};

enum TextAlign
{
    AL_LEFT,
    AL_CENTER,
    AL_RIGHT
};

// x, y is the top-left of the text's ascent (or the anchor point for center/right).
struct Element
{
    uint8_t type;
    int16_t x, y;
    uint8_t align;
    const DFont *font;
    uint8_t color;       // colors here and below are numbers into PATTERNS in layout.h
    const char *text;    // label text, or the format for date / weather / updated / battery
    const char *pending; // "updated" before the first weather fetch
    // schedule only: one line per entry, days at x, time at x + daysWidth, text at x + timeWidth
    uint8_t timeColor, highlightColor, rows;
    int16_t rowHeight, daysWidth, timeWidth, width;
    bool box;
    uint8_t boxColor;
    int16_t boxPad, boxHeight;
    bool highlight;
};

enum EntryKind
{
    ENTRY_AT,     // at a time
    ENTRY_RANGE,  // from a time until an end time
    ENTRY_ALL_DAY
};

struct SchedEntry
{
    uint8_t kind;
    int16_t start, end; // minutes after midnight
    uint8_t days;       // bit 0 = Monday ... bit 6 = Sunday
    const char *text;
};

struct Photo
{
    const char *file; // on the microSD card
    uint16_t minutes;
};

// One line of a schedule file read from SCHEDULE_URL. "|" splits it into days, time
// and text; with two parts the first goes under the days and times; with none it runs
// the full width. "!" in front means the happening-now color, "#" in front leaves the
// line out. Declared here for the same reason as Lookup.
struct FileLine
{
    char days[48], when[48], what[128];
    uint8_t cols; // 1, 2 or 3
    bool hot;
};
