# Vendored: espressif/esp_lcd_gc9107 2.0.0

This directory is a verbatim copy of the Component Registry package
`espressif/esp_lcd_gc9107` 2.0.0 (published from espressif/esp-iot-solution,
`components/display/lcd/esp_lcd_gc9107`, commit
e7ea5f75341fb25cab4210c8bdaf26255e065e8d), with one local patch.

Why vendored: neither published version (1.0.0, 2.0.0) compiles on
ESP-IDF v6.0.1, which this SDK requires. The driver reads
`esp_lcd_panel_dev_config_t.rgb_endian`, a field IDF 6 renamed back to
`rgb_ele_order`, so compilation fails inside the package. The local copy
patches only that one switch, behind an `ESP_IDF_VERSION >= 6.0.0` guard
(see the marked block in `esp_lcd_gc9107.c`). Everything else matches the
registry package (component hash
92b99c7d98a8c5115d708835bb53761993fe3464a2d4b43d5433b8389402da13;
`CHECKSUMS.json` and `.component_hash` are registry artifacts and were
dropped).

A project-local component overrides the managed one of the same name, so
the registry dependency in `main/idf_component.yml` stays as the record
of intent: once a fixed version is published upstream, delete this
directory and the registry package is used again, with no other change.
(The dependency is unconditional in the manifest, like `led_strip`'s;
the backend condition lives in `main/CMakeLists.txt`. A manifest `rules:`
condition on `CONFIG_HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R` does not work
here: that symbol is invisible on non-ESP32-S3 targets, and ESP-IDF then
aborts the configure of every other board with "Missing required kconfig
option".)

License: Apache License 2.0, see `license.txt` (Espressif Systems).
