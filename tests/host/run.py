#!/usr/bin/env python3
"""Host regression checks; no ESP32/RTOS timing or hardware validation claimed."""
import pathlib, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="robot-audit-tests-") as tmp:
    tmp = pathlib.Path(tmp)
    for name in ["esp_err.h", "audio_service.h", "esp_afe_sr_iface.h", "esp_afe_sr_models.h",
                 "esp_afe_config.h", "esp_heap_caps.h", "esp_log.h", "esp_process_sdkconfig.h",
                 "model_path.h", "freertos/FreeRTOS.h", "freertos/task.h"]:
        p = tmp / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('#include "voice_mock.h"\n')
    exe = tmp / "voice-test"
    subprocess.run(["cc", "-std=c11", "-g", "-fsanitize=address,undefined", "-pthread",
                    "-I" + str(tmp), "-I" + str(root / "tests/host"),
                    "-I" + str(root / "components/voice/include"),
                    str(root / "tests/host/voice_lifecycle.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
