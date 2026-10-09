# Host regression checks

Run from the repository root with a C11 compiler (Clang recommended) and Python 3:

```sh
python3 tests/host/run.py
python3 tests/host/http_boundaries.py
```

`voice_lifecycle.c` includes the production WakeNet source and replaces FreeRTOS/AFE/audio only with pthread mocks. It injects allocation and task-start failures, exercises restart, empty capture, callback stop, and delayed worker shutdown under AddressSanitizer/UndefinedBehaviorSanitizer. Allocation counters check reclamation. The mock AFE has a blocking one-slot producer ring.

`http_boundaries.py` compiles the actual accumulator and synchronous request functions extracted from both HA workers. Its fake HTTP client deliberately ignores event callback return values and sends oversized chunks. This checks the latched-error fix through the request result, not just the callback return.

These tests do not simulate hardware, ESP-SR DSP internals, NimBLE, or ESP32 FreeRTOS scheduling. Board tests remain necessary.
