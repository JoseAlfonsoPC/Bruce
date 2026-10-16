# Desarrollo y mantenimiento

## Preparación y verificaciones

Sigue [INSTALACION](INSTALACION.md) y trabaja desde la raíz del repositorio. Las dependencias de construcción del ordenador están en `requirements-build.txt`; las del firmware en `platformio.ini` y los INI de cada placa.

```sh
python -m unittest discover -s tests -v
python tools/check_docs.py
python -m compileall -q build.py patch.py gen_mqjs_headers.py tools tests
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer-adv-3in1
```

Las compilaciones deben hacerse en serie porque comparten cabeceras generadas y parches del SDK. No ejecutes simultáneamente dos procesos que escriban esas rutas. No edites `.pio/libdeps` como solución permanente: registra la corrección en las fuentes, versiones o scripts del proyecto.

## Añadir una función

1. Sitúa la lógica en `src/modules/<grupo>` y utiliza los servicios de `src/core` para menús, almacenamiento y buses.
2. Comprueba capacidades con las banderas existentes y contempla módulos ausentes, memoria insuficiente y salida al menú.
3. Registra el acceso en `src/core/menu_items` y, cuando tenga sentido, en los comandos serie.
4. Para exportar JavaScript, añade la función nativa, su cabecera a `INCLUDES` del generador y su registro a `mqjs_stdlib.c`. Regenera las tablas compilando.
5. Actualiza documentación y pruebas cuando cambie un contrato o una ruta crítica.

La limpieza de SPI no debe detener el controlador de SD o pantalla. La limpieza de I²C no debe apagar el controlador del teclado. No hagas transferencias con valores negativos convertidos a enteros sin signo ni esperas ilimitadas en funciones llamadas desde scripts.

## Placas y perfiles

Consulta [boards/README](../boards/README.md). Un perfil incluye el manifiesto de MCU, los INI, `pins_arduino.h` e `interface.cpp`. Reutiliza los entornos existentes para variaciones del mismo dispositivo; documenta los cambios de cableado. El perfil específico de la expansión 3 en 1 permite seleccionar explícitamente su multiplexación y sus límites.

Para añadir nuevas funciones NRF24 al perfil 3 en 1, coordina todos sus accesos CE/CS y SPI con el teclado, incluidos errores, inicialización y cierre. Prueba repetidamente la salida con teclado y el acceso a SD. Utiliza `nrfRadio()->metodo()`; no conserves el puntero interno después de terminar el objeto de acceso. El controlador RF24 es privado. Evita mantener el bloqueo durante menús o esperas de entrada, y conserva el orden de bloqueo: pines compartidos antes de SPI.

## Recursos generados

- `include/current_year.h` y `include/webFiles.h` se generan al compilar.
- `mqjs_stdlib.h`, `lib/mquickjs_headers/mquickjs_atom.h` y su firma son salidas del generador que el proyecto conserva en Git; deben corresponder al registro actual.
- `.pio/`, `.venv/`, los binarios y sus sumas quedan fuera del código versionado.

Para regenerar únicamente WebUI, `python tools/web_assets.py`. La compresión conserva los archivos originales y usa cabeceras gzip sin fecha ni nombre del equipo. No necesita el servicio de minificación usado anteriormente.

## Entrega

Compila el entorno solicitado, revisa el tamaño real de la partición, conserva `.elf` para diagnóstico y entrega el binario combinado con su SHA-256. Registra revisión de fuentes, perfil y versiones. El éxito de compilación no sustituye al ensayo con hardware. Actualiza [ESTADO](ESTADO.md) y la matriz de [PRUEBAS](PRUEBAS.md) con evidencia real.

El proyecto mantiene licencia AGPL y atribuciones de terceros. Consulta [LICENSE](../LICENSE) y [THIRD_PARTY](../THIRD_PARTY.md) antes de distribuir cambios.
