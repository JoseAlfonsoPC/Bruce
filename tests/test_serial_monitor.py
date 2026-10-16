import errno
import os
import pty
import termios
import unittest

from tools.serial_monitor import diagnostic_line, open_serial_port, read_chunks


class SerialMonitorTests(unittest.TestCase):
    def test_binary_read_and_hangup_disabled(self):
        master, slave = pty.openpty()
        try:
            attrs = termios.tcgetattr(slave)
            attrs[2] |= termios.HUPCL
            termios.tcsetattr(slave, termios.TCSANOW, attrs)
            with open_serial_port(os.ttyname(slave)) as fd:
                self.assertFalse(termios.tcgetattr(fd)[2] & termios.HUPCL)
                data = b"\x00\x03\x11\x13\xff\r\n[MJ scan] frames=12 valid=0\n"
                os.write(master, data)
                self.assertEqual(b"".join(read_chunks(fd, .05)), data)
            self.assertFalse(termios.tcgetattr(slave)[2] & termios.HUPCL)
            with self.assertRaises(OSError) as error:
                os.fstat(fd)
            self.assertEqual(error.exception.errno, errno.EBADF)
        finally:
            os.close(master)
            os.close(slave)

    def test_exception_closes_port_and_duration_is_bounded(self):
        master, slave = pty.openpty()
        try:
            with self.assertRaises(RuntimeError):
                with open_serial_port(os.ttyname(slave)) as fd:
                    for value in [0, -1, float("nan"), float("inf")]:
                        with self.assertRaises(ValueError):
                            list(read_chunks(fd, value))
                    raise RuntimeError("caller interrupted")
            with self.assertRaises(OSError) as error:
                os.fstat(fd)
            self.assertEqual(error.exception.errno, errno.EBADF)
        finally:
            os.close(master)
            os.close(slave)

    def test_console_filter_hides_configuration(self):
        for line in ["MAC addr: 00:11:22:33:44:55", '  "wifi_password": "example"', "Device: Cardputer"]:
            self.assertFalse(diagnostic_line(line))
        for line in ["[MJ scan] frames=12 valid=0", "[MJ scan info] length=10 channel=30", "[FM] Si4713 not found",
                     "rst:0x15 (USB_UART_CHIP_RESET)", "Guru Meditation Error", "Backtrace: 0x4037"]:
            self.assertTrue(diagnostic_line(line))
