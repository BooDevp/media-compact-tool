#ifndef CONFIG_H
#define CONFIG_H

// ── Marcador de archivos procesados ──
#define FIRMA_OPTIMIZADO "_compactador_"
#define SUFIJO_CARPETA   "_optimizado"

// ── Compresion adaptativa (JPEG) ──
#define IMG_QUALITY_1   75
#define IMG_QUALITY_2   80
#define IMG_QUALITY_3   85
#define IMG_QUALITY_4   90

#define SSIM_UMBRAL_1   0.970
#define SSIM_UMBRAL_2   0.965
#define SSIM_UMBRAL_3   0.960
#define SSIM_UMBRAL_4   0.945

#define AHORRO_MINIMO_1 20.0
#define AHORRO_MINIMO_2 15.0
#define AHORRO_MINIMO_3 10.0
#define AHORRO_MINIMO_4  0.0

#define PNG_MAX_COLORES 256
#define SSIM_UMBRAL_PNG 0.90

// ── Configuracion de video ──
#define VID_CRF     "24"
#define VID_PRESET  "medium"
#define VID_EXT_OUT ".mp4"

// ── Modo desarrollo (1 = genera logs en log/) ──
#ifndef MODE_DEV
#define MODE_DEV 1
#endif

// ── Limites del sistema ──
#define MAX_PATH_LEN 2048
#define MAX_NAME_LEN 512

#endif