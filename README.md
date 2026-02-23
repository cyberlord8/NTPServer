
# Pico W GPS NTP Server (NMEA-based Stratum-1 Style)

A **GPS-based NTP server** for the **Raspberry Pi Pico W (RP2040)** built with the **Pico SDK (C/C++)**.

This implementation uses **GPS/NMEA time** (RMC + date) to seed an internal timebase and serves NTP on UDP/123 over **Pico W Wi-Fi**. PPS discipline is planned (the code already reserves a `Locked` state for it).

---

> **Note (AI-Generated README):** This `README.md` was generated with AI assistance. It may contain mistakes, outdated assumptions, or wording that’s overly enthusiastic. Treat it as a starting point and verify details (pin mappings, build steps, features) against the actual code and hardware.
>
> Also note, this note about AI generated content was also AI generated. This line is human generated though.

---

## What the Current Code Does (as of latest sources)

### Networking / NTP
- Initializes **Wi-Fi STA mode** and connects using credentials from `wifi_secrets.h`.
- Configures **static IPv4** by default in `main.cpp`:
  - IP: **192.168.0.123**
  - Netmask: **255.255.255.0**
  - Gateway: **192.168.0.1**
  - DNS (optional): **192.168.0.200**
- Starts an **NTP server** on **UDP port 123** when Wi-Fi connect succeeds.
- NTP reply behavior (`ntp_server.cpp`):
  - Responds only to **client mode (3)** requests.
  - `stratum` is set to:
    - **1** if `timebase_have_time()` and `timebase_is_synced()` are true
    - **2** if time is available but not “synced”
    - **16** if time is not available
  - `LI` (leap indicator) is **0** when synced, **3 (alarm/unsynchronized)** otherwise.
  - `ref_id` is `"GPS\0"`.

### GPS / Timebase
- GPS input is read from **UART0** at **9600 baud**.
- Default UART pin mapping (from `main.cpp`):
  - `GpsUart::init(9600, /*rx_gpio=*/1, /*tx_gpio=*/0);`
- NMEA parsing (`gps_state.cpp`):
  - Supports: **RMC**, **GGA**, **ZDA**
  - Uses **RMC** (time/date + status `A`) to compute Unix UTC seconds and feed `timebase_on_gps_utc_unix()`.
  - Uses **GGA** to determine if a fix exists and to populate sats/HDOP.
- “Acquired” state definition (pre-PPS):
  - `Acquired` requires `gps.rmc_valid == true` AND `gps.gga_fix == true`
  - Otherwise the device is `Acquiring`

### Console UI
- ANSI “single-screen” dashboard (`ui_console.cpp`) refreshed every **500 ms**.
- Shows:
  - GPS state + RMC/GGA validity
  - sats/HDOP
  - last ZDA time string + last RMC time field
  - CPU temperature (smoothed EMA)
  - uptime
  - Wi-Fi link + IP
  - whether NTP server is running (`n_status`)

### LED Behavior
- LED is driven by a repeating timer at **50 ms** (`add_repeating_timer_ms(50, pulse_cb, ...)`).
- Timer callback computes desired LED state only; the main loop applies it via `led_service()` (safe for CYW43 GPIO).
- Pattern varies by GPS state (Booting/Acquiring/Acquired/Locked/Error).

---

## Hardware

### Target
- **Raspberry Pi Pico W (RP2040)**

### GPS Module
- Waveshare Pico GPS L76X (or any GPS that outputs NMEA at 9600 baud)

### Wiring (default pins in code)
- GPS **TX → Pico GPIO1** (UART0 RX)
- GPS **RX → Pico GPIO0** (UART0 TX) *(optional unless configuring GPS)*
- GPS **GND → Pico GND**
- GPS **VCC → Pico 3V3** *(verify module requirements)*

---

## Repository Layout (based on current code)

- `main.cpp` — boot, Wi-Fi config/connect, start NTP server, main loop
- `gps_uart.{h,cpp}` — UART0 RX ISR + ring buffer + line extraction
- `gps_state.{h,cpp}` — NMEA parsing + GPS state machine + status globals
- `timebase.{h,cpp}` — UTC Unix baseline + NTP timestamp conversion
- `ntp_server.{h,cpp}` — lwIP UDP NTP server on port 123
- `wifi_cfg.{h,cpp}` — CYW43 Wi-Fi init/connect + static IP support + status
- `ui_console.{h,cpp}` — ANSI dashboard renderer
- `led.{h,cpp}` — LED patterns + timer callback + `led_service()`
- `temp.{h,cpp}` — temperature read + EMA smoothing
- `uptime.{h,cpp}` — uptime formatting
- `lwipopts.h` — lwIP options

---

## Build Instructions

### Prereqs
- Pico SDK configured and working (CMake + Ninja)
- Target board: `PICO_BOARD=pico_w`

### Configure / Build
```bash
mkdir -p build
cd build
cmake -G Ninja .. -DPICO_BOARD=pico_w
ninja
```

### Flash

Put the Pico W into BOOTSEL mode and copy the generated UF2 from `build/` to the mass storage device.

---

## Wi-Fi Credentials (`wifi_secrets.h`)

`main.cpp` includes `wifi_secrets.h` and expects `ssid` and `pass`.

Create a **local-only** file (do not commit) at your project include path:

```cpp
// wifi_secrets.h
#pragma once

// Use inline constexpr to avoid multiple-definition issues.
inline constexpr const char ssid[] = "YOUR_WIFI_SSID";
inline constexpr const char pass[] = "YOUR_WIFI_PASSWORD";
```

Add it to `.gitignore`:

```gitignore
wifi_secrets.h
```

---

## Running / Console

The firmware uses USB CDC stdio and (when `PICO_STDIO_USB` is defined) will wait until a terminal connects.

Example on Linux:

```bash
picocom -b 115200 /dev/ttyACM0
```

You should see the ANSI dashboard refresh about twice per second.

---

## Using It as an NTP Server

Once connected to Wi-Fi, the Pico W will listen on **UDP/123**.

* Default static IP in code: **192.168.0.123**
* Verify on the dashboard (it prints the IP when assigned).

### Linux (chrony)

Add:

```conf
server 192.168.0.123 iburst
```

### Windows

```powershell
w32tm /config /manualpeerlist:"192.168.0.123" /syncfromflags:manual /update
w32tm /resync
```

---

## Current “Stratum-1” Meaning (Important)

This build reports **stratum 1** when the internal timebase reports “synced”. Right now, `timebase_on_gps_utc_unix()` sets `g_synced = true` when valid RMC time/date is received (no PPS discipline yet).

**Planned next step:** PPS wiring + IRQ timestamping to make “Locked/Stratum-1” truly PPS-disciplined.

---

## Troubleshooting

* **No NTP service:** dashboard will show Wi-Fi down and `n_status` false if connect fails.
* **No time / stratum 16:** GPS likely doesn’t have valid RMC (`A`) and/or no GGA fix yet.
* **Wrong UART pins:** current defaults are RX=GPIO1, TX=GPIO0 at 9600 baud.
* **Static IP conflict:** change the IP in `main.cpp` (`cfg_wifi()`).

---

## Roadmap

* PPS input via GPIO IRQ + `time_us_64()` timestamping
* Use PPS discipline to define `Locked` state and tighten stratum behavior
* Optional: make static vs DHCP configurable at build time
* Optional: additional CDC interfaces (NMEA stream / status console) once USB composite is added

---

```
::contentReference[oaicite:0]{index=0}
```
