"""Package the same flash segments and offsets that PlatformIO uploads."""

import hashlib
from pathlib import Path
import subprocess
import sys

from SCons.Script import Import

Import("env")
senv = env
senv.Append(CXXFLAGS=["-Wno-conversion-null"])
project_dir = Path(senv.subst("$PROJECT_DIR"))
sys.path.insert(0, str(project_dir / "tools"))
from firmware_package import application_partition, validate_images

build_dir = Path(senv.subst("$BUILD_DIR"))
app_bin = build_dir / "firmware.bin"
output = project_dir / f"Bruce-{senv.subst('$PIOENV')}.bin"


def _merge_bins_callback(target, source, env):
    temporary = output.with_suffix(".bin.tmp")
    try:
        app_offset = int(env.subst("$ESP32_APP_OFFSET"), 0)
        label, capacity = application_partition(build_dir / "partitions.bin", app_offset)
        size = app_bin.stat().st_size
        if size > capacity:
            raise ValueError(f"firmware.bin ({size} bytes) exceeds {label} ({capacity} bytes)")
        extras = [(offset, env.subst(str(path))) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
        if not extras:
            raise ValueError("PlatformIO did not provide FLASH_EXTRA_IMAGES")
        images = validate_images(extras + [(app_offset, app_bin)])
        # The platform's OBJCOPY is its managed esptool executable. An argv list
        # keeps paths with spaces and shell metacharacters intact on every host.
        uploader = env.subst("$OBJCOPY").strip('"')
        if not uploader:
            raise ValueError("PlatformIO did not provide an esptool executable")
        command = [uploader, "--chip", env.BoardConfig().get("build.mcu"),
                   "merge-bin", "--output", str(temporary)]
        for offset, path in images:
            command.extend([hex(offset), str(path)])
        print(f"[merge_bin] {label}: {size}/{capacity} bytes ({100 * size / capacity:.1f}%)")
        subprocess.run(command, check=True)
        if not temporary.is_file() or temporary.stat().st_size == 0:
            raise ValueError("esptool did not produce a merged image")
        temporary.replace(output)
        checksum = hashlib.sha256(output.read_bytes()).hexdigest()
        output.with_suffix(".bin.sha256").write_text(f"{checksum}  {output.name}\n", encoding="ascii")
        print(f"[merge_bin] Success -> {output}")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"[merge_bin] Error: {error}")
        return 1
    finally:
        temporary.unlink(missing_ok=True)
    return 0


senv.AddPostAction(str(app_bin), _merge_bins_callback)
senv.AddCustomTarget(
    name="build-firmware",
    dependencies=[str(app_bin)],
    actions=[_merge_bins_callback],
    title="Build Firmware (merge)",
    description="Validate and merge the PlatformIO flash images",
)
senv.AddCustomTarget(
    name="upload-nobuild",
    dependencies=None,
    actions=['"$PYTHONEXE" -m platformio run -t upload -t nobuild -e $PIOENV'],
    title="Upload Nobuild",
    description="Run upload without rebuilding",
)
