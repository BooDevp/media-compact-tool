#ifndef CONFIG_H
#define CONFIG_H

// ── Marcador de archivos procesados ──
#define FIRMA_OPTIMIZADO "_compactador_"
#define SUFIJO_CARPETA   "_optimizado"

// ── Configuracion de imagenes ──
#define IMG_QUALITY  75

// ── Compresion adaptativa ──
#define SSIM_UMBRAL_1   0.975
#define SSIM_UMBRAL_2   0.95
#define AHORRO_MINIMO   20
#define IMG_QUALITY_1   75
#define IMG_QUALITY_2   90
#define PNG_MAX_COLORES 256

// ── Configuracion de video ──
#define VID_CRF     "24"
#define VID_PRESET  "medium"
#define VID_EXT_OUT ".mp4"

// ── Limites del sistema ──
#define MAX_PATH_LEN 2048
#define MAX_NAME_LEN 512

#endif