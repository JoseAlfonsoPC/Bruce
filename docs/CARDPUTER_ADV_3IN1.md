# Cardputer ADV y expansión 3 en 1

## Identificación

La fotografía proporcionada muestra una PCB morada **V1.0**, con módulos Ashining A32, A07 y A01, y tres conectores marcados **LoRa / 433 / NRF**. La referencia LoRa visible y la banda 850–931 MHz corresponden al **A32-S900A22S2a, basado en SX1262**, según [Ashining](https://www.ashiningtech.com/products/00000003-0030-0000-0000-000000000030). La guía de [JosephCGS](https://github.com/JosephCGS/Cardputer-ADV-CC1101-NRF24L01-LoRa) documenta la expansión del ADV con estos tres radios.

La foto permite identificar los módulos, pero no certificar continuidad eléctrica, revisión interna ni funcionamiento de una unidad concreta. El perfil de esta entrega sigue el cableado publicado para esa expansión. No equivale al Cap LoRa oficial de M5Stack con GPS ni al accesorio de dos radios.

## Perfil y conexiones

Compila `m5stack-cardputer-adv-3in1`. Su configuración hereda Cardputer y se encuentra en [el archivo del perfil](../boards/m5stack-cardputer/m5stack-cardputer-adv-3in1.ini).

| Señal | GPIO ADV | Uso |
| --- | --- | --- |
| SCK | 40 | SPI compartido por radios y SD |
| MISO | 39 | SPI compartido |
| MOSI | 14 | SPI compartido |
| SD CS | 12 | Selección de SD |
| CC1101 CS / GDO0 | 15 / 13 | Radio marcado «433» |
| NRF24 CS / CE | 9 / 8 | Radio de 2,4 GHz |
| LoRa CS / RESET / IRQ | 5 / 3 / 4 | SX1262; el nombre histórico `LORA_DIO0` se interpreta como IRQ del módulo |
| TCA8418 SDA / SCL / INT | 8 / 9 / 11 | Controlador del teclado, dirección `0x34` |

La guía de la expansión no proporciona una conexión BUSY para SX1262. Este perfil no inventa un GPIO para ella; RadioLib opera con el valor no conectado y el firmware informa esa condición. Confirma inicio, recepción y retorno al menú en la prueba física.

## Por qué hace falta multiplexación

GPIO8/9 sirven al teclado como bus I²C y al NRF24 como CE/CS. La implementación alterna ambas funciones y coordina su acceso con un mutex recursivo. Cada llamada al controlador RF24 adquiere ese bloqueo hasta terminar. Mientras el teclado o el códec usan I²C, se detiene el tráfico SPI compartido; al terminar, CS queda en HIGH y CE recupera el nivel que tenía antes. Así una tecla no deja parada la radio al forzar CE a LOW. Los bucles dejan tiempo para procesar la entrada del teclado.

La recuperación automática genérica de I²C se omite en este perfil porque interpretaría el CE bajo como SDA bloqueado. Los scripts de I²C/SPI/UART rechazan GPIO8/9 en el perfil 3 en 1. Usa Grove GPIO2/1 para dispositivos I²C externos. El soporte se basa en la adaptación de [leandrofleury, PR 2827](https://github.com/BruceDevices/firmware/pull/2827), revisada para este árbol.

## Funciones y límites de esta integración

- CC1101: el perfil aplica CS15 y GDO0=13; utiliza los menús RF existentes.
- LoRa: un archivo nuevo utiliza SX1262 y 868,1 MHz. Una selección guardada previamente en `/lora_settings.json` puede prevalecer; revisa el selector de radio al abrir Chat y la opción Change Frequency. El perfil limita LoRa a 850–931 MHz.
- NRF24: están disponibles **Hardware check, Spectrum, MouseJack y NRF Jammer**, además de la API del intérprete. Todos utilizan acceso coordinado al controlador. Esto sustituye la restricción anterior a Spectrum. La compilación y las pruebas de concurrencia no sustituyen a la comprobación física de cada modo.
- SD, teclado y audio deben comprobarse también después de entrar y salir de los radios.

El espectro NRF24 indica actividad recibida; no es un analizador calibrado ni identifica por sí solo un protocolo. La banda 850–931 MHz describe la capacidad del componente LoRa; elige una frecuencia y una antena adecuadas al entorno de ensayo.

## Information y MouseJack

`Information` muestra el transporte, el resultado de una comprobación SPI sin transmitir y los pines efectivos. Se cierra con Esc; mantener Enter/G0 no la cierra al abrirla. Sustituye a la pantalla anterior, que solo mostraba un aviso y podía cerrarse con un evento de teclado pendiente.

En MouseJack, `NRF Mode: SPI` abre un selector visible. El perfil de esta expansión solo ofrece `SPI (ADV 3-in-1)`; no hay un puente UART en la placa. Esc o Back conserva la selección anterior y vuelve a MouseJack. La selección automática usada por las API continúa sin abrir menús.

`Scan Devices` es un receptor pasivo de tramas ESB de los protocolos Microsoft/Logitech implementados. El barrido actual recorre los canales 2–84 a 2 Mbps. No enumera dispositivos WiFi, Bluetooth ni todos los periféricos que utilizan 2,4 GHz. El modelo y la actividad del periférico influyen en el resultado; reconocer una trama no prueba que un dispositivo sea vulnerable.

| Indicador | Interpretación |
| --- | --- |
| `Radio: not detected on SPI` en Information | No se ha confirmado comunicación con el módulo; revisar conexión y perfil antes del escaneo |
| `RX: 0`, `Found: 0` | Sin capturas en las ventanas del barrido; no demuestra que no existan dispositivos cercanos |
| `RX` aumenta y `CRC: 0` | Capturas crudas sin ninguna trama ESB validada; pueden ser ruido, otro protocolo o una recepción incompleta |
| `CRC` aumenta y `Found: 0` | Hay capturas que superan el CRC ESB, pero no coinciden con las firmas Microsoft/Logitech implementadas; incluye ACK vacíos |
| `CRC: 1`, `Found: 0` en una observación aislada | No permite atribuir la captura al Logitech ni concluir compatibilidad; registrar RX, duración y si CRC sigue aumentando con actividad del periférico |
| `Found` aumenta | Se han identificado dispositivos mediante tramas que superan el CRC y los patrones implementados |

Cada ventana de recepción de MouseJack conserva el bloqueo de CE/CS hasta detener la escucha; después libera el bloqueo para decodificar, dibujar y atender el teclado. Un fallo de inicio conserva su mensaje de error y no se reemplaza por «No devices found». Esc permite salir de la lista de dispositivos y los submenús vuelven a MouseJack.

Después de que el usuario notificara `RX` creciente con `Found: 0`, se revisó la captura frente al [receptor original de Bastille](https://github.com/BastilleResearch/nrf-research-firmware/blob/master/src/radio.c) y su [dirección de captura](https://github.com/BastilleResearch/nrf-research-firmware/blob/master/src/radio.h). Se utiliza un solo pipe con los bytes `AA 00`, ancho abreviado de dos bytes, CRC por software y payload fijo; se cierran los demás pipes y se desactiva el payload dinámico. Las seis direcciones anteriores (`55 55`, `AA AA`, etc.) no conservaban la configuración de referencia. Se vacía la FIFO al cambiar de canal para evitar atribuir capturas al canal siguiente. La técnica usa un valor reservado de `SETUP_AW`; su funcionamiento depende del chip o clon.

El decodificador pasivo se prueba por separado con tramas generadas mediante división polinómica independiente: ambos alineamientos, longitudes 0–23, PID y no-ack, truncamiento y errores de un bit en dirección, control, datos y CRC. Esta validación no demuestra todavía que el periférico concreto del usuario sea reconocido; comprueba los contadores en la revisión instalada.

El usuario notificó después `CRC: 1` y `Found: 0`. El resultado sigue sin identificar su periférico. Para repetir la prueba, confirma que el Logitech depende del receptor USB desconectándolo brevemente; con él conectado, mantén actividad durante un minuto con el ADV a unos 50 cm y anota `RX`, `CRC` y `Found`. El CRC no aporta por sí solo una firma de fabricante, y las respuestas vacías no se añaden como dispositivos.

En la repetición, el usuario confirmó que el Logitech deja de funcionar al desconectar el receptor; tras el minuto de actividad volvió a notificar `CRC: 1`, `Found: 0`. Para obtener información suficiente se añade **MouseJack → Scan Info**. También se abre al detener un barrido que no identifica dispositivos y se cierra con Esc. Conserva los resultados hasta el siguiente escaneo o reinicio; abrirla no reinicia la radio.

| Campo de Scan Info | Significado |
| --- | --- |
| `RX` / `CRC` / `Found` | Capturas, capturas que superan CRC y dispositivos que coinciden con las firmas |
| `Empty` / `Data` | Capturas validadas sin payload y con payload; `Empty + Data = CRC` |
| `Flat` | Capturas crudas en las que todos los bytes son idénticos; permite comparar cuántas capturas tienen este patrón, sin calificarlas automáticamente como dispositivos |
| `Last LEN` / `CH` | Longitud y canal de la última captura validada con datos |
| `Header` / `Sum8` | Primeros dos bytes y suma de los bytes del último payload con datos, módulo 256; una suma cero por sí sola no demuestra que sea Logitech |

El resumen también se emite por USB al detener el barrido o abrir Scan Info. El diagnóstico conserva metadatos, sin guardar direcciones ni el cuerpo completo de los informes de entrada. Un payload de un byte muestra `00` en la segunda posición de Header; `LEN` distingue esa ausencia. Una respuesta vacía conserva los metadatos de la última captura con datos.

Durante la prueba de esta revisión, el usuario volvió a notificar `CRC: 1`, `Found: 0` y después un reinicio del ADV mientras escaneaba. No se recibió un resumen completo de Scan Info antes del reinicio. Se extrajo la partición de coredump sin escribir firmware y se verificó su CRC32. El identificador de aplicación de ese volcado, `e974e6455`, coincide con Launcher original; el ELF de Bruce instalado tiene prefijo `7db8554d4`. Ese volcado no identifica la causa del reinicio notificado. Bruce volvió a responder a `info` tras la extracción.

La investigación posterior encontró arranques `USB_UART_CHIP_RESET` al utilizar el monitor anterior, también fuera del escaneo. Abrir/cerrar un puerto puede afectar DTR/RTS según el sistema y el controlador, como documenta [Espressif](https://github.com/espressif/esptool/blob/master/docs/en/advanced-topics/boot-mode-selection.rst). La monitorización utilizada en la sesión podía interferir con la prueba. Se añadió [serial_monitor.py](../tools/serial_monitor.py), que no cambia explícitamente DTR/RTS y desactiva HUPCL. En esta unidad, una comprobación conservó el uptime de 100 a 105 segundos al cerrar y volver a abrir, sin mensajes de arranque.

Para registrar sin utilizar el monitor anterior, ajusta el puerto y elige una ruta de salida nueva:

```sh
python tools/serial_monitor.py --port /dev/cu.usbmodem201301 --seconds 180 --output /tmp/bruce-nrf-session.log
```

Esta utilidad funciona en macOS/Linux, no envía comandos al firmware y muestra en consola solo líneas de diagnóstico. El archivo completo se crea con permisos `0600` y puede contener configuración emitida durante el arranque; se conserva localmente. Al finalizar el tiempo de lectura, el escaneo sigue bajo el control del ADV. La herramienta no sobrescribe registros existentes. La comprobación de uptime valida el método de conexión en esta unidad; la estabilidad del escáner se registra por separado.

Con el método corregido, un barrido controlado de **120 segundos** completó la observación y el cierre mediante Esc remoto sin reinicios: `RX: 7229`, `CRC: 5`, `Empty: 5`, `Data: 0`, `Flat: 38`, `Found: 0`. El uptime continuó de `00:03:41` a `00:05:50`. Las capturas que superaron CRC en ese ensayo no contenían payload para reconocer una firma; no permiten atribuir un modelo ni demostrar incompatibilidad del Logitech.

Después de que el usuario confirmara actividad del Logitech se recuperó otro resumen: `RX: 10668`, `CRC: 7`, `Empty: 7`, `Data: 0`, `Flat: 49`, `Found: 0`. El inicio de ese barrido no quedó delimitado por el control remoto, por lo que no se le atribuye una duración de 120 segundos ni se calcula una tasa de recepción. Tras detenerlo, el uptime era `00:11:30`; se volvió a abrir Scan Info y se comprobó que conservaba los mismos contadores. Sigue sin recibirse un payload válido que permita investigar la firma del periférico. Las capturas vacías no se atribuyen automáticamente al Logitech y la causa del reinicio originalmente notificado permanece sin confirmar.

## Puesta en marcha

1. Con el ADV apagado, monta el módulo respetando la orientación del conector. Coloca cada antena en el conector que corresponda a su banda.
2. Instala el binario del perfil 3 en 1 y arranca sin ejecutar scripts automáticamente.
3. Comprueba escritura, navegación y lectura de la SD.
4. Abre RF y comprueba la inicialización del CC1101; vuelve al menú.
5. En LoRa selecciona SX1262 y la frecuencia de ensayo. Valida recepción con otro equipo configurado con los mismos parámetros.
6. Abre **NRF24 → Hardware check**. Debe mostrar `SPI: OK`, `RX CE: HIGH` y `Failures: 0` mientras pulsas teclas. Sal con Esc; esta prueba utiliza recepción con auto-ack desactivado.
7. Abre Spectrum y comprueba varias entradas/salidas, SD y audio. Para el bloqueo notificado en NRF Jammer, verifica el teclado y su salida en el banco de ensayo del usuario después de instalar el nuevo binario. El modo interrumpe temporalmente el uso de los pines de radio cuando atiende el teclado.

No hace falta borrar automáticamente la configuración. Si procedes de firmware del vendedor o de otro cableado, conserva copias de `bruce.conf`, `brucePins.conf` y `lora_settings.json`; revisa los valores antes de restaurarlos. El [plan de aceptación](PRUEBAS.md) registra qué comprobaciones faltan en hardware.

## Corrección de NRF Jammer

La revisión anterior solo ofrecía Spectrum en este perfil. La corrección centraliza todas las llamadas a RF24 en `nrfRadio()`, conserva CE durante el cambio de I²C y cede un tick en el bucle de Jammer para atender teclas. El objeto RF24 queda privado para impedir accesos directos sin bloqueo.

Si el equipo muestra otros modos con el binario anterior, comprueba el archivo instalado: el perfil general y el específico de la expansión tienen cableados diferentes.

El 16 de octubre de 2026 se instaló este perfil por USB en la unidad del usuario, después de guardar una copia completa de sus 8 MB. La unidad utiliza Launcher y varias aplicaciones: se actualizó únicamente `brucea` en `0x170000`, conservando la tabla de particiones, NVS y las otras aplicaciones. La revisión con Scan Info ocupa 4.117.648 de los 4.128.768 bytes disponibles en esa partición. Se verificó la escritura y se seleccionó Bruce como aplicación de arranque mediante el selector OTA, conservando su otra copia. La actualización del diagnóstico conservó los primeros 64 KiB byte a byte, incluidos cargador, tabla, NVS y selector OTA.

Tras reiniciar, el comando `info` respondió con `Bruce vdev`, SDK `v5.5.4-dirty` y `M5Stack Cardputer`, sin panic detectado durante la comprobación. La comprobación del intérprete confirmó `nrf24.begin(76) = true` e `isConnected() = true` en la revisión con Scan Info. No se ha activado un modo de transmisión desde el ordenador. La recepción de capturas crudas fue notificada por el usuario; el reconocimiento de su Logitech tras esta actualización y las pruebas restantes de teclado y RF siguen pendientes. El registro está en [validacion.json](validacion.json).
