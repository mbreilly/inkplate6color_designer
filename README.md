# Inkplate 6COLOR Designer

A browser-based layout designer and matching Arduino firmware for the Soldered Inkplate 6COLOR (600 × 448, 7-color e-paper). It's the Inkplate version of the PyPortal Display Designer: a clock, date, weather, a weekly schedule, free text, and a photo gallery slideshow.

## Folders

- `designer/inkplate-designer.html`: the designer. Open it in a browser, no install or server needed.
- `designer/designer.src.html`: the designer source. The firmware is inlined into it at build time.
- `firmware/InkplateDisplay/`: the Arduino sketch the designer exports. `layout.h`, `fonts.h` and `secrets.h` in this folder are an example export.
- `clock/InkplateClock/`: the earlier standalone clock (hand-written layout, no designer).
- `tools/build_designer.py`: rebuilds `designer/inkplate-designer.html` from the source and firmware.
- `tools/pixel-check/`: a host-side harness that runs the firmware drawing code on a PC so it can be compared pixel for pixel with the designer preview.

## Using the designer

1. Open `designer/inkplate-designer.html`.
2. Lay out the screen: drag elements, pick fonts and inks, edit the weekly schedule, and fill in WiFi and OpenWeatherMap settings.
3. In the Gallery tab, add photos. They're fitted to 600 × 448 and dithered to the 7 inks, and the preview shows how they'll look.
4. You can also import a project saved from the PyPortal designer. Positions, fonts, colors and photos are converted.
5. Click Export. The zip has the `InkplateDisplay` sketch folder and an `sdcard` folder. Copy the `sdcard` contents to the root of a FAT32 microSD card.

## Building and uploading the firmware

1. In the Arduino IDE, add this boards manager URL and install **Inkplate Boards**:
   `https://raw.githubusercontent.com/SolderedElectronics/Dasduino-Board-Definitions-for-Arduino-IDE/master/package_Dasduino_Boards_index.json`
2. Install the libraries **InkplateLibrary** (Soldered) and **ArduinoJson** 7.x.
3. Open `InkplateDisplay/InkplateDisplay.ino`, select **Soldered Inkplate 6COLOR**, and upload.

The 6COLOR panel has no partial refresh, so each update is a full refresh of about 12 to 15 seconds. The board deep-sleeps and the onboard RTC wakes it a little before each minute so the new minute shows at about :00. Photos rotate on the schedule set in the Slideshow card.

## Changing the firmware

The designer embeds a copy of the firmware. After editing `firmware/InkplateDisplay/InkplateDisplay.ino` or `display_types.h`, rebuild the designer:

```
python3 tools/build_designer.py
```

## Secrets

`firmware/InkplateDisplay/secrets.h` and `clock/InkplateClock/config.h` are committed with blank WiFi and API key values. Don't commit them once you've filled them in.

Test-compiled against arduino-esp32 2.0.17 with InkplateLibrary 11.2.0 and ArduinoJson 7.4.2. It has not been run on hardware yet.
