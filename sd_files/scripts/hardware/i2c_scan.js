// Cardputer Grove: SDA=2, SCL=1. Does not use the ADV keyboard bus.
i2c.begin(2, 1, 100000);
try {
    serial.println(JSON.stringify(i2c.scan()));
} finally {
    i2c.end();
}
