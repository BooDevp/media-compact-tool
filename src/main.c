#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include <signal.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

#include <scanner.h>
#include <config.h>
#include <ui.h>
#include <str_util.h>
#include <sistema.h>
#include <log.h>

static void manejar_sigint(int sig)
{
    log_close();
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    vips_shutdown();
    exit(0);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    CreateMutex(NULL, TRUE, "Global\\MediaCompactor_Unique_Mutex_ID");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 0;
    }
    configurar_consola();
#endif

    signal(SIGINT, manejar_sigint);

    if (VIPS_INIT(argv[0]))
        return 1;

    log_init();

    while (1)
    {
        printf("\033[H\033[J");
        ui_imprimir_header();

        char entrada[MAX_PATH_LEN] = {0};
        char salida[MAX_PATH_LEN + 20];

        ui_mostrar_drop_zone();

        if (fgets(entrada, MAX_PATH_LEN, stdin) == NULL)
            break;
        limpiar_ruta(entrada);

        if (strlen(entrada) == 0)
            continue;

        printf("\033[H\033[J");
        ui_imprimir_header();
        printf(HIDE_CURSOR);

        Estadisticas st = {0, 0};
        int procesados = 0;

        if (es_directorio(entrada))
        {
            printf("\n\n  " TEXT_GRAY " Carpeta: " RESET TEXT_CYAN "%s" RESET "\n", entrada);

            snprintf(salida, sizeof(salida), "%s%s", entrada, SUFIJO_CARPETA);

            int contador_temp = 0;
            int total = contar_media_recursiva(entrada, &contador_temp);

            if (total > 0)
            {
                ui_barra_progreso_total(0.0, 0, total);
                procesar_recursivo_con_progreso(entrada, salida, &st, total, &procesados);
            }
            else
            {
                printf("\n  " TEXT_GRAY "No se encontraron archivos multimedia compatibles." RESET "\n");
            }

            ui_finalizar_estado();
            ui_imprimir_final(procesados, st, salida, 1);
        }
        else
        {
            printf("\n\n  " TEXT_GRAY " Archivo: " RESET TEXT_CYAN "%s" RESET "\n", entrada);

            generar_ruta_salida_archivo(entrada, salida, sizeof(salida));
            procesados = procesar_archivo_unico(entrada, salida, &st);

            ui_finalizar_estado();
            ui_imprimir_final(procesados, st, salida, 0);
        }

        printf(RESET "\n\n " RESET " Presiona " TEXT_CYAN "ENTER" RESET " para continuar ...");
        printf(SHOW_CURSOR);

        while (_kbhit())
        {
            _getch();
        }
        getchar();
    }

    log_close();
    vips_shutdown();
    return 0;
}
