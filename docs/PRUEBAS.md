# Pruebas y aceptación

## Pruebas automatizadas del ordenador

```sh
python -m unittest discover -s tests -v
python tools/check_docs.py
```

Las pruebas usan la biblioteca estándar de Python y un compilador C++17. El ejecutable nativo incorpora AddressSanitizer y UndefinedBehaviorSanitizer. No requieren hardware ni emiten señales de radio.

| Área | Casos cubiertos |
| --- | --- |
| WebUI | Descompresión idéntica al original, reconstrucción de cabecera ausente, orden estable, cambios/renombrados/borrados, nombres inválidos y fuentes vacías |
| Particiones | Aplicaciones factory/OTA, direcciones distintas, entradas inválidas y marcador MD5 |
| Empaquetado | Segmentos ordenados, límites, archivos ausentes/vacíos/solapados, fallo de esptool, conservación del binario previo y SHA-256 |
| Preparación del SDK | Fallo de objcopy sin pérdida del original, copia de respaldo, recuperación de archivo ausente e idempotencia |
| I²C | Uso sin iniciar, errores de pines/argumentos, lectura corta y binaria, NACK, escritura incompleta, reloj de sistema y limpieza |
| SPI | Pines y modo, controlador ausente, datos recibidos, CS inactivo, bloque vacío, exceso de datos y limpieza |
| NRF24 y teclado | Exclusión entre tareas, bloqueo recursivo, restauración de CE alto/bajo y CS, SPI detenido durante I²C y cierre; 1.000 operaciones frente a 200 eventos concurrentes |
| Transporte NRF24 | CE ausente, pines repetidos o inválidos, MISO en entrada, conflictos UART incluyendo CE, consola ocupada y restricción SPI del perfil 3 en 1 |
| Decodificador ESB pasivo y diagnóstico | 32.140 comprobaciones: codificación independiente por bits/división polinómica, ambos alineamientos, longitudes 0–23, PID/no-ack, captura vacía, truncamiento, corrupción de cada bit protegido y estadísticas de ruido/vacíos/datos, metadatos de la última captura e inicio de nuevo escaneo |
| UART | Inicialización, lectura binaria, timeout, escritura, límites y cierre |
| Detección FM | 179 comprobaciones generales y 191 del perfil 3 en 1 sobre el código FM real: ocho puntos de entrada con NACK/error/timeout, controlador no invocado, liberación del bus antes del error, pines inválidos o compartidos, ausencia de bus, fallo de inicio, estado obsoleto y cancelación sin emitir |
| Monitor USB POSIX | Lectura binaria mediante pseudoterminal, HUPCL desactivado, cierre tras errores, duraciones acotadas y filtrado de configuración en consola |

Los buses se simulan para comprobar contratos y límites. Estas pruebas no miden temporización eléctrica, nivel RF ni la respuesta real del módulo.

## Compilaciones

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer-adv-3in1
```

Comprueba que cada ejecución termina con SUCCESS y que existe un binario con su suma válida:

```sh
shasum -a 256 -c Bruce-m5stack-cardputer-adv-3in1.bin.sha256
```

El flujo `Host checks` ejecuta las pruebas y verifica enlaces/compilación de scripts en CI. Los flujos de firmware existentes conservan las otras placas; el perfil 3 en 1 se añade a la comprobación de pull requests. Compilar todas las placas es un trabajo independiente de la aceptación del hardware objetivo.

## Matriz de aceptación física

Los ensayos funcionales que no constan como realizados siguen **pendientes en el dispositivo del usuario**. La instalación USB, la integridad de la grabación, el arranque de Bruce y la inicialización/respuesta SPI del NRF24 están verificados en [el registro](validacion.json). También se completó un barrido controlado de 120 segundos sin reinicios con el monitor USB corregido y salida mediante Esc remoto; el reconocimiento del Logitech y el teclado físico siguen pendientes. Registra fecha, revisión de firmware y resultado; no marques una fila como validada solo porque compila.

| Ensayo | Procedimiento y resultado esperado |
| --- | --- |
| Arranque ADV | Apagar/encender varias veces; menú visible, teclado detectado, sin reinicios |
| Teclado | Escribir una frase, borrar, usar modificadores y salir de un menú |
| SD | Crear, leer y borrar un archivo de prueba; volver a leer después de usar radios |
| Configuración | Cambiar brillo, reiniciar y comprobar persistencia |
| WebUI | Iniciar sesión, listar archivos, cerrar sesión y comprobar que se pide autenticación |
| I²C externo | Ejecutar `i2c_scan.js` con un periférico conocido en Grove; dirección correcta y teclado operativo al terminar |
| SPI CC1101 | Ejecutar `cc1101_status.js`; lectura coherente y regreso al menú sin bloquear SD |
| UART | Ejecutar `uart_loopback.js` con el puente indicado; recibir `00 55 AA FF` |
| CC1101 | Inicializar y recibir una señal de prueba propia en la banda/antena del módulo |
| LoRa SX1262 | Seleccionar SX1262 y frecuencia; intercambiar un mensaje de prueba con un segundo equipo compatible |
| FM en el perfil 3 en 1 | Verificado en el ADV: FM visible; Spectrum y Stop muestran `Si4713 not found`, mantienen el aviso hasta una tecla y permiten volver al menú con Esc remoto. Después, NRF24 Hardware check completó 37 muestras y 0 fallos. No se activaron transmisiones; teclado físico y uso de un Si4713 real siguen pendientes |
| NRF24 Hardware check | Mantener `SPI: OK`, `RX CE: HIGH` y `Failures: 0` mientras se pulsan teclas; salir con Esc |
| NRF24 Information | Ver transporte, respuesta SPI y pines; mantener Enter/G0 al abrirla y comprobar que solo se cierra con Esc |
| MouseJack: modo | Abrir `NRF Mode: SPI`, seleccionar SPI o cancelar; regresar a MouseJack conservando el modo |
| MouseJack: recepción | Revisar `RX`/`CRC`/`Found` con un periférico compatible activo; CRC cuenta tramas validadas y Found firmas reconocidas; anotar tipo de enlace y modelo si está disponible; verificar salida y que un fallo de inicio conserva su error |
| MouseJack: diagnóstico | Verificados el resumen por USB y su conservación al volver a abrir Scan Info: RX 10668, CRC 7, Empty 7, Data 0, Flat 49, Found 0, tras actividad confirmada por el usuario. Duración sin delimitar. Sigue pendiente capturar datos y comprobar LEN/CH y Header/Sum8; salida con Esc remoto comprobada |
| Reinicio notificado en NRF24 | El usuario confirma que ocurrió durante el escaneo; se identificaron reinicios USB inducidos por el monitor anterior. Con la conexión corregida se completaron 120 s de barrido y cierre por Esc remoto sin reinicios. El volcado recuperado pertenece a Launcher y no demuestra un fallo del escáner |
| NRF24 Spectrum | Entrar/salir al menos diez veces, incluyendo ausencia de señal; mantener navegación y respuesta del teclado |
| NRF Jammer: teclado | En el banco de ensayo, cambiar opciones y salir con Esc sin bloqueo; comprobar después SD y teclado |
| Otros modos NRF24 | Verificar inicialización y salida en hardware; no inferir prestaciones RF a partir de las pruebas del multiplexor |
| Radio ausente | Arrancar sin expansión; informar módulo no encontrado sin bloquear indefinidamente |
| Recursos compartidos | Alternar SD, radio, teclado y audio y comprobar que ninguno queda inutilizado |
| Duración | Mantener una sesión de recepción y observar memoria, temperatura y ausencia de reinicios |

Los tres ejemplos también se han compilado a bytecode con MicroQuickJS 0.0.6 para comprobar su sintaxis.

La disponibilidad de los modos NRF24 se ha restablecido con acceso protegido; cada ensayo físico sigue pendiente hasta registrar su resultado. Para batería/energía se requiere el ensayo físico; no se infiere autonomía a partir de una compilación.
