![GitHub All Releases](https://img.shields.io/github/downloads/maccoylton/esp-homekit-wired-thermostat/total) 
![GitHub Releases](https://img.shields.io/github/downloads/maccoylton/esp-homekit-wired-thermostat/latest/total)

# esp-homekit-wired-thermostat
Homekit Firmware for Wired Thermostat BHT-002

---

## Factory Reset (Remote, Password-Protected)

The firmware supports a secure remote factory reset via UDP on port **9876**. This is useful if you lose physical access to the device or need to reset it without button presses.

### How It Works

1. **Set a password** via HomeKit (Eve app or any HomeKit app that exposes custom characteristics):
   - Find the **"Set Factory Reset Password"** characteristic in the Thermostat service
   - Enter a password (e.g., `MySecret123`)
   - The password is stored once and **cannot be changed** without a full reflash

2. **Trigger factory reset** by sending a UDP packet:
   ```bash
   printf "FACTORY_RESET YourPasswordHere" | nc -u -w1 <device-ip> 9876
   ```
   - Replace `YourPasswordHere` with the password you set
   - Replace `<device-ip>` with the thermostat's IP address
   - Use `printf` (not `echo`) to avoid trailing newlines

3. **What happens**:
   - Device validates the password
   - Sets `ota_count=14` in sysparam
   - Reboots into LCM (Life Cycle Manager) OTA mode
   - LCM erases all user config, WiFi credentials, and HomeKit pairings
   - Device boots into AP mode ready for fresh setup

### Security Notes

- Password is **write-once** (cannot be changed without a full reflash)
- UDP is **not encrypted** - use on trusted networks only
- No rate limiting - protect with network segmentation if needed
- Password stored as `FRPW:YourPassword` in `ota_string` sysparam

### Debugging

Enable verbose logging (log level 7 via HomeKit) to see:
```
fr_recv_callback: packet received from 192.168.1.X:XXXXX, len=XX
fr_recv_callback: payload='FACTORY_RESET YourPassword'
fr_recv_callback: searching for password 'YourPassword' (len=XX)
fr_recv_callback: found stored ota_string='FRPW:YourPassword'
fr_recv_callback: actual_fr_pw='YourPassword' (len=XX)
fr_recv_callback: PASSWORD MATCH! Executing factory reset!
```

### Example

```bash
# Set password in Eve app first, then:
printf "FACTORY_RESET MySecret123" | nc -u -w1 192.168.1.208 9876
```

---

## Call for Heat (v1.2.0)

The accessory exposes a second service, a switch named **Call for Heat**, visible as a normal toggle in the Apple Home app.

- **On**: forces heating on regardless of the thermostat's own temperature comparison or its target state (Off). The firmware powers the MCU and overrides its setpoint to the maximum (35°C) so the heating relay stays closed. `CurrentHeatingCoolingState` reports *Heating* while active.
- **Off**: restores the normal target temperature and power state.
- The switch is **latched**: it stays on until something (Home app, Home Assistant, a HomeKit automation) turns it off, and it survives a reboot (persisted to flash).
- While active, the HomeKit target temperature is preserved (changes are saved but only pushed to the heater after the switch is cleared), and the physical panel's power button/knob cannot defeat the override.

Intended for external controllers (e.g. Home Assistant or a schedule) to demand heat independently of the local temperature reading.

---

## Device Info

- **Hardware**: BHT-002 wired thermostat
- **MCU**: Tuya-based (UART 9600 baud)
- **ESP8266**: 1MB flash, dout, 40MHz
- **OTA**: rboot-ota with Life Cycle Manager support