# Project Context

ESP8266-based wired thermostat with HomeKit support, UDP logging, Tuya MCU.

Project-level notes (OTA update mechanism, etc.) are in `../AGENTS.md`.

## Critical Constraints

- **Always propose changes and ask permission before applying.** Do not modify files, restart containers, or change config without explicit approval.
- Explain exactly which file will be changed and what the change does before making it.

## Device Info

- Manufacturer: maccoylton, Model: wired-thermostat
- FW_VERSION: `1.1.7` in `src/main.c`
- Button: GPIO 0, Status LED: GPIO 2 (off=-1)
- Uses Tuya MCU component (`../../components/tuya_mcu`)
- Flash: 1MB, dout, 40MHz
- Flash layout: rboot at `0x0`, blank at `0x1000`, app at `0x2000`

## Build & Flash

```bash
# Build inside Docker
./build.sh firmware esp-homekit-wired-thermostat

# Sign firmware
openssl sha384 -binary -out src/firmware/main.bin.sig src/firmware/main.bin
printf "%08x" $(wc -c < src/firmware/main.bin) | xxd -r -p >> src/firmware/main.bin.sig

# Flash app only
esptool.py -p /dev/cu.usbserial-XXXX -b 115200 write_flash -fs 1MB -fm dout -ff 40m 0x2000 src/firmware/main.bin
```

Docker image: `espbuilder`, SDK_PATH: `/project/esp-open-rtos`. Run from `/Users/dave/Documents/OpenCode/esp-firmwares`.

## Key Findings

### Tuya MCU
- Uses `tuya_mcu` component for Tuya-based thermostat hardware communication.
- SNTP is included for timekeeping (`extras/sntp` component).
- The Tuya MCU is a separate chip handling all buttons/display. ESP never sees individual button presses — only serial protocol commands over UART (9600 baud).
- **0x04 handler (`MSG_CMD_RESET_WIFI_SWITCH_NET_CFG`) is a stub** — echoes ACK but never calls `sysparam_erase("wifi_*")` or `sdk_system_restart()`. Holding power button ~5s likely triggers this from MCU, but nothing happens.
- **0x05 handler (`MSG_CMD_RESET_WIFI_SELECT_NET_CFG`) is missing entirely** — defined in enum but no case in switch.
- **0xF4 (factory reset) not implemented** — would be stronger than 0x04, also clears device ID.
- Heartbeat intervals don't match Tuya spec (3s→10s instead of 1s→15s). Product info PID received but never validated. No MCU heartbeat-loss timeout.

### WiFi Reconnect Fix (v1.1.7)
- Root cause: `checkWifiTask()` in `shared_functions.c` relied on DNS failure counting, but lwIP caches DNS results — `dns_error_count` never reaches 12 because cached `ERR_OK` resets it every cycle; the `sdk_system_restart()` path never fires.
- Fix (branch `fix/wifi-reconnect` in `esp-homekit-common-functions`): use station status as primary indicator, active reconnect (disconnect+connect) after 3 consecutive failures, restart after 3 reconnect cycles exhausted.

### Versioning
- FW_VERSION in `src/main.c` follows semver.
- Release naming: tag `x.y.z`, title `vx.y.z-pre`, prerelease=true.

### Factory Reset / Recovery
- **No physical button combo triggers factory reset** on BHT-002. Manual confirms factory reset is only via BecaSmart phone app (menu → Restore Manufacturer Defaults). `gpio_init()` in `main.c` is empty — no button handler was ever wired.
- **LCM bootloader power-cycle method** relies on **rboot4lcm** (custom rboot fork). The thermostat uses `extras/rboot-ota` (standard rboot) in its build config. Whether rboot4lcm is installed on-device depends on original provisioning — unknown unless tested. Power-cycle thresholds: count 1–4 normal, 5–7 check update, 8–10 erase WiFi, 11–13 erase WiFi + LCM_beta, 14+ factory reset (default `count_step=3`).

## Relevant Files

- `src/main.c`: firmware entry, thermostat logic, FW_VERSION
- `src/Makefile`: build config, component paths, signature target
- `../../components/`: shared components (UDPlogger, esp-homekit, tuya_mcu, etc.)
