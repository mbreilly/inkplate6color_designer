# Inkplate 6COLOR Designer

A browser-based layout designer and matching Arduino firmware for the Soldered Inkplate 6COLOR (600 × 448, 7-color e-paper). It's the Inkplate version of the PyPortal Display Designer: a clock, date, weather, a weekly schedule, free text, and a photo gallery slideshow.

The schedule can be typed into the designer or read from a plain-text file at a web address (Schedule tab → Read it from a web address), the same as the PyPortal version. The Inkplate checks the file on the interval you set and keeps the last good copy in flash.

The 6COLOR panel can only redraw the whole screen (a 12 to 15 second refresh with flashing), so the clock is redrawn every few minutes rather than every minute: Weather tab → Time format → Redraw the clock every (1 to 30 minutes, 5 by default). The clock can also show the time of day in words instead (Early morning, Morning, Midday, Afternoon, Evening, Night, Late night, all editable). Then the screen only redraws when the words change, at midnight, when a schedule entry starts or ends, and when weather or the schedule is fetched. A WiFi status icon (bottom row by default) shows whether the board's last connection worked; it turns into a slashed icon in its own ink when it didn't.

## Inkplate 6 (black and white) version

`designer/inkplate6-designer.html` is the same designer for the Soldered Inkplate 6 (800 × 600, black and white), exporting the `Inkplate6Display` sketch. That panel can change just the pixels that differ, so the clock ticks every minute with a quick partial update (about half a second, no flashing). Before each new time it blinks the clock area clean (two extra quick updates: opposite ink, then background), and every so often (10 quick updates by default, set in Clock & Weather → Time format) it does one full refresh to clear ghosting. Light text on a dark background ghosts the most; black on white the least. Grays are drawn as dot patterns (light gray, gray, dark gray), and gallery photos use the panel's 8-gray mode, which only does full refreshes. In Arduino, pick **Soldered Inkplate6**.

Partial updates and deep sleep: sleeping wipes the library's record of what's on the panel, so on each wake the sketch redraws the previous minute's screen from what it kept in RTC memory, passes it to `preloadScreen()`, then draws the new one and calls `partialUpdate(true)`.

## Folders

- `designer/inkplate-designer.html`: the designer. Open it in a browser, no install or server needed.
- `designer/designer.src.html`: the designer source. The firmware is inlined into it at build time.
- `designer/inkplate6-designer.html`: the designer for the black-and-white Inkplate 6.
- `firmware/InkplateDisplay/`: the Arduino sketch the 6COLOR designer exports. `layout.h`, `fonts.h` and `secrets.h` in this folder are an example export.
- `firmware/Inkplate6Display/`: the same for the Inkplate 6.
- `clock/InkplateClock/`: the earlier standalone clock (hand-written layout, no designer).
- `tools/build_designer.py`: rebuilds both designers from the one source and each board's firmware.
- `tools/pixel-check/`: a host-side harness (`extract.py <sketch>`, then build `harness.cpp` with `-I firmware/<sketch>`, plus `-DBW` for the Inkplate 6) that runs the firmware drawing code on a PC so it can be compared pixel for pixel with the designer preview.

## Using the designer

1. Open `designer/inkplate-designer.html`.
2. Lay out the screen: drag elements, pick fonts and inks, edit the weekly schedule, and fill in WiFi and OpenWeatherMap settings.
   Under the seven inks, every color setting also has eight mixed colors (light blue, purple, pink, gold, olive, brown, navy and grey). The panel has only seven inks, so each mix is drawn as an even 50/50 dot pattern of two inks. They look best on large areas like the background or the schedule panel; small text in a mixed color looks speckled.
3. In the Gallery tab, add photos. They're fitted to 600 × 448 and dithered to the 7 inks, and the preview shows how they'll look.
4. You can also import a project saved from the PyPortal designer. Positions, fonts, colors and photos are converted.
5. Click Export. The zip has the `InkplateDisplay` sketch folder and an `sdcard` folder. Copy the `sdcard` contents to the root of a FAT32 microSD card.

## Building and uploading the firmware

1. In the Arduino IDE, add this boards manager URL and install **Inkplate Boards**:
   `https://raw.githubusercontent.com/SolderedElectronics/Dasduino-Board-Definitions-for-Arduino-IDE/master/package_Dasduino_Boards_index.json`
2. Install the libraries **InkplateLibrary** (Soldered) and **ArduinoJson** 7.x.
3. Open `InkplateDisplay/InkplateDisplay.ino`, select **Soldered Inkplate 6COLOR**, and upload.

If your network needs the board's WiFi MAC address, it's on the `MAC:` line of the upload output, and the sketch also prints it to Serial Monitor (115200) on power-up.

The 6COLOR panel has no partial refresh, so each update is a full refresh of about 12 to 15 seconds. The board deep-sleeps and the onboard RTC wakes it a little before each minute so the new minute shows at about :00. Photos rotate on the schedule set in the Slideshow card.

## Changing the firmware

The designer embeds a copy of the firmware. After editing either sketch's `.ino` or `display_types.h`, or `designer/designer.src.html`, rebuild the designers:

```
python3 tools/build_designer.py
```

## Secrets

`firmware/InkplateDisplay/secrets.h`, `firmware/Inkplate6Display/secrets.h` and `clock/InkplateClock/config.h` are committed with blank WiFi and API key values. Don't commit them once you've filled them in.

Test-compiled against arduino-esp32 2.0.17 with InkplateLibrary 11.2.0 and ArduinoJson 7.4.2. It has not been run on hardware yet.
