````markdown
> **Note ✨ (AI-Generated README):** This `README.md` was generated with AI assistance. It may contain mistakes, outdated assumptions, or phrasing that is overly enthusiastic about the project. Treat it as a starting point and verify technical details (pin mappings, build steps, features, and roadmap items) against the actual code and hardware setup.

# Pico GPS Stratum-1 NTP Server (RP2040 / Pico W)

A **GPS-disciplined (Stratum-1 style) NTP server** built on **Raspberry Pi Pico W (RP2040)** using the **Pico SDK (C/C++)**.

**Time source:** Waveshare Pico GPS L76X via **UART0**  
**Time discipline:** NMEA (RMC/GGA/ZDA) now, **PPS** planned (GPIO IRQ timestamping)

This project is aimed at being a **small, deterministic, no-BS time appliance**: stable, debuggable, and friendly to embedded constraints.

---

## Features

- **NTP server over UDP** (lwIP)
- **GPS/NMEA parsing** (lightweight, robust)
  - Minimally handles: **RMC, GGA, ZDA**
  - Does *not* require a full NMEA library
- **State machine** for GPS lock state:
  - `Booting → Acquiring → Acquired → Locked (reserved for PPS) → Error`
  - *Pre-PPS meaning:*  
    - **Acquired** requires `RMC status = 'A'` and `GGA fix quality > 0`
    - **Locked** is reserved for PPS discipline
- **USB CDC console UI** (ANSI dashboard, 2–5 Hz redraw)
- **LED heartbeat** via repeating timer (short + deterministic callbacks)
- **Scaled integer / raw string output** preferred (float printf not required)

---

## Hardware

### Target Board
- Raspberry Pi **Pico W**  
  (`PICO_BOARD=pico_w`)

### GPS Module
- Waveshare **Pico GPS L76X** (UART NMEA output)

### Wiring (default)
> Adjust GPIOs to match your local wiring/config.

- GPS **TX → Pico RX (UART0 RX)**  
- GPS **RX → Pico TX (UART0 TX)** *(optional unless configuring GPS)*
- GPS **GND → Pico GND**
- GPS **VCC → Pico 3V3** (verify your GPS module supports 3.3V)

**Planned PPS wiring (future):**
- GPS **PPS → Pico GPIO** (GPIO IRQ + `time_us_64()` timestamping)

---

## Software / Toolchain

- Pico SDK (CMake)
- Ninja
- VS Code / VSCodium
- USB serial terminal (example: `picocom`)

---

## Build

### 1) Configure
From the repo root:

```bash
mkdir -p build
cd build
cmake -G Ninja .. -DPICO_BOARD=pico_w
```

### 2) Compile

```bash
ninja
```

### 3) Flash

This produces a `.uf2` you can drag to the Pico in BOOTSEL mode.

Example:

* `build/<target>.uf2`

---

## Running / Console

Connect USB and open the CDC serial port:

```bash
picocom -b 115200 /dev/ttyACM0
```

The device prints an ANSI “single screen” dashboard. Logs (if enabled) should be separate from UI redraws.

**Terminal compatibility:** output uses `\r\n`.

---

## NTP Usage

Once the Pico is on your network (Wi-Fi or other interface depending on your build), point clients to it as an NTP server.

Examples:

### Linux (chrony)

Add to `/etc/chrony/chrony.conf`:

```conf
server <PICO_IP> iburst
```

### Windows

Set a manual peer list:

```powershell
w32tm /config /manualpeerlist:"<PICO_IP>" /syncfromflags:manual /update
w32tm /resync
```

> Replace `<PICO_IP>` with the Pico’s IP address.

---

## Project Layout (typical)

* `main.cpp`
  Application entry point and top-level loop
* `gps_uart.*`
  UART0 configuration + line acquisition
* `gps_state.*`
  NMEA parsing, state machine, fix/time validity
* `timebase.*`
  Time conversion + NTP epoch helpers
* `ntp_server.*`
  UDP listener + NTP packet handling
* `ui_console.*`
  ANSI dashboard rendering + optional hotkeys
* `led.*`
  LED heartbeat / status patterns
* `wifi_cfg.*` / `wifi_secrets.h`
  Wi-Fi configuration (if enabled)
* `lwipopts.h`
  lwIP configuration overrides

---

## Design Notes / Conventions

### Code Organization Rules (important)

* All non-inline functions are **declared in headers** and **defined in `.cpp`**
* **No globals defined in headers**

  * Use `extern` declarations in `.h`
  * Define exactly once in a `.cpp`
* Use `static` only for:

  * file-local helpers in `.cpp`
  * `static inline` helpers in headers

### Real-Time / Determinism

* Avoid blocking calls in main loop and callbacks
* No `sleep_ms()` in timer callbacks
* Keep repeating timer callbacks short and deterministic

### NMEA Parsing Scope

* Robust handling of **RMC/GGA/ZDA**
* Ignore unrelated sentences (GSV/VTG/etc.) without penalizing state

---

## Status & Roadmap

### Current (NMEA discipline)

* Uses NMEA time/fix validity for “Acquired”
* NTP replies are based on the current GPS-derived timebase

### Next (PPS discipline)

* GPIO IRQ on PPS
* Timestamp using `time_us_64()`
* Introduce **Locked** state when PPS is stable
* Reduce jitter and improve holdover behavior

---

## Troubleshooting

### “It builds but time is wrong”

* Verify GPS has a fix (**RMC status ‘A’**, **GGA fix quality > 0**)
* Verify baud rate matches GPS output (commonly 9600)
* Ensure UART wiring is correct (TX/RX crossed)

### “Link errors after adding new files”

* Make sure new `.cpp` files are added to `CMakeLists.txt` `add_executable(...)` sources

### “Float printing doesn’t work”

If you truly need float `printf`, add:

```cmake
target_link_options(<target> PRIVATE -Wl,-u,_printf_float)
```

But prefer scaled integers / raw strings.

---

## Security / Deployment Notes

* This is a **LAN appliance**. Treat it like infrastructure:

  * Put it on a trusted network segment
  * Restrict inbound UDP/123 if needed
* If you later add USB networking (RNDIS/ECM), consider the host-side security posture.

---

## License

Choose a license that matches your intent (MIT/BSD-3-Clause are common for Pico projects).
Add `LICENSE` at repo root.

---

## Acknowledgements

* Raspberry Pi Pico SDK
* lwIP
* GPS/NTP protocol docs and community references

```

If you want, paste your actual repo name, target binary name, and the exact GPIO mapping you’re using (UART0 pins + planned PPS pin), and I’ll tighten the README to match the codebase exactly.
```
