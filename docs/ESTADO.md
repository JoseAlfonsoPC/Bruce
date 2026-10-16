# Estado y alcance de la entrega

Fecha de referencia: **17 de octubre de 2026**. Base del repositorio: `a59213f3`. El objetivo de hardware acordado es **Cardputer ADV con LoRa 850–931 MHz, CC1101 y NRF24L01**, identificado mediante la fotografía y la guía de la expansión JosephCGS. Las prioridades acordadas son I²C, SPI y UART del intérprete.

## Implementado

- Perfil `m5stack-cardputer-adv-3in1` con las conexiones de la expansión y coordinación de teclado/NRF24 en GPIO8/9.
- NRF24 con acceso protegido en todos sus modos y API: Spectrum, MouseJack y NRF Jammer. Se restaura CE tras atender el teclado y Jammer cede ejecución para procesar teclas. Incluye Hardware check para diagnóstico en recepción.
- Information con estado SPI y pines efectivos; selector de transporte visible en MouseJack, validación de CE y conflictos; ventanas de recepción protegidas, contadores de capturas/dispositivos y navegación de vuelta corregida.
- Receptor pasivo MouseJack con dirección de captura `AA 00` de la referencia original, un solo pipe, FIFO vaciada al cambiar de canal y contador adicional de tramas ESB con CRC válido. El reconocimiento del Logitech del usuario sigue pendiente de comprobación tras esta corrección.
- Scan Info conserva contadores de capturas vacías/con datos y metadatos de la última captura con datos. Se añade tras repetirse `CRC: 1`, `Found: 0` con el enlace USB del Logitech confirmado; el diagnóstico permite investigar la captura sin ampliar las firmas por suposición.
- Monitor USB POSIX que evita operaciones explícitas sobre DTR/RTS y desactiva HUPCL. Sustituye la apertura del monitor que producía reinicios USB en esta unidad durante la investigación.
- Selección inicial SX1262 y banda LoRa limitada al rango del módulo en este perfil.
- Lectura binaria I²C y escritura/lectura combinada; validación de argumentos, manejo de errores y liberación de recursos.
- Interfaces SPI y UART con límites de datos y tiempos de espera, más ejemplos para diagnóstico.
- Generación local de WebUI, sin dependencia de minificadores externos.
- Generador MicroQuickJS nativo para el ordenador, con salida de tablas de 32 bits.
- Preparación de biblioteca WiFi que se detiene ante errores y recupera el archivo original si una ejecución anterior lo dejó ausente.
- Empaquetado basado en segmentos de PlatformIO, validación de la partición real y suma SHA-256.
- Dependencias de construcción documentadas, FastLED fijado a 3.10.3, configuración Docker y pruebas automatizadas.
- Guías locales de instalación, operación, arquitectura, hardware, scripting, desarrollo y aceptación.

## Evidencia de validación

- Python del entorno de construcción: 3.11; PlatformIO: 6.2.0; pioarduino: 55.03.39.
- Suite del ordenador: 24 pruebas, incluyendo 57 comprobaciones nativas de buses generales, 59 del perfil 3 en 1, 32 del acceso/multiplexor NRF24, 16 de transportes/pines NRF24 y 32.140 del decodificador ESB pasivo y sus estadísticas; AddressSanitizer y UndefinedBehaviorSanitizer activos. El monitor USB se prueba con pseudoterminales POSIX para lectura binaria, HUPCL, cierre, límites y filtrado de consola.
- Tres ejemplos de hardware compilados a bytecode con MicroQuickJS 0.0.6; esta comprobación valida su sintaxis, sin ejecutar transferencias reales.
- Perfil general Cardputer: compilación y empaquetado verificados; aplicación de 4.114.960 bytes en una partición de 5.111.808 bytes (80,5 %) en la compilación final.
- Perfil ADV 3 en 1: compilación y empaquetado verificados; aplicación de 4.117.648 bytes en una partición de 5.111.808 bytes (80,6 %). Las dos imágenes combinadas coinciden byte a byte con sus imágenes factory de PlatformIO y sus sumas SHA-256 son correctas.
- La prueba del multiplexor ejecuta 1.000 operaciones de radio frente a 200 eventos de teclado en hilos concurrentes. Comprueba exclusión, recursividad, restauración de CE y cierre.
- Los enlaces documentales y la sintaxis Python se comprueban con las herramientas del repositorio.
- Instalación USB en la unidad del usuario: copia previa de 8 MB, actualización de la partición existente `brucea`, verificación de la aplicación y conservación de Launcher, tabla de particiones y NVS durante la grabación. Se seleccionó Bruce como aplicación de arranque. La revisión con Scan Info está instalada: respondió a `info`, inicializó el NRF24 y confirmó comunicación SPI sin panic detectado durante la comprobación.
- Con el monitor USB corregido, un barrido pasivo de 120 segundos terminó sin reinicios y se detuvo con Esc remoto: 7.229 capturas, 5 que superan CRC, las 5 con payload vacío, 0 dispositivos identificados. El uptime pasó de `00:03:41` a `00:05:50`. Esta comprobación valida ese ensayo y su cierre; el reconocimiento del Logitech sigue pendiente.
- Tras la confirmación de actividad del Logitech se recuperó un segundo resumen: 10.668 capturas, 7 que superan CRC, las 7 vacías, 0 con datos y 0 dispositivos identificados. Su duración no quedó delimitada. Se comprobó que Scan Info conservaba esos contadores al volver a abrirla; el uptime tras detener el barrido era `00:11:30`. La ausencia de datos no demuestra incompatibilidad ni identifica la causa RF.

Los tamaños y las sumas de los binarios finales están en [el registro de validación](validacion.json).

## Pendientes y límites

La instalación, la verificación de memoria, el arranque por USB, la inicialización/respuesta SPI del NRF24 y el barrido controlado de 120 segundos se han realizado en esta sesión. Las pruebas funcionales restantes de teclado físico, SD, audio y radios siguen pendientes. Sigue la [matriz de aceptación](PRUEBAS.md), incluido Hardware check y la entrada/salida repetida de Spectrum NRF24. La integración del módulo es un perfil explícito; no se aplica al perfil general por deducción automática.

La documentación y la compilación no convierten toda la hoja de ruta histórica en trabajo terminado. Permanecen pendientes:

| Elemento de la hoja de ruta | Situación |
| --- | --- |
| Unificación de InputHandler de todas las placas | Mantiene adaptaciones por dispositivo; no se ha refactorizado toda la matriz |
| PPM / FuelGauge comunes | Pendiente de diseño y validación por hardware |
| BLE Session hijacking | No implementado en esta entrega; requiere concretar protocolo y alcance |
| RF Rolljam | No implementado en esta entrega; requiere especificación y banco de pruebas |
| Trasladar FM a JavaScript | Pendiente; permanece el módulo existente |
| Modos NRF24 en el dispositivo del usuario | Coordinación implementada y probada con simulación; validación eléctrica y RF pendiente |
| Todas las demás placas del repositorio | No se afirma validación completa por compilar Cardputer |

La sintaxis de los flujos de CI y Docker se ha comprobado; no se han ejecutado los flujos remotos ni una construcción dentro de Docker en esta sesión.

Las dependencias de firmware que usan ramas o intervalos siguen sujetas a cambios del proveedor. La cadena genera cabeceras compartidas: construye entornos en serie. Las bibliotecas externas pueden emitir advertencias; revisa los registros de cada compilación al actualizar dependencias.
