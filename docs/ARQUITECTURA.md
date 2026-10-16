# Arquitectura

Bruce es un firmware C++ sobre Arduino-ESP32 y ESP-IDF, compilado con PlatformIO/pioarduino. La selección de placa combina un manifiesto JSON, un entorno INI, sus pines y su implementación de entrada/alimentación/pantalla.

## Organización

| Ruta | Responsabilidad |
| --- | --- |
| `src/main.cpp` | Instancias globales, arranque y bucle principal |
| `src/core/` | Menús, configuración, buses, almacenamiento, consola, energía y WebUI |
| `src/core/menu_items/` | Entrada de cada módulo en los menús |
| `src/modules/` | WiFi, BLE, RF, NRF24, LoRa, RFID, IR, GPS, Ethernet y utilidades |
| `src/modules/bjs_interpreter/` | MicroQuickJS y sus enlaces nativos |
| `boards/` | Configuraciones y adaptaciones por dispositivo |
| `boards/_boards_json/` | Manifiestos de MCU, flash y capacidades |
| `include/` | Cabeceras compartidas y recursos generados |
| `lib/` | Bibliotecas incluidas y tablas auxiliares |
| `embedded_resources/web_interface/` | Fuentes de WebUI |
| `sd_files/` | Recursos opcionales para almacenamiento |
| `tools/` | Utilidades comprobables sin ESP32 |
| `tests/` | Pruebas de empaquetado, recursos y contratos de buses |
| `.github/workflows/` | Comprobaciones y compilaciones remotas |

## Secuencia de arranque

1. Abre la consola, prepara memoria y comunica el estado PSRAM.
2. Ejecuta la adaptación GPIO de la placa e inicializa pantalla si existe.
3. Monta LittleFS y SD y carga ajustes de usuario y pines.
4. Aplica pantalla, reloj, LED y ajustes de conectividad.
5. Ejecuta la configuración posterior de la placa, incluida la detección del teclado ADV.
6. Inicia tareas de entrada, consola y, si procede, conectividad y aplicación de inicio.
7. El bucle principal presenta el menú o cede al intérprete mientras ejecuta un script.

Las funciones débiles `_setup_gpio`, `_pre_storage_gpio`, `_post_setup_gpio`, `InputHandler` y las funciones de batería permiten que cada placa adapte el comportamiento común. La consolidación completa de entradas y alimentación de todas las placas sigue en la hoja de ruta.

## Buses y recursos compartidos

`bus_HAL.cpp` decide qué controlador I²C/SPI utilizar. Diferencia el I²C del sistema del bus de periféricos de usuario y conserva el controlador de pantalla/SD. La disponibilidad de un controlador no implica disponibilidad exclusiva del dispositivo conectado.

El perfil ADV 3 en 1 añade un mutex para alternar GPIO8/9 entre teclado y NRF24. El acceso I²C del teclado/códec toma además el bloqueo de la transacción SPI compartida. Todos los clientes RF24 acceden mediante `nrfRadio()`, un objeto temporal que mantiene el mutex durante la llamada completa al controlador. El mutex es recursivo para permitir proteger ventanas de recepción que incluyen varias llamadas. Tras el acceso I²C se restaura el nivel anterior de CE. El SPI del intérprete utiliza el mismo orden de bloqueo.

UART del intérprete utiliza `Serial1`; GPS y algunos módulos emplean otras instancias o UART1. La aplicación debe cerrar el propietario anterior antes de cambiar de función.

## Intérprete

`mqjs_stdlib.c` declara módulos, clases y funciones. `gen_mqjs_headers.py` compila el generador del motor en el ordenador y produce tablas para un destino de 32 bits. El intérprete crea un contexto en heap/PSRAM, ejecuta el script, procesa temporizadores y libera contexto y recursos de buses al finalizar.

Los punteros a datos del motor se copian antes de hacer transferencias. La exploración I²C mantiene el resultado protegido frente al recolector mientras rellena el array. Consulta [SCRIPTING](SCRIPTING.md) para contratos y límites.

## Cadena de construcción

1. PlatformIO resuelve el entorno y descarga sus dependencias.
2. `patch.py` aplica la adaptación del archivo WiFi utilizando el objcopy del SDK. Conserva el original si la herramienta falla. También genera WebUI localmente.
3. Los scripts previos generan el año y las tablas MicroQuickJS y aplican parches de compatibilidad de bibliotecas.
4. `flto_prep.py` ajusta las opciones de enlace LTO.
5. Se compilan firmware y bibliotecas y se genera `partitions.bin`.
6. `build.py` valida la partición de aplicación y combina exactamente los segmentos indicados por PlatformIO. Produce el binario final y su suma SHA-256.

Los generadores escriben algunos archivos compartidos por entornos. Las compilaciones de entornos diferentes deben ejecutarse en serie en un mismo checkout. Usa checkouts separados si necesitas compilaciones concurrentes.
