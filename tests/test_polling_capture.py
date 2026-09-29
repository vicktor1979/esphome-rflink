#!/usr/bin/env python3
"""Host GPIO regression: optional polling acquisition and unchanged IRQ path.

Requires Python 3 and g++. GPIO/time/ESPHome services are test doubles.
No ESP8266 firmware compiler or radio is used.
"""
from pathlib import Path
import importlib.util
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("poll_stage", ROOT / "components/rflink/stage_sources.py")
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
# Receiver allocations have firmware lifetime. Disable LeakSanitizer only;
# ASan and UBSan remain active, including bounds and lifetime checks.
with tempfile.TemporaryDirectory(prefix="rflink-polling-") as directory:
    build = Path(directory)
    stubs = build / "stubs"
    shutil.copytree(ROOT / "tests/rx_gate/stubs", stubs)
    shutil.copy2(ROOT / "components/remote_receiver/remote_receiver.h",
                 stubs / "esphome/components/remote_receiver/remote_receiver.h")
    for kind in ("sensor", "text_sensor", "binary_sensor"):
        rel = Path("esphome/components") / kind / (kind + ".h")
        shutil.copy2(ROOT / "tests/remote_config/stubs" / rel, stubs / rel)
    common = ["g++", "-std=gnu++20", "-O1", "-g", "-DESP8266", "-DUSE_ESP8266",
              "-DUSE_RFLINK_AUTO_START", "-DUSE_NETWORK", "-DUSE_API",
              "-fno-pie", "-no-pie", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
              "-I" + str(stubs), "-I" + str(build),
              "-I" + str(ROOT / "components/remote_receiver"), "-I" + str(ROOT / "components/rflink"),
              str(ROOT / "components/remote_receiver/remote_receiver.cpp"),
              str(ROOT / "components/rflink/rflink_engine.cpp"),
              str(ROOT / "components/rflink/rflink.cpp")]
    def run(test):
        binary = build / test.stem
        result = subprocess.run(common + [str(test), "-o", str(binary)], capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True, env=env, timeout=90)
        print(result.stdout, flush=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    stage.stage(ROOT, build / "rflink_vendor", "all", "legacy")
    for test in ("test_capture.cpp", "test_scheduled_capture.cpp"):
        run(ROOT / "tests/rx_gate" / test)

    # A polling loop must see time advance and scheduled physical pin levels.
    # Do not feed pre-decoded timings into the polling regression.
    hal = stubs / "esphome/core/hal.h"
    hal.write_text('''#pragma once
#include <cstdint>
#define IRAM_ATTR
#define HOT
extern uint32_t test_millis;
uint32_t polling_test_micros();
namespace esphome {
inline uint32_t micros() { return ::polling_test_micros(); }
inline uint32_t millis() { return ::test_millis; }
}
''')
    gpio = stubs / "esphome/components/remote_base/remote_base.h"
    content = gpio.read_text().replace("namespace esphome {", "bool polling_test_level();\nnamespace esphome {", 1)
    content = content.replace("bool digital_read() const { return level; }",
                              "bool digital_read() const { return ::polling_test_level(); }")
    gpio.write_text(content)
    for profile in ("legacy", "extended"):
        selected = stage.stage(ROOT, build / "rflink_vendor", "all" if profile == "extended" else [30, 61, 254], profile)
        print(profile, len(selected), "compiled plugins; runtime 001/030/061", flush=True)
        run(ROOT / "tests/rx_gate/test_polling_capture.cpp")
print("PASS polling + IRQ regressions (host only, ASan/UBSan)")
