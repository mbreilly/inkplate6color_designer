// WiFi and weather settings. Fill in the blanks before uploading.
#pragma once

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
// Free key from https://openweathermap.org/api (Current Weather). New keys can take an hour to start working.
#define OPENWEATHER_KEY ""
#define WEATHER_ZIP "19104"
#define WEATHER_COUNTRY "US"
// Used only until the first weather fetch tells the device its time zone.
#define UTC_OFFSET_FALLBACK_SECONDS (-14400)
