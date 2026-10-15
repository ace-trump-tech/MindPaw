"""Hardware configuration checks; these do not simulate electrical behavior."""
import configparser
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class HardwareProfiles(unittest.TestCase):
    def test_s3_pins_are_unique_and_avoid_reserved_pins(self):
        source = (ROOT / "MindPaw_main/src/board_config.h").read_text(encoding="utf-8")
        s3 = source.split("#elif defined(CONFIG_IDF_TARGET_ESP32S3)")[1].split("#else")[0]
        pins = dict((name, int(value)) for name, value in re.findall(r"#define (MP_\w+) (-?\d+)", s3))
        assigned = [value for value in pins.values() if value >= 0]
        self.assertEqual(len(assigned), len(set(assigned)), pins)
        self.assertFalse(set(assigned) & {0, 3, 19, 20, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 43, 44, 45, 46})

    def test_spi_pins_are_not_peripheral_outputs(self):
        source = (ROOT / "MindPaw_main/src/board_config.h").read_text(encoding="utf-8")
        esp = source.split("#if defined(ESP8266)")[1].split("#elif")[0]
        pins = [int(v) for v in re.findall(r"#define MP_\w+ (\d+)", esp)]
        self.assertFalse(set(pins) & {12, 13, 14, 15})

    def test_build_profiles_select_camera_and_memory(self):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read(ROOT / "MindPaw_main/platformio.ini")
        self.assertIn("-D_MEMORYSAVER_", cfg["env:nodemcuv2"]["build_flags"])
        self.assertIn("-DOV2640_MINI_2MP_PLUS", cfg["env:nodemcuv2"]["build_flags"])
        self.assertNotIn("ArduCAM", cfg["env:esp32s3_dvp"]["lib_deps"])
        self.assertEqual(cfg["env:esp32s3_dvp"]["board_build.arduino.memory_type"], "qio_opi")
        self.assertIn("-DBOARD_HAS_PSRAM", cfg["env:esp32s3_dvp"]["build_flags"])


if __name__ == "__main__":
    unittest.main()
