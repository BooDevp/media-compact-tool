#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <vips/vips.h>
#include <libavformat/avformat.h>

#include <scanner.h>
#include <imagen.h>
#include <video.h>
#include <config.h>
#include <ui.h>

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

static int es_imagen_media(const char *ruta)
{
    return vips_foreign_find_load(ruta) ? 1 : 0;
}

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

static int procesar_archivo(const char *ruta_in, const char *nombre_archivo, const char *dir_out, Estadisticas *stats)
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

static int procesar_recursivo_interno(const char *dir_in, const char *dir_out, Estadisticas *stats, int total_files, int *processed_count)
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
            archivos_en_rama += procesar_recursivo_interno(ruta_in, ruta_out, stats, total_files, processed_count);
        }
        else
        {
            if (procesar_archivo(ruta_in, ent->d_name, dir_out, stats))
            {
                archivos_en_rama++;
            }
            if (processed_count && total_files > 0)
            {
                (*processed_count)++;
                double pct = ((double)(*processed_count)) / (double)total_files;
                ui_barra_progreso_total(pct, *processed_count, total_files);
            }
        }
    }
    closedir(dir);

    if (archivos_en_rama == 0)
        rmdir(dir_out);

    return archivos_en_rama;
}

int procesar_recursivo(const char *dir_in, const char *dir_out, Estadisticas *stats)
{
    return procesar_recursivo_interno(dir_in, dir_out, stats, 0, NULL);
}

int procesar_recursivo_con_progreso(const char *dir_in, const char *dir_out, Estadisticas *stats, int total_files, int *processed_count)
{
    return procesar_recursivo_interno(dir_in, dir_out, stats, total_files, processed_count);
}

int contar_media_recursiva(const char *dir_in, int *acumulado)
{
    DIR *dir = opendir(dir_in);
    if (!dir) return 0;

    int total_en_esta_carpeta = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.') continue;

        char ruta_in[MAX_PATH_LEN];
        snprintf(ruta_in, sizeof(ruta_in), "%s/%s", dir_in, ent->d_name);

        if (es_directorio(ruta_in))
        {
            total_en_esta_carpeta += contar_media_recursiva(ruta_in, acumulado);
        }
        else
        {
            if (es_imagen_media(ruta_in) || es_video_media(ruta_in))
            {
                total_en_esta_carpeta++;
                (*acumulado)++;
                ui_animar_analisis(*acumulado);
            }
        }
    }

    closedir(dir);
    return total_en_esta_carpeta;
}
