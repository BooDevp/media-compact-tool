#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include <signal.h>

// Librerías Windows soporte ANSI y UTF-8
#ifdef _WIN32
#include <windows.h>
#endif

#include "../include/scanner.h"
#include "../include/config.h"
#include "../include/ui.h"

void handle_sigint(int sig)
{
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);
    // --- CONFIGURACIÓN DEL ENTORNO (SOPORTE "CSS" EN TERMINAL) ---
    #ifdef _WIN32
        // Activar el procesamiento de secuencias de terminal virtual (Colores ANSI)
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE)
        {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode))
            {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        // Forzar codificación UTF-8
        SetConsoleOutputCP(CP_UTF8);
    #endif

    if (argc < 2)
    {
        ui_error_args();
        return 1;
    }

    // --- INICIALIZACIÓN DE LIBRERÍAS ---
    if (VIPS_INIT(argv[0]))
    {
        printf(CLR_RED "Error: No se pudo inicializar libvips.\n" CLR_RESET);
        return 1;
    }

    // --- PREPARACIÓN DE RUTAS ---
    char carpeta_in[MAX_PATH_LEN];
    char carpeta_out[MAX_PATH_LEN + 20];

    strncpy(carpeta_in, argv[1], MAX_PATH_LEN - 1);
    carpeta_in[MAX_PATH_LEN - 1] = '\0';

    // Limpiar slash final para evitar rutas dobles (ej: carpeta//archivo)
    size_t len = strlen(carpeta_in);
    if (len > 0 && (carpeta_in[len - 1] == '/' || carpeta_in[len - 1] == '\\'))
        carpeta_in[len - 1] = '\0';

    snprintf(carpeta_out, sizeof(carpeta_out), "%s%s", carpeta_in, SUFIJO_CARPETA);

    // --- EJECUCIÓN DEL NÚCLEO ---
    ui_imprimir_header();

    Stats stats = {0, 0};
    int total = procesar_recursivo(carpeta_in, carpeta_out, &stats);

    // --- CIERRE Y RESULTADOS ---
    ui_finalizar_estado();
    ui_imprimir_final(total, stats, carpeta_out);

    vips_shutdown();
    printf(SHOW_CURSOR);

    return 0;
}