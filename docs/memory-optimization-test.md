# Memory optimization / BLE on-demand experiment

This branch keeps 640×172 UI and Hi ESP WakeNet. Changes here are **not yet compiled or hardware verified**.

### Design
- Bluetooth controller + NimBLE are **not initialized at boot**. Bluetooth Settings starts in OFF mode.
- Tapping the existing Bluetooth icon to ON calls lazy `bluetooth_service_init()`. All existing scan/pair/connect functions remain compiled and available.
- Tapping OFF stops scan/disconnects but **does not currently release NimBLE/controller RAM**. Full deinitialization after prior ON needs a separate tested lifecycle change; this is not a "memory-freeing" toggle after first activation. The default-OFF boot is the memory-saving feature.
- Boot order: board -> UI -> audio I2S -> Wi-Fi -> WakeNet AFE. Audio DMA is still reserved before Wi-Fi; Wi-Fi internal allocations occur before model runtime.
- Per-stage memory prints: `MEM before board`, `MEM after board`, `MEM after UI`, `MEM after audio`, `MEM after Wi-Fi init`, `MEM after WakeNet start`; prints internal heap, largest contiguous internal 8-bit block, DMA-capable internal bytes and free PSRAM.
- No GUI visual design changes.

### Test
```sh
cd ~/Desktop/esp32-ai-robot-voice
git pull origin feature/hello-robot-wakenet
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem201401 flash monitor
```

Check for Wi-Fi got IP, WakeNet startup, and repeated Hi ESP detections; then verify UI settings Bluetooth reads OFF and enabling it works. On-demand BLE activation can still fail for insufficient contiguous internal heap while AFE runs; collect the `MEM ...` lines and `bluetooth: before NimBLE` / `nimble_port_init failed` logs to guide next optimization pass. Toggle ON/OFF at least 5–10 times and verify scan, Wi-Fi, WakeNet, heap, and no crashes. A failed on-demand BLE init still requires debugging; a default-off boot cannot guarantee future ON fits available internal heap.

Do not interpret this first change as a proven guarantee that Wi-Fi + BLE + WakeNet + UI can all run simultaneously. This requires full load and soak testing.
