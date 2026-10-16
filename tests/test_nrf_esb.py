from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NRF24EsbTests(unittest.TestCase):
    def test_passive_esb_decoder(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "Install a C++ compiler to run ESB decoder tests")
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "test_nrf_esb")
            compilation = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=address,undefined", "-g", "-I" + str(ROOT / "src"),
                 str(ROOT / "tests/native/test_nrf_esb.cpp"), "-o", executable],
                capture_output=True, text=True)
            self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
            result = subprocess.run([executable], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())
