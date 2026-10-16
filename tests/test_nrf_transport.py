from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NRF24TransportTests(unittest.TestCase):
    def test_transport_configuration(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "Install a C++ compiler to run NRF24 transport tests")
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "test_nrf_transport")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-Wno-missing-field-initializers", "-fsanitize=address,undefined", "-g",
                       "-I" + str(ROOT / "tests/native"), "-I" + str(ROOT / "tests/native/mocks"),
                       "-I" + str(ROOT / "src"), str(ROOT / "tests/native/test_nrf_transport.cpp"),
                       "-o", executable]
            compilation = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
            result = subprocess.run([executable], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())
