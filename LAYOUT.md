# Estructura del repositorio

```text
.
├── boards/                 # Manifiestos, pines, perfiles y adaptación de placas
├── docs/                   # Guías en español y estado de validación
├── embedded_resources/     # Fuentes de la interfaz web
├── include/                # Cabeceras compartidas y recursos generados
├── lib/                    # Bibliotecas incluidas y tablas del intérprete
├── src/
│   ├── core/               # Menús, buses, configuración y servicios comunes
│   ├── modules/            # Funciones del firmware e intérprete
│   └── main.cpp            # Arranque y bucle principal
├── sd_files/               # Ejemplos y recursos opcionales de almacenamiento
├── tests/                  # Pruebas de ordenador con simulaciones de hardware
├── tools/                  # Generación y validación de artefactos
├── docker/                 # Entorno de construcción
├── .github/workflows/      # Comprobaciones y compilaciones de CI
├── platformio.ini          # Configuración común y dependencias de firmware
└── requirements-build.txt  # Herramientas Python del ordenador
```

Consulta la [arquitectura](docs/ARQUITECTURA.md) y el [índice documental](docs/README.md).
