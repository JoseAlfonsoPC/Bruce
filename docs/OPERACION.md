# Operación y configuración

## Arranque y navegación

Cardputer y ADV comparten la interfaz de menú. El firmware detecta el controlador TCA8418 del ADV; si no responde, el perfil general recurre al teclado del Cardputer original. Tras arrancar, verifica navegación, selección, retroceso y escritura antes de abrir un módulo de radio. La consola USB trabaja a **115200 baudios** y permite consultar el dispositivo sin depender de la pantalla.

Los menús se construyen en `src/core/main_menu.cpp`. La presencia de una opción no demuestra que el periférico esté instalado.

| Grupo | Función general | Dependencia |
| --- | --- | --- |
| WiFi y BLE | Conectividad, exploración y herramientas del firmware | Radios del ESP32 y memoria disponible |
| RF | Sub-GHz y gestión de señales | CC1101 o módulos RF configurados |
| NRF24 | Espectro de 2,4 GHz y herramientas existentes | NRF24 externo; consultar límites del perfil 3 en 1 |
| LoRa | Configuración y mensajería | SX1262/SX1276 compatible |
| IR | Recepción/transmisión de infrarrojos | Emisor/receptor y GPIO correctos |
| RFID | Lectura, archivos y operaciones según módulo | PN532, ST25R3916 u otros controladores admitidos |
| Ethernet y GPS | Herramientas específicas | Módulos externos correspondientes |
| FM | Emisión y medición FM mediante Si4713 | Si4713 independiente; sin respuesta en I²C muestra `Si4713 not found` y permite volver con una tecla |
| Files y Scripts | Archivos y JavaScript | SD/LittleFS; intérprete en versión completa |
| Clock, Others y Config | Reloj, utilidades y ajustes | Capacidades de la placa |

El catálogo detallado de funciones preexistentes se conserva en [README](../README.md) y la [wiki del proyecto](https://wiki.bruce.computer/). Este manual no convierte las opciones pendientes de la hoja de ruta en funciones disponibles.

## Consola serie

Conecta el monitor y envía comandos con salto de línea. Usa `help` para ver la sintaxis de la versión instalada:

```text
help
info
uptime
free
settings bright
settings bright 70
storage list /
```

Para conectar tu red y abrir WebUI:

```text
wifi add "MiRed" "MiClave"
wifi on
webui
```

`nav`, `options` y las funciones de pantalla permiten controlar menús; sus argumentos se registran en `src/core/serial_commands`. Algunos comandos dependen de pantalla, audio, USB HID o de la versión completa. `factory_reset` renombra las configuraciones de usuario como copia de respaldo y reinicia; no equivale a formatear toda la SD.

## WebUI

Puede utilizar una red WiFi existente o el punto de acceso del dispositivo. Los valores iniciales del código son:

| Campo | Valor inicial |
| --- | --- |
| SSID del punto de acceso | `BruceNet` |
| Clave del punto de acceso | `brucenet` |
| IP del punto de acceso | `172.0.0.1` |
| Usuario WebUI | `admin` |
| Contraseña WebUI | `bruce` |
| Nombre mDNS | `bruce.local` |

Usa la IP que muestre la placa; mDNS depende de la red y del ordenador. Cambia las credenciales desde configuración o consola antes de compartir acceso:

```text
settings webUI "miusuario,mi-clave"
settings wifiAp "MiBruce,otra-clave"
```

La interfaz utiliza HTTP y una sesión en cookie. Los archivos de configuración pueden contener contraseñas de red y tokens en texto legible: conserva las copias fuera de repositorios y revisa los registros antes de compartirlos. WebUI administra almacenamiento y ejecuta comandos del dispositivo; úsala en una red de confianza.

Los recursos HTML/CSS/JS se mantienen en `embedded_resources/web_interface`. `patch.py` los comprime localmente mediante `tools/web_assets.py` y genera `include/webFiles.h`. Una carpeta `/BruceWebUI/` en almacenamiento permite servir archivos personalizados según la lógica de `webInterface.cpp`.

## Almacenamiento y ajustes

| Ruta del dispositivo | Propósito |
| --- | --- |
| `/bruce.conf` | Apariencia, red, credenciales, sonido y aplicaciones de inicio |
| `/brucePins.conf` | Pines y buses por identidad del dispositivo |
| `/lora_settings.json` | Selección y ajustes LoRa |
| `/boot.jpg` o `/boot.gif` | Imagen/animación de arranque opcional |
| `/BruceWebUI/` | Recursos personalizados para WebUI |

Al arrancar se monta LittleFS y se intenta montar SD. Se cargan los ajustes desde el almacenamiento disponible. Los cambios se guardan en LittleFS y se copian a SD cuando corresponde; la configuración de pines contiene entradas asociadas a la MAC. No copies una identidad completa de otra placa sin revisar los pines.

`sd_files/` reúne ejemplos y recursos opcionales. Copia únicamente lo que necesites, manteniendo sus rutas. Para los ejemplos nuevos, consulta [SCRIPTING](SCRIPTING.md). No es necesario incluirlos todos para que el firmware arranque.

## Diagnóstico

La línea de arranque `[PSRAM]` y los mensajes `[RAM]` permiten observar memoria disponible. Cardputer no garantiza PSRAM: evita asumir que WiFi, BLE y scripts grandes podrán coexistir. Una pantalla negra, un reinicio o un módulo no detectado requieren revisar alimentación, perfil, pines y memoria antes de modificar bibliotecas.

Para un informe útil registra: revisión del código, perfil compilado, versión de PlatformIO/Python, pasos mínimos, módulo conectado, mensajes serie y si el problema ocurre sin la SD. No publiques contraseñas ni tokens al adjuntar `bruce.conf` o registros.
