"""POSIX USB monitor without explicit modem-line changes; logs stay private."""
import argparse
from contextlib import contextmanager
import math
import os
from pathlib import Path
import re
import select
import termios
import time


@contextmanager
def open_serial_port(port):
    """Keep DTR/RTS untouched and disable hang-up on close (HUPCL)."""
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = ((attrs[2] & ~(termios.PARENB | termios.CSIZE |
                                 termios.CSTOPB | termios.HUPCL)) |
                    termios.CS8 | termios.CLOCAL | termios.CREAD)
        attrs[4] = attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        yield fd
    finally:
        os.close(fd)


def read_chunks(fd, seconds):
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("duration must be finite and positive")
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if not select.select([fd], [], [], min(.2, max(0, end - time.monotonic())))[0]:
            continue
        try:
            data = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not data:
            raise ConnectionError("serial device disconnected")
        yield data


def diagnostic_line(line):
    """Avoid printing startup configuration and identifiers to the terminal."""
    return bool(re.match(
        r"^(?:\[MJ scan(?: info)?\]|\[NRF24 check\]|\[FM\]|\[boot\]|\[RAMLOG\]|"
        r"rst:|ESP-ROM:|Guru Meditation|panic\(|Backtrace:|Rebooting|Brownout|Uptime:)",
        line))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=120)
    parser.add_argument("--output", type=Path, required=True,
                        help="New private raw log; an existing file is never overwritten")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error("--seconds must be finite and positive")
    with os.fdopen(os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "wb") as log:
        with open_serial_port(args.port) as fd:
            print("USB monitor ready (no DTR/RTS operations, HUPCL disabled).", flush=True)
            pending = b""
            for data in read_chunks(fd, args.seconds):
                log.write(data)
                log.flush()
                pending += data
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    text = line.decode(errors="replace").strip()
                    if diagnostic_line(text):
                        print(text, flush=True)
                pending = pending[-8192:]


if __name__ == "__main__":
    main()
