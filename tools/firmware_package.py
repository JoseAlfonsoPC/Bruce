"""Validation for PlatformIO's flash images. No SCons dependency."""

from pathlib import Path
import struct


def application_partition(table_path, app_offset):
    """Find the application at the actual upload address in partitions.bin."""
    data = Path(table_path).read_bytes()
    for index in range(0, len(data) - 31, 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from("<HBBII16sI", data, index)
        if magic in (0xFFFF, 0xEBEB):
            break
        if magic != 0x50AA:
            raise ValueError(f"Invalid partition entry at byte {index}")
        if kind == 0 and offset == app_offset:
            if size == 0:
                raise ValueError("Application partition is empty")
            return label.split(b"\0", 1)[0].decode("utf-8"), size
    raise ValueError(f"No application partition at {app_offset:#x}")


def validate_images(images):
    """Reject missing, empty, duplicate, or overlapping flash segments."""
    images = sorted((int(offset, 0) if isinstance(offset, str) else offset, Path(path))
                    for offset, path in images)
    previous_end = 0
    for offset, path in images:
        if offset < previous_end:
            raise ValueError(f"Overlapping image at {offset:#x}: {path}")
        size = path.stat().st_size
        if size == 0:
            raise ValueError(f"Empty flash image: {path}")
        previous_end = offset + size
    return images
