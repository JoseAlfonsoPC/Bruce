import gzip
import hashlib
from pathlib import Path
import re
import runpy
import struct
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from web_assets import prepare_web_assets
from firmware_package import application_partition, validate_images


def partition(kind=0, subtype=0, offset=0x10000, size=0x20000, label=b"factory"):
    return struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size, label, 0)


class WebAssetsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "embedded_resources/web_interface"
        self.source.mkdir(parents=True)
        self.header = self.root / "include/webFiles.h"
        self.source.joinpath("index.html").write_bytes(b'<script>let s="a  b";</script>\r\n')
        self.source.joinpath("index.js").write_bytes(b'// keep newline\nlet x = 1;\n')

    def test_gzip_preserves_original_content(self):
        self.assertTrue(prepare_web_assets(self.root))
        text = self.header.read_text()
        for path in self.source.glob("*.*"):
            if path.suffix not in (".html", ".js"):
                continue
            body = re.search(rf"{path.name.replace('.', '_')}\[\].*?\{{(.*?)\}}", text, re.S).group(1)
            data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-F]{2})", body))
            self.assertEqual(gzip.decompress(data), path.read_bytes())

    def test_unchanged_build_preserves_header(self):
        prepare_web_assets(self.root)
        before = self.header.stat().st_mtime_ns
        self.assertFalse(prepare_web_assets(self.root))
        self.assertEqual(before, self.header.stat().st_mtime_ns)

    def test_missing_header_is_recreated_despite_checksum(self):
        prepare_web_assets(self.root)
        expected = self.header.read_bytes()
        self.header.unlink()
        self.assertTrue(prepare_web_assets(self.root))
        self.assertEqual(expected, self.header.read_bytes())

    def test_changed_deleted_and_renamed_assets_update_header(self):
        prepare_web_assets(self.root)
        self.source.joinpath("index.js").rename(self.source / "other.js")
        self.assertTrue(prepare_web_assets(self.root))
        self.assertNotIn("index_js[]", self.header.read_text())
        self.assertIn("other_js[]", self.header.read_text())
        self.source.joinpath("other.js").unlink()
        self.assertTrue(prepare_web_assets(self.root))

    def test_invalid_identifier_keeps_previous_header(self):
        prepare_web_assets(self.root)
        expected = self.header.read_bytes()
        self.source.joinpath("invalid-name.js").write_bytes(b"bad")
        with self.assertRaises(ValueError):
            prepare_web_assets(self.root)
        self.assertEqual(expected, self.header.read_bytes())

    def test_empty_source_fails(self):
        for path in self.source.iterdir():
            path.unlink()
        with self.assertRaises(ValueError):
            prepare_web_assets(self.root)


class FirmwareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="bruce path with spaces ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.table = self.root / "partitions.bin"
        self.table.write_bytes(partition() + b"\xff" * 32)

    def test_factory_and_ota_offsets(self):
        self.assertEqual(application_partition(self.table, 0x10000), ("factory", 0x20000))
        self.table.write_bytes(partition(subtype=0x10, offset=0x20000, label=b"app0"))
        self.assertEqual(application_partition(self.table, 0x20000), ("app0", 0x20000))
        with self.assertRaises(ValueError):
            application_partition(self.table, 0x10000)

    def test_rejects_invalid_partition_and_non_application(self):
        for data in (b"bad", b"\0" * 32, partition(kind=1), partition(size=0)):
            with self.subTest(data=data):
                self.table.write_bytes(data)
                with self.assertRaises(ValueError):
                    application_partition(self.table, 0x10000)

    def test_md5_record_terminates_table(self):
        self.table.write_bytes(b"\xeb\xeb" + b"\xff" * 30 + partition())
        with self.assertRaises(ValueError):
            application_partition(self.table, 0x10000)

    def test_image_sorting_and_boundaries(self):
        a, b = self.root / "a.bin", self.root / "b.bin"
        a.write_bytes(b"abcd")
        b.write_bytes(b"efgh")
        self.assertEqual(validate_images([("0x4", b), (0, a)]), [(0, a), (4, b)])
        for segments in ([(0, a), (3, b)], [(-1, a)], [(0, a), (0, a)]):
            with self.assertRaises(ValueError):
                validate_images(segments)
        b.write_bytes(b"")
        with self.assertRaises(ValueError):
            validate_images([(0, b)])
        with self.assertRaises(FileNotFoundError):
            validate_images([(0, self.root / "missing.bin")])

    def callback(self, app_size=12):
        self.app = self.root / "firmware.bin"
        self.app.write_bytes(b"A" * app_size)
        self.boot = self.root / "bootloader.bin"
        self.boot.write_bytes(b"BOOT")
        root = self.root
        class Environment:
            def subst(self, value):
                return {"$PROJECT_DIR": str(root), "$BUILD_DIR": str(root), "$PIOENV": "test",
                        "$ESP32_APP_OFFSET": "0x10000", "$OBJCOPY": str(root / "tool with spaces")}.get(value, value)
            def Append(self, **kwargs): pass
            def AddPostAction(self, *args, **kwargs): pass
            def AddCustomTarget(self, *args, **kwargs): pass
            def get(self, key, default=None):
                return {"FLASH_EXTRA_IMAGES": [("0x2000", str(root / "bootloader.bin")),
                                               ("0x8000", str(root / "partitions.bin"))]}.get(key, default)
            def BoardConfig(self): return {"build.mcu": "esp32c6"}
        self.env = Environment()
        module = types.ModuleType("SCons.Script")
        module.Import = lambda name: None
        with patch.dict(sys.modules, {"SCons": types.ModuleType("SCons"), "SCons.Script": module}):
            self.namespace = runpy.run_path(str(ROOT / "build.py"), init_globals={"env": self.env})
        return self.namespace["_merge_bins_callback"]

    def test_merge_preserves_platform_offsets_and_writes_checksum(self):
        callback = self.callback()
        def merge(command, check):
            self.assertEqual(command[0], str(self.root / "tool with spaces"))
            self.assertEqual(command[-6:], ["0x2000", str(self.boot), "0x8000", str(self.table),
                                           "0x10000", str(self.app)])
            Path(command[command.index("--output") + 1]).write_bytes(b"merged")
        with patch("subprocess.run", side_effect=merge):
            self.assertEqual(callback(None, None, self.env), 0)
        output = self.root / "Bruce-test.bin"
        self.assertEqual(output.read_bytes(), b"merged")
        self.assertEqual(output.with_suffix(".bin.sha256").read_text(),
                         hashlib.sha256(b"merged").hexdigest() + "  Bruce-test.bin\n")

    def test_oversized_firmware_stops_before_merge(self):
        callback = self.callback(app_size=0x20001)
        with patch("subprocess.run") as run:
            self.assertEqual(callback(None, None, self.env), 1)
            run.assert_not_called()

    def test_missing_bootloader_stops_before_merge(self):
        callback = self.callback()
        self.boot.unlink()
        with patch("subprocess.run") as run:
            self.assertEqual(callback(None, None, self.env), 1)
            run.assert_not_called()

    def test_failed_merge_preserves_previous_binary(self):
        callback = self.callback()
        output = self.root / "Bruce-test.bin"
        output.write_bytes(b"previous")
        with patch("subprocess.run", side_effect=OSError("merge failed")):
            self.assertEqual(callback(None, None, self.env), 1)
        self.assertEqual(output.read_bytes(), b"previous")
        self.assertFalse(output.with_suffix(".bin.tmp").exists())


class SDKPatchTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="bruce sdk ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.library = self.root / "framework/esp32s3/lib"
        self.library.mkdir(parents=True)
        self.original = self.library / "libnet80211.a"
        self.backup = self.library / "libnet80211.a.old"
        self.marker = self.library / ".patched"
        self.original.write_bytes(b"ORIGINAL")
        web = self.root / "embedded_resources/web_interface"
        web.mkdir(parents=True)
        web.joinpath("index.html").write_bytes(b"test")
        root = self.root
        class Platform:
            def get_package_dir(self, name):
                return str(root / ("framework" if name == "framework-arduinoespressif32-libs" else "toolchain"))
        class Environment:
            def PioPlatform(self): return Platform()
            def BoardConfig(self): return {"build.mcu": "esp32s3"}
            def subst(self, value): return str(root) if value == "$PROJECT_DIR" else value
        self.env = Environment()

    def execute(self):
        return runpy.run_path(str(ROOT / "patch.py"), init_globals={"env": self.env, "Import": lambda name: None})

    def successful_objcopy(self, command, check):
        self.assertEqual(command[0], str(self.root / "toolchain/bin/xtensa-esp32s3-elf-objcopy"))
        Path(command[-1]).write_bytes(b"PATCHED")

    def test_objcopy_failure_preserves_original_archive(self):
        import subprocess
        with patch("subprocess.run", side_effect=subprocess.CalledProcessError(1, "objcopy")):
            with self.assertRaises(subprocess.CalledProcessError):
                self.execute()
        self.assertEqual(self.original.read_bytes(), b"ORIGINAL")
        self.assertFalse(self.backup.exists())
        self.assertFalse(self.marker.exists())

    def test_success_keeps_backup_and_is_idempotent(self):
        with patch("subprocess.run", side_effect=self.successful_objcopy) as run:
            self.execute()
            self.execute()
            self.assertEqual(run.call_count, 1)
        self.assertEqual(self.original.read_bytes(), b"PATCHED")
        self.assertEqual(self.backup.read_bytes(), b"ORIGINAL")
        self.assertTrue(self.marker.exists())

    def test_recovers_archive_left_missing_by_old_script(self):
        self.original.replace(self.backup)
        self.marker.touch()
        with patch("subprocess.run", side_effect=self.successful_objcopy) as run:
            self.execute()
            run.assert_called_once()
        self.assertEqual(self.original.read_bytes(), b"PATCHED")
        self.assertEqual(self.backup.read_bytes(), b"ORIGINAL")
