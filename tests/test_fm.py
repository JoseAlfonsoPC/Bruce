from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FMPresenceTests(unittest.TestCase):
    def test_fm_entry_points_without_hardware(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "Install a C++ compiler to run FM tests")
        for profile in ("general", "adv_3in1"):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as directory:
                executable = str(Path(directory) / "test_fm")
                command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                           "-Wno-unused-parameter", "-fsanitize=address,undefined", "-g",
                           "-I" + str(ROOT / "tests/native/fm_mocks"), "-I" + str(ROOT / "src"),
                           str(ROOT / "tests/native/test_fm.cpp"),
                           str(ROOT / "src/modules/fm/fm.cpp"), "-o", executable]
                if profile == "adv_3in1":
                    command.insert(1, "-DCARDPUTER_ADV_3IN1_MUX=1")
                compilation = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
                result = subprocess.run([executable], capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                print(result.stdout.strip())
