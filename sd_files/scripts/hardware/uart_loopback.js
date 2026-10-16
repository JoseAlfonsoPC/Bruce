// Disconnect external devices first. Connect Grove GPIO1 (RX) to GPIO2 (TX).
uart.begin(115200, 1, 2);
try {
    uart.write(new Uint8Array([0, 85, 170, 255]));
    var data = uart.read(4, 1000);
    if (data.length !== 4 || data[0] !== 0 || data[1] !== 85 || data[2] !== 170 || data[3] !== 255) {
        throw new Error("UART loopback failed");
    }
    serial.println("UART loopback OK");
} finally {
    uart.end();
}
