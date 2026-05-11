# Compactador

Compresor inteligente de imágenes y vídeos con calidad adaptativa basada en **SSIM** (Structural Similarity Index). Interfaz gráfica nativa de Windows con drag & drop, barras de progreso y vista de resultados.

## Flujo de trabajo

```
Arrastras carpeta o archivo →
             │
      ┌──────┴──────┐
      │             │
  ¿Directorio?   ¿Archivo?
      │             │
 Crear salida    Generar nombre
 _optimizado     _optimizado.ext
      │             │
      └──────┬──────┘
             │
      ┌──────┴──────┐
      │             │
  ¿Imagen?      ¿Vídeo?
      │             │
compresión      ffmpeg
adaptativa      CRF 24
(SSIM)          medium
      │
 ┌────┴────┐
 │         │
Sin alpha  Con alpha
(JPEG)     (PNG palette)
```

## Compresión adaptativa de imágenes

### Sin transparencia (JPEG)

Dos intentos de re-encode con control de calidad vía SSIM:

| Intento | Calidad | Umbral SSIM | Ahorro mínimo |
|---|---|---|---|---|
| 1 (Q75) | 75 | ≥ 0.970 | ≥ 20% |
| 2 (Q80) | 80 | ≥ 0.965 | ≥ 15% |
| 3 (Q85) | 85 | ≥ 0.960 | ≥ 10% |
| 4 (Q90) | 90 | ≥ 0.945 | > 0% |

Si el primer intento no cumple SSIM o ahorro, prueba con el segundo. Si ambos fallan, el archivo se omite (no se copia).

### Con transparencia (PNG palette)

- Convierte a PNG con paleta de 256 colores
- Umbral SSIM: ≥ 0.90
- Si no cumple SSIM o el tamaño no se reduce, el archivo se omite

### Metadatos

- Se eliminan EXIF, IPTC y XMP antes de comprimir
- Se aplica rotación automática (`vips_autorot`)
- Se añade la firma `_compactador_` en el campo EXIF Artist solo si el archivo se comprime con éxito
- Los archivos que ya tienen la firma se copian tal cual a la salida en modo directorio (safety), y se saltan en modo archivo individual (evita cadenas infinitas)

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
- Códec: HEVC (H.265) con CRF 24, preset medium
- **Detección de codec**: si la entrada ya es HEVC, hace stream copy directo sin re-encode
- Si la entrada es H.264, MPEG4 u otro, re-encodea a HEVC
- **Audio**: pass-through directo (no se re-encodea, se copia tal cual)
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

El Makefile busca automáticamente todos los `.c` en `src/` y sus subdirectorios. El ejecutable se genera como `compactador.exe` con el icono de `src/logo.ico` embebido (visible en Explorer, barra de tareas y Alt+Tab).

> **Nota**: El programa usa la API Win32 (`windows.h`, `gdi32`, `shell32`) para la interfaz gráfica. No requiere bibliotecas adicionales de UI.

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

## Uso (GUI)

1. Al iniciar, aparece una ventana con zona drag & drop y botón **OPTIMIZAR**
2. Arrastra una **carpeta** o **archivo** sobre la ventana:
   - Se muestra la ruta en la zona de drop y se habilita el botón **OPTIMIZAR**
3. Pulsa **OPTIMIZAR** para iniciar el procesamiento:
   - **Carpeta**: escanea recursivamente, crea `_optimizado` con los resultados
   - **Archivo**: procesa individualmente, genera `nombre_optimizado.ext`
4. Durante el procesamiento se muestra:
   - Barra de progreso global (archivos procesados / total)
   - Para vídeos: barra de progreso individual
   - Para imágenes: indicador animado sin barra
5. Al terminar, se muestra un cuadro resumen con estadísticas:
   - **ABRIR CARPETA**: abre el directorio de salida en el explorador
   - **VOLVER**: regresa a la vista inicial para procesar otro archivo
   - **SALIR**: cierra la aplicación

### Comportamiento de archivos con firma (`_compactador_`)

- **Modo directorio**: los archivos que ya tienen la firma se copian a la salida como safety (por si el usuario borra el origen). Si al final solo hay copias y ninguna compresión real, la carpeta `_optimizado` se elimina automáticamente.
- **Modo archivo individual**: los archivos con firma se saltan sin copiar (evita cadenas como `_optimizado_optimizado_optimizado...`).

### Archivos no soportados

Los archivos que no son imágenes ni vídeos reconocidos se ignoran silenciosamente (`.lnk`, `.gitconfig`, texto plano, etc.). La detección verifica el contenido real del archivo, no solo la extensión.

### Archivos soportados

- **Imágenes**: cualquier formato que libvips pueda leer y cargar (JPEG, PNG, WebP, TIFF, BMP, GIF, etc.)
- **Vídeos**: cualquier formato que ffmpeg pueda abrir y contenga al menos un stream de video (MP4, AVI, MKV, MOV, etc.)

## Logs (modo desarrollo)

Cuando `MODE_DEV = 1` (config.h), cada ejecución genera un archivo de log en `log/compactador_YYYYMMDD_HHMMSS.log` con detalle de cada archivo procesado:

```
[Hora] >>> PROCESANDO: entrada=ruta salida=ruta
[Hora]   tam_original=4429312 has_alpha=0
[Hora]   JPEG intento=1 calidad=75 umbral_ssim=0.9750
[Hora]   JPEG: SSIM=0.957151 ahorro=68.5% len=1393884 orig=4429312
[Hora]   JPEG intento1: Falla por SSIM insuficiente (0.957151 < 0.9750) | Ahorro OK (68.5%) -> pasando a intento 2
[Hora]   JPEG intento=2 calidad=90 umbral_ssim=0.9500
[Hora]   JPEG: SSIM=0.980123 ahorro=55.0% len=1993200 orig=4429312
[Hora]   JPEG intento2: COMPRIMIDO Q90 (SSIM OK 0.9801 >= 0.9500, ahorro 55.0% > 0%)
[Hora] SALTADO: ya contiene FIRMA_OPTIMIZADO (Artist=_compactador_...)
[Hora] >>> VIDEO: entrada=video.mp4 salida=video.mp4
[Hora]   VIDEO: tam_original=939KB
[Hora]   VIDEO: saltado, ya contiene firma
```

Para compilar sin logs (modo release): cambiar `MODE_DEV` a `0` en `config.h` o pasar `-DMODE_DEV=0` al compilador. En modo release las funciones de log se convierten en macros vacías (cero overhead).

## Configuración (`include/config.h`)

| Constante | Valor | Descripción |
|---|---|---|
| `FIRMA_OPTIMIZADO` | `_compactador_` | Marca EXIF en archivos comprimidos |
| `SUFIJO_CARPETA` | `_optimizado` | Sufijo para carpeta de salida |
| `SSIM_UMBRAL_1` | 0.970 | SSIM mínimo para intento Q75 |
| `SSIM_UMBRAL_2` | 0.965 | SSIM mínimo para intento Q80 |
| `SSIM_UMBRAL_3` | 0.960 | SSIM mínimo para intento Q85 |
| `SSIM_UMBRAL_4` | 0.945 | SSIM mínimo para intento Q90 |
| `AHORRO_MINIMO_1` | 20% | Ahorro mínimo requerido en intento Q75 |
| `AHORRO_MINIMO_2` | 15% | Ahorro mínimo requerido en intento Q80 |
| `AHORRO_MINIMO_3` | 10% | Ahorro mínimo requerido en intento Q85 |
| `AHORRO_MINIMO_4` | 0% | Ahorro mínimo requerido en intento Q90 (>0% real) |
| `IMG_QUALITY_1` | 75 | Calidad JPEG del primer intento |
| `IMG_QUALITY_2` | 80 | Calidad JPEG del segundo intento |
| `IMG_QUALITY_3` | 85 | Calidad JPEG del tercer intento |
| `IMG_QUALITY_4` | 90 | Calidad JPEG del cuarto intento |
| `PNG_MAX_COLORES` | 256 | Colores máximos en paleta PNG |
| `MODE_DEV` | 1 | 0 = sin logs, 1 = genera logs en log/ |
| `VID_CRF` | 24 | CRF para H.265 |
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
│   ├── app.h                  API de inicio de la GUI
│   ├── config.h               Constantes de configuración
│   ├── imagen.h               API de compresión de imágenes
│   ├── log.h                  API de logging
│   ├── progreso.h             API de callbacks de progreso
│   ├── scanner.h              API de escaneo de archivos
│   ├── str_util.h             Utilidades de cadenas
│   ├── types.h                Tipos comunes (Estadisticas)
│   └── video.h                API de compresión de vídeo
├── src/                        Código fuente
│   ├── main.c                 Punto de entrada
│   ├── logo.ico               Icono de la aplicación
│   ├── resources.rc            Recurso Windows (icono embebido)
│   ├── gui/
│   │   └── app.c              Ventana principal (Win32 API, GDI)
│   ├── compact/
│   │   ├── imagen.c           SSIM + compresión adaptativa JPEG/PNG
│   │   └── video.c            Compresión de vídeo con ffmpeg
│   └── utils/
│       ├── log.c              Implementación de logging
│       ├── progreso.c         Callbacks de progreso (puente hilo-GUI)
│       ├── scanner.c          Escáner recursivo + detección de medios
│       └── str_util.c         Utilidades de cadenas y rutas
├── libs/                       DLLs en tiempo de ejecución
│   ├── *.dll
│   └── vips-modules-8.18/
├── log/                        Archivos de log generados
│   └── compactador_*.log
├── vips-dev/                   Cabeceras y bibliotecas de libvips
└── ffmpeg-dev/                 Cabeceras y bibliotecas de ffmpeg
```


