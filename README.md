# Compactador

Compresor inteligente de imágenes y vídeos con calidad adaptativa basada en **SSIM** (Structural Similarity Index).

## Flujo de trabajo

```
Arrastras carpeta o archivo →
  ┌─ Es directorio? → escanea recursivamente → procesa cada archivo
  └─ Es archivo?    → procesa individualmente
                            │
                     ┌──────┴──────┐
                     │             │
                 ¿Imagen?      ¿Vídeo?
                     │             │
              compresión       ffmpeg
              adaptativa       CRF 24
              (SSIM)           medium
                     │
              ┌──────┴──────┐
              │             │
          Sin alpha    Con alpha
          (JPEG)        (PNG palette)
```

## Compresión adaptativa de imágenes

### Sin transparencia (JPEG)

Dos intentos de re-encode con control de calidad vía SSIM:

| Intento | Calidad | Umbral SSIM | Ahorro mínimo |
|---|---|---|---|
| 1 (Q75) | 75 | ≥ 0.975 | ≥ 20% |
| 2 (Q90) | 90 | ≥ 0.95 | > 0% |

Si el primer intento no cumple SSIM o ahorro, prueba con el segundo. Si ambos fallan, copia el original sin modificar.

### Con transparencia (PNG palette)

- Convierte a PNG con paleta de 256 colores
- Umbral SSIM: ≥ 0.90
- Si no cumple SSIM o el tamaño no se reduce, copia el original

### Metadatos

- Se eliminan EXIF, IPTC y XMP antes de comprimir
- Se aplica rotación automática (`vips_autorot`)
- Se añade la firma `_compactador_` en el campo EXIF Artist solo si el archivo se comprime con éxito
- Las copias directas (fallback) **no** llevan firma, permitiendo reintentar en otra ejecución

### Cálculo de SSIM

Implementación desde cero con libvips (sin dependencia externa):

- Extrae el primer canal (luminancia equivalente)
- Convierte a flotante
- Aplica filtro Gaussiano (σ = 1.5) para medias locales
- Calcula varianzas, covarianza y el mapa SSIM
- Promedia el mapa para obtener el valor global
- Constantes estándar: C1 = (0.01 × 255)², C2 = (0.03 × 255)²

## Compresión de vídeo

- Usa **ffmpeg** via libavformat/libavcodec
- Códec: H.264
- CRF: 24
- Preset: medium
- Salida: `.mp4`

## Requisitos de compilación

- **w64devkit** (MinGW-w64 para Windows)
- **libvips 8.18** (cabeceras + DLLs en `vips-dev/`)
- **ffmpeg** (cabeceras + DLLs en `ffmpeg-dev/`)
- **make** (incluido en w64devkit)

## Compilación

```bash
cd compactador
make
```

El Makefile busca automáticamente todos los `.c` en `src/` y sus subdirectorios. El ejecutable se genera como `compactador.exe`.

Para limpiar:

```bash
make clean
```

## Ejecución

### Lanzador (recomendado)

El programa necesita las DLLs de libvips y ffmpeg en el PATH. El lanzador `compactador.bat` configura las rutas automáticamente:

```batch
@echo off
set "PATH=libs;%PATH%"
compactador.exe
```

### Manual

```bash
set PATH=libs;%PATH%
compactador.exe
```

### Mutex de instancia única

En Windows, el programa crea un mutex global para evitar múltiples instancias simultáneas.

## Uso

1. Al iniciar, aparece una interfaz en terminal con zona de "drop"
2. Arrastra o escribe la ruta de una **carpeta** o **archivo** y presiona Enter
   - **Carpeta**: procesa recursivamente todo el contenido, crea una carpeta `_optimizado`
   - **Archivo**: procesa el archivo individual, genera `nombre_optimizado.ext`
3. Al terminar, muestra resumen con archivos procesados y abre el explorador si es una carpeta

### Archivos soportados

- **Imágenes**: cualquier formato que libvips pueda leer (JPEG, PNG, WebP, TIFF, BMP, GIF, etc.)
- **Vídeos**: cualquier formato que ffmpeg pueda leer (MP4, AVI, MKV, MOV, etc.)

## Logs

Cada ejecución genera un archivo de log en `log/compactador_YYYYMMDD_HHMMSS.log` con detalle de cada imagen procesada:

```
[Hora] >>> PROCESANDO: entrada=ruta salida=ruta
[Hora]   tam_original=4429312 has_alpha=0
[Hora]   JPEG intento=1 calidad=75 umbral_ssim=0.9750
[Hora]   JPEG: SSIM=0.957151 ahorro=68.5% len=1393884 orig=4429312
[Hora]   JPEG intento1: Falla por SSIM insuficiente (0.957151 < 0.9750) | Ahorro OK (68.5%) -> pasando a intento 2
[Hora]   JPEG intento=2 calidad=90 umbral_ssim=0.9500
[Hora]   JPEG: SSIM=0.980123 ahorro=55.0% len=1993200 orig=4429312
[Hora]   JPEG intento2: COMPRIMIDO Q90 (SSIM OK 0.9801 >= 0.9500, ahorro 55.0% > 0%)
```

## Configuración (`include/config.h`)

| Constante | Valor | Descripción |
|---|---|---|
| `FIRMA_OPTIMIZADO` | `_compactador_` | Marca EXIF en archivos comprimidos |
| `SUFIJO_CARPETA` | `_optimizado` | Sufijo para carpeta de salida |
| `SSIM_UMBRAL_1` | 0.975 | SSIM mínimo para intento Q75 |
| `SSIM_UMBRAL_2` | 0.95 | SSIM mínimo para intento Q90 |
| `SSIM_UMBRAL_PNG` | 0.90 | SSIM mínimo para PNG con transparencia |
| `AHORRO_MINIMO` | 20% | Ahorro mínimo requerido en intento Q75 |
| `IMG_QUALITY_1` | 75 | Calidad JPEG del primer intento |
| `IMG_QUALITY_2` | 90 | Calidad JPEG del segundo intento |
| `PNG_MAX_COLORES` | 256 | Colores máximos en paleta PNG |
| `VID_CRF` | 24 | CRF para H.264 |
| `VID_PRESET` | medium | Preset de x264 |
| `VID_EXT_OUT` | .mp4 | Extensión de salida de vídeo |
| `MAX_PATH_LEN` | 2048 | Tamaño máximo de ruta |
| `MAX_NAME_LEN` | 512 | Tamaño máximo de nombre de archivo |

## Estructura del proyecto

```
compactador/
├── compactador.exe            Ejecutable compilado
├── Makefile                    Sistema de compilación
├── README.md                   Este archivo
├── include/                    Cabeceras
│   ├── config.h               Constantes de configuración
│   ├── imagen.h               API de compresión de imágenes
│   ├── log.h                  API de logging
│   ├── scanner.h              API de escaneo de archivos
│   ├── sistema.h              API de utilidades del sistema
│   ├── str_util.h             API de utilidades de cadenas
│   ├── types.h                Tipos comunes (Estadisticas)
│   └── ui.h                   API de interfaz de terminal
├── src/                        Código fuente
│   ├── main.c                 Punto de entrada y bucle principal
│   ├── compact/
│   │   ├── imagen.c           SSIM + compresión adaptativa JPEG/PNG
│   │   └── video.c            Compresión de vídeo con ffmpeg
│   └── utils/
│       ├── log.c              Implementación de logging
│       ├── scanner.c          Escáner recursivo de archivos
│       ├── sistema.c          Configuración de consola Windows
│       ├── str_util.c         Utilidades de cadenas y rutas
│       └── ui.c               Interfaz de terminal (barras, colores)
├── libs/                       DLLs en tiempo de ejecución
│   ├── *.dll
│   └── vips-modules-8.18/
├── log/                        Archivos de log generados
│   └── compactador_*.log
├── vips-dev/                   Cabeceras y bibliotecas de libvips
└── ffmpeg-dev/                 Cabeceras y bibliotecas de ffmpeg
```

## Licencia

Uso interno.
