#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include "../include/scanner.h"
#include "../include/config.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: ./compactador <carpeta>\n");
        return 1;
    }

    if (VIPS_INIT(argv[0]))
        return 1;

    char carpeta_in[MAX_PATH_LEN], carpeta_out[MAX_PATH_LEN + 20];
    strncpy(carpeta_in, argv[1], MAX_PATH_LEN - 1);

    size_t len = strlen(carpeta_in);
    if (len > 0 && (carpeta_in[len - 1] == '/' || carpeta_in[len - 1] == '\\'))
        carpeta_in[len - 1] = '\0';

    snprintf(carpeta_out, sizeof(carpeta_out), "%s%s", carpeta_in, SUFIJO_CARPETA);

    Stats stats = {0, 0};
    printf("\n--- Iniciando Optimizacion ---\n");

    int total = procesar_recursivo(carpeta_in, carpeta_out, &stats);

    vips_shutdown();

    if (total > 0)
    {
        printf("\n--- FINALIZADO ---\n");
        printf("Procesados -> Imagenes: %d | Videos: %d\n", stats.imagenes, stats.videos);
        printf("Carpeta: %s\n", carpeta_out);
    }
    else
    {
        printf("\n[INFO] No habia archivos nuevos para optimizar.\n");
    }

    return 0;
}