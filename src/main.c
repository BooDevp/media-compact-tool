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

// Limpia y normaliza la ruta de entrada
static void handle_sigint(int sig)
{
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    exit(0);
}

static void limpiar_ruta_entrada(char *ruta)
{
    ruta[strcspn(ruta, "\n")] = 0;
    ruta[strcspn(ruta, "\r")] = 0;

    if (ruta[0] == '"')
    {
        memmove(ruta, ruta + 1, strlen(ruta));
        size_t l = strlen(ruta);
        if (l > 0 && ruta[l - 1] == '"')
        {
            ruta[l - 1] = 0;
        }
    }

    size_t len = strlen(ruta);
    if (len > 0 && (ruta[len - 1] == '/' || ruta[len - 1] == '\\'))
        ruta[len - 1] = '\0';
}

static void configurar_consola_windows(void)
{
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
}

static int obtener_carpeta_entrada(int argc, char *argv[], char *carpeta_in, size_t size)
{
    if (argc >= 2)
    {
        strncpy(carpeta_in, argv[1], size - 1);
        carpeta_in[size - 1] = '\0';
        limpiar_ruta_entrada(carpeta_in);
        printf("\n");
        return 1;
    }

    ui_mostrar_drop_zone();
    if (fgets(carpeta_in, (int)size, stdin) == NULL)
        return 0;
    limpiar_ruta_entrada(carpeta_in);
    printf("\n");
    return (strlen(carpeta_in) > 0);
}

// Punto de entrada: inicializa y ejecuta el procesamiento principal
int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);

    configurar_consola_windows();

    ui_imprimir_header();

    char carpeta_in[MAX_PATH_LEN];
    char carpeta_out[MAX_PATH_LEN + 20];

    if (!obtener_carpeta_entrada(argc, argv, carpeta_in, sizeof(carpeta_in)))
    {
        ui_error_args();
        return 1;
    }

    if (VIPS_INIT(argv[0]))
    {
        printf("\033[31mError: No se pudo inicializar libvips.\n\033[0m");
        return 1;
    }

    snprintf(carpeta_out, sizeof(carpeta_out), "%s%s", carpeta_in, SUFIJO_CARPETA);

    printf(HIDE_CURSOR);

    Stats stats = {0, 0};
    int total = procesar_recursivo(carpeta_in, carpeta_out, &stats);

    ui_finalizar_estado();
    ui_imprimir_final(total, stats, carpeta_out);

    vips_shutdown();

    printf(RESET DIM "  Presiona ENTER para salir..." RESET);
    printf(SHOW_CURSOR);
    getchar();

    return 0;
}