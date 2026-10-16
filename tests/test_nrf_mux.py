from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NRF24MuxTests(unittest.TestCase):
    def test_radio_and_keyboard_share_pins(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "Install a C++ compiler to run NRF24 mux tests")
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "test_nrf_mux")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
                       "-Wno-missing-field-initializers", "-fsanitize=address,undefined", "-pthread", "-g",
                       "-DTEST_REAL_ADV_MUX=1", "-include", str(ROOT / "tests/native/fakes.h"),
                       "-I" + str(ROOT / "tests/native"), "-I" + str(ROOT / "tests/native/mocks"),
                       "-I" + str(ROOT / "src"), "-I" + str(ROOT / "boards/m5stack-cardputer"),
                       str(ROOT / "tests/native/test_nrf_mux.cpp"),
                       str(ROOT / "boards/m5stack-cardputer/radio_mux.cpp"), "-o", executable]
            for flags in ([], ["-DCARDPUTER_ADV_3IN1_MUX=1"]):
                with self.subTest(profile=flags):
                    compilation = subprocess.run(command + flags, capture_output=True, text=True)
                    self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
                    result = subprocess.run([executable], capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    print(result.stdout.strip())
