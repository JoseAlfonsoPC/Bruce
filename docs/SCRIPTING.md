# Interfaces de hardware en JavaScript

Los scripts se ejecutan con MicroQuickJS desde el menú Scripts. Puedes guardarlos en SD o LittleFS; el gestor permite seleccionar archivos `.js` y `.bjs`. Las variantes `LITE_VERSION` o `DISABLE_INTERPRETER` no incluyen esta API. Los ejemplos están en [sd_files/scripts/hardware](../sd_files/scripts/hardware).

Las funciones nativas están en `i2c_js.cpp` y `hardware_js.cpp`; `mqjs_stdlib.c` registra las exportaciones. Después de cambiar el registro, la compilación regenera las tablas y los encabezados necesarios. No es un entorno Node.js ni un navegador.

## Reglas comunes

Los pines y parámetros numéricos deben ser enteros finitos y estar dentro del rango admitido. Se validan los GPIO de entrada/salida del chip. Un pin físicamente existente puede estar ocupado por otro periférico: consulta el perfil de placa antes de usarlo. En la expansión 3 en 1, GPIO8/9 están reservados y se rechazan.

Las funciones de datos aceptan cadenas UTF-8 o arrays tipados. Las lecturas devuelven `Uint8Array`, incluso cuando no llega ningún byte. Una lectura corta conserva su longitud real. Los argumentos inválidos y el uso anterior a `begin` generan excepciones. Finaliza con `end()` en un bloque `finally`; el intérprete también libera los recursos después de terminar el script y sus temporizadores.

No uses el mismo periférico desde un script en segundo plano y un menú de hardware a la vez. El HAL permite compartir los controladores con SD/pantalla, pero no asigna propiedad exclusiva de cada dispositivo a aplicaciones concurrentes. UART utiliza UART1, compartido con otros módulos que usan `Serial1`; no modifica la consola USB ni UART2 de GPS.

## I²C

| Llamada | Resultado |
| --- | --- |
| `i2c.begin(sda, scl, hz)` | `true`; frecuencia entre 1 kHz y 1 MHz |
| `i2c.scan()` | Array de direcciones que responden, de 1 a 126 |
| `i2c.write(address, data, stop=true)` | Código Wire: 0 correcto; distinto de 0 indica error |
| `i2c.read(address, length)` | `Uint8Array` de los bytes recibidos |
| `i2c.writeRead(address, data, length, waitMs=0)` | `Uint8Array`, o código Wire negativo si falla la escritura |
| `i2c.end()` | Libera la referencia al bus de usuario |

Máximo 128 bytes por escritura o lectura; espera adicional máxima de 1000 ms. `writeRead` usa una condición repeated START cuando habrá lectura. Una escritura incompleta devuelve 1 o -1, respectivamente. Cuando el bus coincide con el del sistema, no se cambia su reloj ni se detiene al finalizar.

```js
i2c.begin(2, 1, 100000);
try {
    var addresses = i2c.scan();
    serial.println(JSON.stringify(addresses));
} finally {
    i2c.end();
}
```

## SPI

| Llamada | Resultado |
| --- | --- |
| `spi.begin(sck, miso, mosi, cs, hz=1000000, mode=0)` | `true`, o excepción si no queda controlador disponible |
| `spi.transfer(data)` | `Uint8Array` de igual longitud con los bytes recibidos |
| `spi.end()` | Deselecciona el dispositivo sin detener el bus compartido |

Pines distintos; frecuencia de 1 kHz a 40 MHz; modos 0–3; orden MSB primero. Una transferencia admite hasta 4096 bytes. CS pasa a LOW durante la transacción y vuelve a HIGH al finalizar. Un bloque vacío no activa CS.

Ejemplo de lectura de un registro de estado del CC1101 en el módulo 3 en 1:

```js
spi.begin(40, 39, 14, 15, 1000000, 0);
try {
    var response = spi.transfer(new Uint8Array([0xF5, 0x00]));
    serial.println(response[1]);
} finally {
    spi.end();
}
```

La API transfiere bytes; los comandos y registros los define el periférico. Cierra primero el menú que controle ese radio.

## UART

| Llamada | Resultado |
| --- | --- |
| `uart.begin(baud, rx, tx)` | `true`; formato 8N1, de 300 a 5.000.000 baudios |
| `uart.available()` | Número de bytes disponibles |
| `uart.write(data)` | Número de bytes escritos |
| `uart.read(length=1, timeoutMs=0)` | `Uint8Array`; máximo 4096 bytes y 5000 ms |
| `uart.end()` | Detiene UART1 si el script la inició |

RX y TX deben ser distintos. Sin espera, `read` consume únicamente lo ya disponible. Con espera, termina al completar la longitud o vencer el plazo; cede ejecución mientras no hay datos.

Para una prueba de bucle local, con el kit de radios desconectado, conecta RX1 a TX2 en Grove y ejecuta:

```js
uart.begin(115200, 1, 2);
try {
    uart.write(new Uint8Array([0, 85, 170, 255]));
    var received = uart.read(4, 1000);
    serial.println(JSON.stringify(received));
} finally {
    uart.end();
}
```

Esta UART es una ampliación para periféricos serie. Los radios SPI de la expansión no se controlan por ella.
