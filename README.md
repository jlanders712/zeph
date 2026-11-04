# Heltec V4 custom GPS tracker

Two Heltec WiFi LoRa 32 V4 boards, no Meshtastic.

```
[Tracker V4 + GPS] --LoRa 915 MHz--> [Receiver V4] --BLE--> [Android Chrome: track page + offline map]
```

## 1. Firmware (`heltec_tracker/`)

1. Install Arduino libraries: **RadioLib** (7.x), **TinyGPSPlus**, **NimBLE-Arduino** (2.x).
2. Board: Heltec package -> "WiFi LoRa 32 V4" (package URL:
   `https://resource.heltec.cn/download/package_heltec_esp32_index.json`), or generic
   "ESP32S3 Dev Module" with **USB CDC On Boot: Enabled** (the V4 has no USB-serial chip).
3. In `config.h` set `FEM_REV` (3 for V4.3, 2 for V4.2) and `LORA_FREQ_MHZ` for your board's band.
4. Tracker board: plug the GNSS module into the 8-pin GNSS connector, keep `ROLE_TRACKER`, flash.
5. Receiver board: set `ROLE ROLE_RECEIVER`, flash. No GPS module needed.
6. Always have an antenna attached before transmitting.

Serial Monitor at 115200: the tracker prints `GPS: ...` lines ("no NMEA data" = wrong GPS pins/power, adjust
`GPS_*` in `config.h`); the receiver prints `RX seq=... lat=... lon=...` per packet.

## 2. Phone page (`phone/`)

Without a map file the page draws a plain north-up view (your phone, the tracker, its trail, distance rings).
With map files it draws the same things on a real offline map of Montana.

### 2a. One-time setup on a computer with internet

1. Fill `phone/lib/` with MapLibre and PMTiles, using whichever fits:
   - **Windows:** double-click `get-libs.bat` (a `.sh` file does nothing when double-clicked).
   - **macOS/Linux:** in a terminal, `cd` into `phone/` and run `bash get-libs.sh`.
   - **By hand (any system):** save these three links into `phone/lib/` with exactly these names:
     - https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.js
     - https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.css
     - https://cdn.jsdelivr.net/npm/pmtiles@3/dist/pmtiles.js
2. Host the whole `phone/` folder over HTTPS (GitHub Pages, Netlify, Cloudflare Pages). Do **not** put the
   big `.pmtiles` files in it.
3. On the phone, open the URL once in **Chrome on Android**, then Chrome menu -> "Add to Home screen".
   The page, libraries and fonts are now cached and open with no signal.

### 2b. Make the Montana map files (on a computer)

Get the `pmtiles` command-line tool from <https://github.com/protomaps/go-pmtiles/releases>.
Run extract commands from command prompt in folder directory.

**Base map** (roads, towns, rivers, lakes, land cover, peaks). Copy the latest daily build URL from
<https://maps.protomaps.com/builds>:

```
pmtiles extract <build-url>.pmtiles montana.pmtiles --bbox=-116.05,44.36,-104.04,49.00 --maxzoom=12 --dry-run
```

`--dry-run` only prints the size. Each extra zoom level roughly doubles the file, so drop to `--maxzoom=11`
if it's too big, or go to 13 if you have room. Remove `--dry-run` to write the file.

**Terrain** (hillshade, optional) from Mapterhorn open elevation data:

```
pmtiles extract https://download.mapterhorn.com/planet.pmtiles montana-terrain.pmtiles --bbox=-116.05,44.36,-104.04,49.00 --maxzoom=10 --dry-run

pmtiles extract https://download.mapterhorn.com/6-12-22.pmtiles montana-terrain.pmtiles --bbox=-116.05,44.36,-107.96,49.00 --maxzoom=15 --dry-run
```

### 2c. Put the map on the phone

Copy `montana.pmtiles` (and `montana-terrain.pmtiles`) to the phone (USB, Drive, Bluetooth). In the page tap
**Map**, select both files at once. The page tells vector from elevation files itself, saves them inside the
browser, and from then on they load by themselves, offline. Loading a new file replaces the old one. You can
delete the copies in Downloads afterwards. To remove the saved map, clear the site's data in Chrome.

Buttons: + / - zoom, drag to pan, **Fit** re-centres on you and the tracker, **Clear** wipes the saved trail,
**Demo** simulates a tracker, **Connect** pairs with the receiver (`LoRaTrack-RX`).

### Status of this part

The page logic (file loading, saving to the browser, offline caching, tracker/trail updates) was tested in a
headless browser against stand-in map libraries. **The real MapLibre drawing was not run**, because the
build environment couldn't download it. The map style is hand-written for the Protomaps basemap layer
names, and the terrain source assumes Mapterhorn's Terrarium/512 px format. If the map is blank, or features
are missing, note which and send it back.

## Packet format

16 bytes over LoRa (little-endian): magic `0xA5`, node id, seq (u16), lat/lon (i32, 1e-7 deg),
altitude (i16 m), speed (u8 km/h), sats + fix flag (u8). The receiver appends RSSI (i8) and SNR (i8)
before sending to the phone (18 bytes). Defined in `config.h` and parsed in `handlePacket()`.
