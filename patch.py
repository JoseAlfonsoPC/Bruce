from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    Import: Any = None
    env: Any = {}


Import("env")  # type: ignore

# Keep the original archive intact if objcopy fails (including missing tools).
from pathlib import Path
import subprocess
import sys

framework_dir = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs"))
mcu = env.BoardConfig().get("build.mcu", "")
library_dir = framework_dir / mcu / "lib"
original = library_dir / "libnet80211.a"
backup = library_dir / "libnet80211.a.old"
patched = library_dir / "libnet80211.a.patched"
marker = library_dir / ".patched"
if not original.exists() and backup.exists():
    backup.replace(original)
    marker.unlink(missing_ok=True)
if mcu != "esp32p4" and not marker.is_file():
    architecture = "riscv32-esp" if mcu in ("esp32c5", "esp32c6") else f"xtensa-{mcu}"
    package = "toolchain-riscv32-esp" if mcu in ("esp32c5", "esp32c6") else "toolchain-xtensa-esp-elf"
    toolchain = Path(env.PioPlatform().get_package_dir(package))
    suffix = ".exe" if sys.platform == "win32" else ""
    objcopy = toolchain / "bin" / f"{architecture}-elf-objcopy{suffix}"
    subprocess.run([
        str(objcopy), "--weaken-symbol=ieee80211_raw_frame_sanity_check",
        str(original), str(patched),
    ], check=True)
    if not patched.is_file() or not patched.stat().st_size:
        raise RuntimeError("objcopy did not produce a patched WiFi archive")
    original.replace(backup)
    patched.replace(original)
    marker.touch()


# Embed locally so clean builds do not depend on a public minifier.
sys.path.insert(0, str(Path(env.subst("$PROJECT_DIR")) / "tools"))
from web_assets import prepare_web_assets
prepare_web_assets(env.subst("$PROJECT_DIR"))
