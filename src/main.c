#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include <signal.h>
#include <stdlib.h>

// Librerías Windows soporte ANSI y UTF-8
#ifdef _WIN32
#include <windows.h>
#endif

#include "../include/scanner.h"
#include "../include/config.h"
#include "../include/ui.h"

// Manejo de cierre forzado (Ctrl+C)
void handle_sigint(int sig)
{
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    exit(0);
}

// Función auxiliar para limpiar la ruta del "Drag & Drop"
void limpiar_ruta_entrada(char *ruta)
{
    // Eliminar saltos de línea al final
    ruta[strcspn(ruta, "\n")] = 0;
    ruta[strcspn(ruta, "\r")] = 0;

    // Eliminar comillas iniciales y finales si existen (común al arrastrar carpetas)
    if (ruta[0] == '"')
    {
        memmove(ruta, ruta + 1, strlen(ruta));
        size_t l = strlen(ruta);
        if (l > 0 && ruta[l - 1] == '"')
        {
            ruta[l - 1] = 0;
        }
    }

    // Eliminar slash final para evitar rutas dobles
    size_t len = strlen(ruta);
    if (len > 0 && (ruta[len - 1] == '/' || ruta[len - 1] == '\\'))
        ruta[len - 1] = '\0';
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);

// --- CONFIGURACIÓN DEL ENFORNO (SOPORTE ANSI Y UTF-8) ---
#ifdef _WIN32
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
    SetConsoleOutputCP(CP_UTF8);
#endif

    ui_imprimir_header();

    // --- OBTENCIÓN DE LA RUTA (PARÁMETRO O DRAG & DROP) ---
    char carpeta_in[MAX_PATH_LEN];
    char carpeta_out[MAX_PATH_LEN + 20];

    if (argc >= 2)
    {
        strncpy(carpeta_in, argv[1], MAX_PATH_LEN - 1);
        limpiar_ruta_entrada(carpeta_in);
        printf("\n");
    }
    else
    {
        ui_mostrar_drop_zone();
        if (fgets(carpeta_in, MAX_PATH_LEN, stdin) == NULL)
            return 1;
        limpiar_ruta_entrada(carpeta_in);
        printf("\n");
    }

    // Validar que la ruta no esté vacía tras la limpieza
    if (strlen(carpeta_in) == 0)
    {
        ui_error_args();
        return 1;
    }

    // --- INICIALIZACIÓN DE LIBRERÍAS ---
    if (VIPS_INIT(argv[0]))
    {
        printf("\033[31mError: No se pudo inicializar libvips.\n\033[0m");
        return 1;
    }

    // Preparar carpeta de salida
    snprintf(carpeta_out, sizeof(carpeta_out), "%s%s", carpeta_in, SUFIJO_CARPETA);

    // --- EJECUCIÓN DEL NÚCLEO ---
    printf(HIDE_CURSOR); // Ocultar cursor antes de empezar el caos visual

    Stats stats = {0, 0};
    int total = procesar_recursivo(carpeta_in, carpeta_out, &stats);

    // --- CIERRE Y RESULTADOS ---
    ui_finalizar_estado();
    ui_imprimir_final(total, stats, carpeta_out);

    vips_shutdown();

    printf(RESET DIM "  Presiona ENTER para salir..." RESET);
    printf(SHOW_CURSOR);
    getchar();

    return 0;
}