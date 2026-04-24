"""PlatformIO extra_script: fix mbedtls include paths for ESP32 Arduino.

pioarduino's ESP32 Arduino framework doesn't configure MBEDTLS_CONFIG_FILE
for test builds that use WiFi/TLS. This causes mbedtls PSA crypto headers
to fail with 'mbedtls_md5_context does not name a type' and similar errors.

This script detects ESP32 builds and adds the missing mbedtls configuration
flags that the framework normally provides for application builds.

Usage in platformio.ini:
    extra_scripts = pre:fix_mbedtls.py   ; if note-emu is a local path
    ; OR note-emu's library.json applies it automatically
"""

Import("env")

# Only needed for ESP32 Arduino
if env.get("PIOPLATFORM", "") not in ("espressif32",):
    pass  # might be pioarduino URL-based platform
elif "arduino" not in env.get("PIOFRAMEWORK", []):
    pass  # only for Arduino framework

import os

# Find the framework libs directory for this board's MCU
mcu = env.BoardConfig().get("build.mcu", "esp32")
# Map MCU to the framework-libs directory name
mcu_dir = mcu.replace("-", "")

platformio_dir = env.subst("$PROJECT_PACKAGES_DIR") or os.path.expanduser("~/.platformio/packages")
libs_base = os.path.join(platformio_dir, "framework-arduinoespressif32-libs", mcu_dir)

if not os.path.isdir(libs_base):
    # Try the user's global packages
    libs_base = os.path.join(os.path.expanduser("~/.platformio/packages"),
                             "framework-arduinoespressif32-libs", mcu_dir)

mbedtls_port = os.path.join(libs_base, "include", "mbedtls", "port", "include")

if os.path.isdir(mbedtls_port):
    env.Append(CPPPATH=[mbedtls_port])
    env.Append(CPPDEFINES=[
        ("MBEDTLS_CONFIG_FILE", '\\"mbedtls/esp_config.h\\"'),
    ])
