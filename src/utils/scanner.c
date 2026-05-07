#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <vips/vips.h>
#include <libavformat/avformat.h>

#include "../../include/scanner.h"
#include "../../include/image.h"
#include "../../include/video.h"
#include "../../include/config.h"
#include "../../include/ui.h"

// Asegura que exista el directorio destino, crea si hace falta
static void asegurar_directorio(const char *ruta)
{
#ifdef _WIN32
    mkdir(ruta);
#else
    mkdir(ruta, 0777);
#endif
}

// Comprueba si la ruta es un directorio
static int es_directorio(const char *ruta)
{
    struct stat st;
    if (stat(ruta, &st) == 0)
        return S_ISDIR(st.st_mode);
    return 0;
}

// Devuelve 1 si libvips puede cargar la ruta como imagen
static int es_imagen_media(const char *ruta)
{
    return vips_foreign_find_load(ruta) ? 1 : 0;
}

// Devuelve 1 si ffmpeg puede abrir la ruta como un contenedor válido
static int es_video_media(const char *ruta)
{
    AVFormatContext *pCtx = NULL;
    int ok = 0;
    if (avformat_open_input(&pCtx, ruta, NULL, NULL) == 0)
    {
        avformat_close_input(&pCtx);
        ok = 1;
    }
    return ok;
}

// Procesa un único archivo (imagen o vídeo) y actualiza las estadísticas
static int procesar_archivo(const char *ruta_in, const char *nombre_archivo, const char *dir_out, Stats *stats)
{
    char nombre_base[MAX_NAME_LEN];
    strncpy(nombre_base, nombre_archivo, MAX_NAME_LEN);
    nombre_base[MAX_NAME_LEN - 1] = '\0';
    char *p = strrchr(nombre_base, '.');
    if (p)
        *p = '\0';

    char ruta_out[MAX_PATH_LEN];

    if (es_imagen_media(ruta_in))
    {
        for (int i = 0; i < 5; i++)
        {
            ui_loading_imagen(nombre_archivo);
            usleep(20000);
        }

        snprintf(ruta_out, sizeof(ruta_out), "%s/%s%s", dir_out, nombre_base, IMG_EXT_OUT);

        if (compactar_imagen_jpg(ruta_in, ruta_out))
        {
            stats->imagenes++;
            return 1;
        }
    }
    else if (es_video_media(ruta_in))
    {
        ui_barra_progreso("VIDEO", nombre_archivo, 0.0);

        snprintf(ruta_out, sizeof(ruta_out), "%s/%s%s", dir_out, nombre_base, VID_EXT_OUT);

        if (compactar_video(ruta_in, ruta_out))
        {
            stats->videos++;
            return 1;
        }
    }

    return 0;
}

// Recorre el directorio de entrada recursivamente y procesa archivos multimedia
int procesar_recursivo(const char *dir_in, const char *dir_out, Stats *stats)
{
    DIR *dir = opendir(dir_in);
    if (!dir)
        return 0;

    asegurar_directorio(dir_out);
    int archivos_en_rama = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        char ruta_in[MAX_PATH_LEN], ruta_out[MAX_PATH_LEN];
        snprintf(ruta_in, sizeof(ruta_in), "%s/%s", dir_in, ent->d_name);

        if (es_directorio(ruta_in))
        {
            snprintf(ruta_out, sizeof(ruta_out), "%s/%s", dir_out, ent->d_name);
            archivos_en_rama += procesar_recursivo(ruta_in, ruta_out, stats);
        }
        else
        {
            if (procesar_archivo(ruta_in, ent->d_name, dir_out, stats))
            {
                archivos_en_rama++;
            }
        }
    }
    closedir(dir);

    if (archivos_en_rama == 0)
    {
        rmdir(dir_out);
    }

    return archivos_en_rama;
}