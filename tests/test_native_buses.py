from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeBusTests(unittest.TestCase):
    def test_bus_contracts(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "Install a C++ compiler to run the native bus tests")
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "test_buses")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Wno-comment", "-Wno-missing-field-initializers",
                       "-Werror", "-fsanitize=address,undefined", "-g",
                       "-include", str(ROOT / "tests/native/fakes.h"),
                       "-I" + str(ROOT / "tests/native"), "-I" + str(ROOT / "tests/native/mocks"),
                       "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/modules/bjs_interpreter"),
                       str(ROOT / "tests/native/test_buses.cpp"),
                       str(ROOT / "src/modules/bjs_interpreter/hardware_js.cpp"),
                       str(ROOT / "src/modules/bjs_interpreter/i2c_js.cpp"), "-o", executable]
            if "clang" in subprocess.check_output([compiler, "--version"], text=True).lower():
                # The fake JSValue owns STL objects; the real engine uses a scalar C value.
                command.append("-Wno-return-type-c-linkage")
            for profile_flags in ([], ["-DCARDPUTER_ADV_3IN1_MUX=1"]):
                with self.subTest(profile=profile_flags):
                    compilation = subprocess.run(command + profile_flags, capture_output=True, text=True)
                    self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
                    result = subprocess.run([executable], check=True, capture_output=True, text=True)
                    print(result.stdout.strip())
