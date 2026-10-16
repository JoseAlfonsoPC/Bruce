// JosephCGS 3-in-1: read CC1101 MARCSTATE, without transmitting.
spi.begin(40, 39, 14, 15, 1000000, 0);
try {
    var response = spi.transfer(new Uint8Array([0xF5, 0x00]));
    serial.println("MARCSTATE:", response[1]);
} finally {
    spi.end();
}
