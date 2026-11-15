# Project Context

ESP8266-based wired thermostat with HomeKit support, UDP logging, Tuya MCU.

Project-level notes (OTA update mechanism, etc.) are in `../AGENTS.md`.

## Critical Constraints

- **Always propose changes and ask permission before applying.** Do not modify files, restart containers, or change config without explicit approval.
- Explain exactly which file will be changed and what the change does before making it.
- **Release notes must only contain user-facing changes.** Internal/dev notes (reset handler investigations, tag naming conventions, etc.) go in AGENTS.md only.
- **Prototype and pre-release work goes on a git branch, never straight onto `main`/`master`.** Create `release/<version>` (e.g. `release/1.2.2`) for the work, commit there, and merge back to `main`/`master` once the pre-release is promoted to a full release. This applies to both this repo and `components/tuya_mcu` (separate repo).

## Device Info

- Manufacturer: maccoylton, Model: wired-thermostat
- FW_VERSION: `1.2.1` in `src/main.c`
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

## Version History

| Version | Changes |
|---------|---------|
| 1.1.7 | WiFi reconnect fix: station-status-based reconnect replaces broken DNS-counting logic. Confirmed via deauth test. |
| 1.1.8 | DNS check removed from checkWifiTask (reduces fragmentation). Serial value leak fixed. 0x04/0x05/0xF4 reset handlers implemented then stripped (MCU never sends these commands — confirmation via UDP logging). **Gateway reachability check added** — non-blocking TCP connect to router IP (from DHCP lease) to catch stale `STATION_GOT_IP` after reboot. Default `wifi_check_interval` changed from 10s to 60s. |
| 1.1.9 | Default `task_stats` turned off (HomeKit characteristic default `false`, init code sets `false`). Can be enabled via HomeKit app if needed. |
| 1.1.10 | **OTA identity self-heal**: `standard_init()` re-anchors sysparam to the LCM's fixed area (0xF7000) and restores `ota_repo`/`ota_file`/`ota_version` if missing (e.g. after UDP factory reset). `DEVICE_MODEL` changed to `esp-homekit-wired-thermostat` (repo suffix, so `manufacturer/model` == the real GitHub repo). Bench device recovered to 1.1.10 via OTA alone. |
| 1.2.0 | **Call for heat**: new standard Switch service ("Call for Heat") visible in Apple Home, latched until cleared. When on: forces MCU power on + pushes 35°C setpoint override (so the MCU relay closes regardless of room temp), `CurrentHeatingCoolingState` reports Heat, and target-state Off / setpoint changes / panel power-off and panel knob turns are all guarded (override re-asserted, HomeKit target temp preserved, state recorded for restore). When cleared: setpoint and power restored from `target_temperature`/`target_state`. Persisted to flash, re-asserted on boot. Accessory `config_number` bumped 1→2 so paired devices discover the new service. |
| 1.2.1 | **Memory tuning**: lwIP `TCP_WND` halved from default 4×MSS (5840) to 2×MSS (2920) via `-DTCP_WND=2920` in `src/Makefile` EXTRA_CFLAGS — saves ~2.9KB heap per connected HomeKit client (verified: preprocessor shows 2920 with build flags, `(4*1460)` without). No functional impact on HomeKit (small messages); OTA/HTTP download throughput roughly halved. Binary size unchanged at 506224. Options 1 (`HOMEKIT_MAX_CLIENTS`→4), 5 (`HOMEKIT_DEBUG` heap logging), and 4 (NETCONN) recorded but not applied. |
| 1.2.2 | **Call-for-heat override moved into `tuya_thermostat`** (fixes target-temperature corruption when toggling the override off). The component now stores the override flag, the real target temperature and the real target power state, forces power on + pins the setpoint at 35°C while active, drops the MCU's 35°C "confirmation" reports instead of passing them to HomeKit, drops + re-asserts any panel knob change away from 35°C, and for 5s after the override clears treats a 35°C report as a stale echo (re-pushing the real target). `main.c` is now a thin HomeKit wrapper — the duplicated POWER/SETPOINT guards in `emitChange` were removed. New unconditional senders `send_power_cmd()`/`send_setpoint_cmd()` added because `setSetPointTemp()` only transmits when the cached value differs (a re-assert of 35 would otherwise be a silent no-op). Binary 507152 (+928). |

## Key Findings

### Call for Heat Override (v1.2.0, reworked v1.2.2)
- The override state machine lives in `components/tuya_mcu/tuya_thermostat.c`, not in `main.c`. `main.c` only owns the HomeKit switch characteristic, its flash persistence, and `set_current_state()`.
- Component state: `callForHeat`, `callForHeatTargetTemp`, static `callForHeatTargetPower`, `callForHeatClearedTick`, `callForHeatEchoGuard`. API: `tuya_thermostat_setCallForHeat(bool on, float real_target_temp, bool real_target_power)`.
- The MCU (`BHT-002`) has no dedicated "force heat" command — the only lever is the setpoint, so the override pins it at `TUYA_THERMOSTAT_CALL_FOR_HEAT_SETPOINT` (35°C, the panel maximum) and forces power on.
- **The v1.2.0/v1.2.1 bug**: the firmware pushed 35°C, the user switched the override off, the firmware pushed the real target back — but the MCU's *echo* of the 35°C arrived afterwards. The `if (call_for_heat)` guard was evaluated at echo time, so it no longer fired: `target_temperature` became 35°C (shown as 95°F), and the 3s save timer then pushed 35°C back to the MCU, making it sticky. Fixed by handling the echo inside the component (drop while active, drop for 5s after clearing) and by removing the guards from `main.c`.
- `setSetPointTemp()` transmits only when the value differs from its cache; override re-asserts therefore need the unconditional `send_setpoint_cmd()`/`send_power_cmd()` helpers.
- Boot restore passes the flashed `target_temperature`/`target_state` into `tuya_thermostat_setCallForHeat()`, so the real target survives a reboot while the override is latched.
- The OFF transition uses the caller's current target (not the value saved at ON), so a target change made *during* the override is honoured on release.
- Any device using the component without calling `setCallForHeat()` is unaffected (`callForHeat` defaults to false).

### Tuya MCU
- Uses `tuya_mcu` component for Tuya-based thermostat hardware communication.
- SNTP is included for timekeeping (`extras/sntp` component).
- The Tuya MCU is a separate chip handling all buttons/display. ESP never sees individual button presses — only serial protocol commands over UART (9600 baud).
- **Reset handlers (0x04/0x05/0xF4) were implemented in v1.1.8 then stripped** — UDP logging confirmed the BHT-002 MCU never sends these commands. The MCU handles buttons/display internally and only sends DP status messages. No physical button combo on this device triggers a reset command over UART.
- Heartbeat intervals don't match Tuya spec (3s→10s instead of 1s→15s). Product info PID received but never validated. No MCU heartbeat-loss timeout.

### WiFi Reconnect Fix (v1.1.7)
- Root cause: `checkWifiTask()` in `shared_functions.c` relied on DNS failure counting, but lwIP caches DNS results — `dns_error_count` never reaches 12 because cached `ERR_OK` resets it every cycle; the `sdk_system_restart()` path never fires.
- Fix (branch `fix/wifi-reconnect` in `esp-homekit-common-functions`): use station status as primary indicator, active reconnect (disconnect+connect) after 3 consecutive failures, restart after 3 reconnect cycles exhausted.

### Gateway Reachability Check (v1.1.8)
- After station status confirms `STATION_GOT_IP`, the device verifies it can actually reach the router by doing a **non-blocking TCP connect to port 443** on the gateway IP (obtained from `sdk_wifi_get_ip_info()`).
- If the TCP handshake succeeds or returns `ECONNREFUSED` (RST from closed port), the router is reachable — the secondary check passes.
- If the connect times out after 3 seconds (`select()` timeout), the gateway is unreachable — treated as stale `STATION_GOT_IP` → triggers reconnect/recovery flow like any other WiFi failure.
- Runs every check cycle (default 60s), replacing the need for a separate "deep check" counter. The user-adjustable `wifi_check_interval` characteristic controls the frequency.
- This catches the well-known ESP8266 SDK bug where `sdk_wifi_station_get_connect_status()` returns `STATION_GOT_IP` after a router reboot but data cannot actually be transmitted.

### Task Stack Usage (measured v1.2.1, via `task_stats` characteristic)
- **Reading the numbers — both firmware labels mislead on units**: `prvTaskCheckFreeStackSpace` (FreeRTOS tasks.c) divides the fill-byte count by `sizeof(StackType_t)`, so `uxTaskGetStackHighWaterMark` / `TaskStatus_t.usStackHighWaterMark` return **words, not bytes**. `task_stats`'s `"High Water Mark (bytes)"` label is wrong (value × 4 = bytes). `checkWifiTask`'s `printf("Free Stack=%lu", uxTaskGetStackHighWaterMark(NULL)/4)` is a double conversion — its comment claims "returns bytes" but it doesn't; real free = printed × 16 bytes. High-water marks are minimums since boot (monotonic worst case), not current. All `Runtime Counter:0` is expected — `configGENERATE_RUN_TIME_STATS` is not enabled. State codes: 0=running, 1=ready, 2=blocked, 3=suspended.
- **Measured stack health (v1.2.1, words free / total)**: udplog 40/320 (⚠️ 88% used — tightest), Tmr Svc 122/512, HomeKit Server 496/1664, mDNS 209/512, **Check WiFi 221/512 (884 bytes free — healthy)**, tcpip_thread 221/480, tuya_mcu_loop 345/512, task_stats 394/512, IDLE/SDK tasks fine.
- `CHECK_WIFI_STACK_SIZE` is now 512 words (2048 bytes, was 384 in v1.1.8) with ~884 bytes free at the high-water mark — comfortable. The old v1.1.8 "~22 bytes free" note is stale (stack was bumped since).
- **`udplog` stack option (recorded, not applied)**: `UDPlogsendSTACKsize` 320 → 512 in `components/UDPlogger/udplogger.c` (shared repo — needs explicit approval per no-library-edits rule). Only if udplog overflows in practice; note the trade-off: FreeRTOS stacks are heap-allocated, so +832 bytes costs ~3.3KB of heap. Currently stable at 88% used — warning, not emergency.

### Heap Fragmentation Fix (v1.1.8)
- Removed `netconn_gethostbyname()` DNS check from `checkWifiTask()` — `STATION_GOT_IP` is now trusted directly, eliminating repeated lwIP UDP PCB allocate/free cycles every check interval that fragmented the heap.

### Versioning
- FW_VERSION in `src/main.c` follows semver.
- Release naming: tag `x.y.z`, title `vx.y.z-pre`, prerelease=true.
- **CRITICAL: Release tags must NOT have a `v` prefix** (e.g. `1.1.8` not `v1.1.8`). The OTA code in `life-cycle-manager` constructs download URLs directly from the version string as `releases/download/{version}/{file}`. A tag like `v1.1.8` produces a 404 because the URL uses `1.1.8` but GitHub's tag is `v1.1.8`. The stable release `1.1.4` established this convention with a bare tag — all subsequent releases must match.

### Factory Reset / Recovery
- **No physical button combo triggers factory reset** on BHT-002. Manual confirms factory reset is only via BecaSmart phone app (menu → Restore Manufacturer Defaults). `gpio_init()` in `main.c` is empty — no button handler was ever wired.
- **LCM bootloader power-cycle method** relies on **rboot4lcm** (custom rboot fork). The thermostat uses `extras/rboot-ota` (standard rboot) in its build config. Whether rboot4lcm is installed on-device depends on original provisioning — unknown unless tested. Power-cycle thresholds: count 1–4 normal, 5–7 check update, 8–10 erase WiFi, 11–13 erase WiFi + LCM_beta, 14+ factory reset (default `count_step=3`).

### OTA Identity Wipe (v1.1.9 factory reset → 1.1.10 self-heal)
- The v1.1.9 UDP `FACTORY_RESET` (port 9876, password-protected) erases `SYSPARAMSECTOR` (0xF7000) + the next sector — wiping wifi creds **and** the OTA triad (`ota_repo`/`ota_file`/`ota_version`).
- Symptom: after re-provisioning wifi (portal `/settings`), mDNS showed `md=model_unknown`, revision `0.0.0`, `c#=1`. Root cause: `ota_read_sysparam()` (ota-api.c:20) falls back to `manuf_unknown`/`model_unknown`/`0.0.0` when `ota_repo`/`ota_version` are absent.
- Fix (v1.1.10, in shared `esp-homekit-common-functions` `standard_init()`):
  1. `sysparam_init(0xF7000, 0)` re-anchor — the LCM always maintains a 2-sector area at 0xF7000 (creates/rebuilds on every boot, `life-cycle-manager/ota.c` `ota_read_rtc`); `app_main` may init at a flash-size-derived address on non-1MB devices.
  2. `ota_sysparam_ensure(manufacturer, model, revision)` — write-if-missing restore of the triad (healthy devices: zero writes).
- **`DEVICE_MODEL` must equal the repo suffix** (`esp-homekit-wired-thermostat`) so `manufacturer + "/" + model` == `maccoylton/esp-homekit-wired-thermostat` (the real repo).
- Bench thermostat was already healable: its OTA keys had been restored during wifi re-provisioning through the portal, so it OTA'd to 1.1.10 on its own (no AP-portal recovery needed). mDNS verified correct model + revision `1.1.10` afterwards.

### Binary Size Reduction Options (recorded v1.2.0; **Option 2 applied in v1.2.1**, others pending)
- **Applied in v1.2.1 — Option 2 (TCP_WND halved)**: `-DTCP_WND=2920` added to `EXTRA_CFLAGS` in `src/Makefile` (see version history).
- **Context**: 512K rboot app slot; v1.2.0/v1.2.1 `main.bin` is 506,224 bytes (~18K headroom). `.irom0.text` is 468,932. Build already has `-ffunction-sections`/`-gc-sections` (`SPLIT_SECTIONS ?= 1`), so dead code is stripped. Options below are ranked by expected win; apply one at a time, rebuild, and measure `main.bin` size before keeping the change.
- **Option 1 — `HOMEKIT_SMALL = 1` in `src/Makefile` (currently 0). Biggest win, ~30-45K.** Adds `-DCURVE25519_SMALL -DED25519_SMALL` to wolfSSL (`components/esp-homekit/component.mk`). Removes the 30,720-byte `.rodata.base` precomputed table in `ge_operations.o` and shrinks `sc_muladd`/`sc_reduce`/`fe_*`. Trade-off: HomeKit pair-setup becomes noticeably slower; normal operation unaffected.
- **Option 2 — compile with `-Os` instead of `-O2`, ~30-50K.** esp-open-rtos `FLAVOR=release` uses `-g -O2` (`esp-open-rtos/parameters.mk:120`); no `-Os` anywhere in the build system. Add `-Os` via `EXTRA_CFLAGS` (last `-O` flag wins in GCC). Low risk, slight speed cost.
- **Option 3 — remove `extras/dht` from `EXTRA_COMPONENTS` in `src/Makefile`.** Not referenced anywhere in thermostat sources or shared components; already gc-sectioned out, so ~0K flash win — hygiene only (skips a pointless compile).
- **Option 4 — trim float logging, ~6-7K.** Replace `%f`/`%2.1f` in LOG macros with integer-tenths formatting to drop `_dtoa_r`/`_strtod_l` (~6.7K combined). Moderate effort, degrades log readability — only if Options 1+2 are insufficient.
- **Not worth pursuing**: swapping `http_parser_execute` (9.5K, needed), stripping `-g` (debug sections aren't in `main.bin`), trimming log strings (marginal).
- **Biggest symbols at v1.2.0**: `.rodata.base` 30,720 (wolfSSL ge_operations), `sc_muladd` 11,837, `http_parser_execute` 9,572, `sc_reduce` 5,007, `fe_mul` 4,919, `sdk_SHA1Transform` 4,837, `_strtod_l` 3,482, `_dtoa_r` 3,230.
- Expected outcome if Options 1+2 both applied: ~430-450K — comfortable headroom.

### Runtime Memory / Client Connection Options (recorded v1.2.0; **TCP_WND applied in v1.2.1**, others pending)
- **Context**: ~35-40KB heap free after static `.bss` 36,968 + `.data` 6,412. With 1-2 HomeKit clients connected: `Free Heap=19756, Free Stack=53` (stack reading is `checkWifiTask`, ~212 bytes headroom, stable high-water mark). Each client ≈ 6KB (TCP window + pcb/netconn + `client_context_t`). lwIP `MEMP_NUM_TCP_PCB = 5` hard-caps connections at 5 regardless of `HOMEKIT_MAX_CLIENTS`. Stale-client cleanup: `SO_RCVTIMEO` 10s + TCP keepalive idle 180s, interval 30s, 4 probes.
- **Applied in v1.2.1 — Option 2**: `TCP_WND` halved 5840→2920 (~2.9KB saved per client).
- **Option 1 — `HOMEKIT_MAX_CLIENTS` 16 → 4 in `src/Makefile`** (very low comms risk). Makes the 5th connection a clean refusal instead of an OOM-crash risk; esp-homekit's component.mk forwards the Makefile value as `-DHOMEKIT_MAX_CLIENTS=` (no library edit). Watch: iPhone + iPad + Mac + Apple TV/HomePod hub = 4 controllers already.
- **Option 3 — TCP keepalive idle 180s → 60s** in `components/esp-homekit/src/server.c` (`idle = 180`). ⚠️ This one IS in the esp-homekit library (separate git repo — user prefers not to modify standard libraries; needs explicit approval). Only starts probing sooner; still needs ~9 failed probes (~10 min) to drop a connection.
- **Option 4 — `MEMP_NUM_NETCONN` 12 → 8: REJECTED.** Socket users counted: mDNS (persistent) + UDPlogger (persistent) + HomeKit listen + up to 5 clients + DHCP/portal (1-2 during provisioning) + OTA HTTPS (1) ≈ 11 worst case → would fail socket creation during OTA/portal with clients connected. Saving ~400 bytes not worth it.
- **Option 5 — `HOMEKIT_DEBUG = 1` in `src/Makefile`** (zero library edit): esp-homekit's server.c already calls `DEBUG_HEAP()` after every client accept — the flag activates it, plus ALL esp-homekit debug chatter (noisy, goes to UDP log, a few KB more flash). Suggest temporary diagnostic, not permanent.
- **Option 6 — heap/stack readings**: `checkWifiTask` already prints `Free Heap/Free Stack` every `wifi_check_interval`; `task_stats` characteristic (default off since v1.1.9) dumps per-task stack high-water marks.

## Relevant Files

- `src/main.c`: firmware entry, thermostat logic, FW_VERSION
- `src/Makefile`: build config, component paths, signature target
- `../../components/`: shared components (UDPlogger, esp-homekit, tuya_mcu, etc.)
