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

static void asegurar_directorio(const char *ruta)
{
#ifdef _WIN32
    mkdir(ruta);
#else
    mkdir(ruta, 0777);
#endif
}

static int es_directorio(const char *ruta)
{
    struct stat st;
    if (stat(ruta, &st) == 0)
        return S_ISDIR(st.st_mode);
    return 0;
}

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
            char nombre_base[MAX_NAME_LEN];
            strncpy(nombre_base, ent->d_name, MAX_NAME_LEN);
            char *p = strrchr(nombre_base, '.');
            if (p)
                *p = '\0';

            // --- PROCESADO DE IMAGEN ---
            if (vips_foreign_find_load(ruta_in))
            {
                for (int i = 0; i < 5; i++)
                {
                    ui_loading_imagen(ent->d_name);
                    usleep(20000);
                }

                snprintf(ruta_out, sizeof(ruta_out), "%s/%s%s", dir_out, nombre_base, IMG_EXT_OUT);

                if (compactar_imagen_jpg(ruta_in, ruta_out))
                {
                    stats->imagenes++;
                    archivos_en_rama++;
                }
            }
            // --- PROCESADO DE VIDEO ---
            else
            {
                AVFormatContext *pCtx = NULL;
                if (avformat_open_input(&pCtx, ruta_in, NULL, NULL) == 0)
                {
                    avformat_close_input(&pCtx);

                    // Iniciamos la barra al 0%
                    ui_barra_progreso("VIDEO", ent->d_name, 0.0);

                    snprintf(ruta_out, sizeof(ruta_out), "%s/%s%s", dir_out, nombre_base, VID_EXT_OUT);

                    if (compactar_video(ruta_in, ruta_out))
                    {
                        stats->videos++;
                        archivos_en_rama++;
                    }
                }
            }
        }
    }
    closedir(dir);

    if (archivos_en_rama == 0){
        rmdir(dir_out);
    }

    return archivos_en_rama;
}