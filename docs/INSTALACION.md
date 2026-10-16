# Instalación y compilación

## Requisitos

- Python 3.10 o superior, compatible con la plataforma seleccionada. Esta entrega utiliza Python 3.11.
- Git y un compilador C/C++ nativo. En macOS, las herramientas de Xcode; en Debian/Ubuntu, `build-essential`.
- Acceso a Internet en la primera compilación para descargar pioarduino, el SDK, las herramientas ESP32 y las bibliotecas.
- Para instalar: Cardputer ADV, un cable USB de datos y el puerto serie correspondiente.

Las pruebas del ordenador requieren Python y el compilador C++, sin PlatformIO ni placa. La primera descarga del SDK ocupa varios GB; reserva espacio suficiente. La plataforma está fijada en `platformio.ini`; algunas bibliotecas siguen referencias de Git o intervalos de versiones, por lo que no se garantiza reproducibilidad binaria de todas las dependencias.

## Entorno local

Desde la raíz del repositorio, en macOS/Linux:

```sh
python3.11 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-build.txt
export PLATFORMIO_CORE_DIR="$PWD/.pio/core"
pio --version
python -m unittest discover -s tests -v
```

En Windows PowerShell, usa Python compatible y las herramientas de compilación C++:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements-build.txt
$env:PLATFORMIO_CORE_DIR = "$PWD/.pio/core"
pio --version
```

La generación de MicroQuickJS compila una herramienta nativa y le pide producir tablas de destino de 32 bits. No requiere un ejecutable de ordenador de 32 bits. Puedes indicar el compilador con `MQJS_HOST_CC` o `mqjs_host_cc` en PlatformIO.

## Elegir la configuración

```sh
pio project config
pio run -e m5stack-cardputer-adv-3in1
```

El perfil `m5stack-cardputer-adv-3in1` aplica el cableado y la multiplexación de GPIO8/9 de la expansión; selecciónalo explícitamente con `-e m5stack-cardputer-adv-3in1`. `pio run` conserva el perfil general `m5stack-cardputer` como configuración predeterminada del proyecto oficial.

Se generan:

| Archivo | Contenido |
| --- | --- |
| `.pio/build/<entorno>/firmware.elf` | Ejecutable con símbolos de depuración |
| `.pio/build/<entorno>/firmware.bin` | Aplicación sin las otras imágenes de arranque |
| `Bruce-<entorno>.bin` | Imagen combinada para escribir en dirección `0x0` |
| `Bruce-<entorno>.bin.sha256` | Suma SHA-256 de la imagen combinada |

`build.py` usa las direcciones y segmentos de PlatformIO, comprueba el tamaño de la aplicación contra `partitions.bin` y rechaza segmentos ausentes o solapados. Un error devuelve un estado de fallo; el binario de una compilación anterior puede seguir existiendo y no debe confundirse con una entrega nueva.

Para reconstruir únicamente la imagen combinada de artefactos existentes:

```sh
pio run -e m5stack-cardputer-adv-3in1 -t build-firmware
```

## Instalar y abrir la consola

Antes de sustituir firmware, conserva los archivos de usuario y las configuraciones de la SD. Conecta la placa y localiza el puerto:

```sh
pio device list
pio run -e m5stack-cardputer-adv-3in1 -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

Sustituye el puerto por el real (`COM4` en Windows, por ejemplo). Para entrar en descarga en Cardputer ADV, desconecta el USB, pon el interruptor en OFF, mantén G0 pulsado, conecta el USB con un cable de datos y suelta G0. Es el [procedimiento de M5Stack](https://docs.m5stack.com/en/arduino/m5cardputer/program). Tras instalar, pulsa RST. Cierra otros monitores que estén usando el puerto. Si el equipo no aparece ni como dispositivo USB, comprueba el cable y prueba un puerto directo del ordenador.

La imagen combinada se puede instalar con el esptool gestionado por la plataforma o con una instalación compatible de [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html):

```sh
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX write-flash 0x0 Bruce-m5stack-cardputer-adv-3in1.bin
shasum -a 256 -c Bruce-m5stack-cardputer-adv-3in1.bin.sha256
```

La imagen combinada y la aplicación suelta tienen destinos diferentes. Para la aplicación suelta, deja que PlatformIO determine la dirección de carga.

### Dispositivo con Launcher y aplicaciones existentes

Antes de grabar una imagen combinada o ejecutar la carga del perfil, comprueba la tabla de particiones del dispositivo si quieres conservar Launcher y sus aplicaciones. La imagen combinada incluye relleno entre segmentos y la tabla del perfil; escribirla en `0x0` sustituye el esquema existente y puede sobrescribir configuración y aplicaciones.

En la unidad del usuario se encontró `brucea`, una partición OTA de 4.128.768 bytes en `0x170000`. La instalación de esta sesión guardó una copia completa de 8 MB y escribió exclusivamente `.pio/build/m5stack-cardputer-adv-3in1/firmware.bin` en esa partición, con verificación posterior. Se conservaron el cargador de Launcher, NVS, la tabla de particiones y las otras aplicaciones. Después se seleccionó `brucea` para arrancar usando el selector OTA y se verificó la respuesta de Bruce por USB. Las direcciones de esta unidad no deben suponerse para otros dispositivos: consulta [el registro de validación](validacion.json).

## Docker

```sh
docker compose build
PIO_ENVS=m5stack-cardputer-adv-3in1 docker compose run --rm platformio_build
```

El contenedor conserva las herramientas en `pio_home` y los objetos en `pio_build`. Puedes pasar una lista de entornos separada por espacios mediante `PIO_ENVS`. `PIO_JOBS` controla cuántos entornos se ejecutan a la vez; utiliza `1` para trabajar con los generadores compartidos de este repositorio. Docker debe estar arrancado y su socket debe ser accesible.

## Problemas habituales

| Síntoma | Acción |
| --- | --- |
| Python 3.9 rechazado | Recrear `.venv` con Python 3.10 o superior |
| PlatformIO incompatible | Instalar `requirements-build.txt` en el entorno activado |
| No se encuentra `gcc` | Instalar compilador nativo o indicar `MQJS_HOST_CC` |
| Error de descarga | Revisar conexión/certificados y repetir el mismo entorno |
| Errores de FastLED con `FP` | Usar la versión exacta del `platformio.ini`; no actualizarla aisladamente |
| NRF24 no detectado | Comprobar perfil 3 en 1 y consultar su guía de hardware |
| Teclado bloqueado | Revisar perfil, montaje y ausencia de acceso paralelo a GPIO8/9 |
| No hay puerto USB | Probar cable de datos, modo de descarga y cerrar otros monitores |
