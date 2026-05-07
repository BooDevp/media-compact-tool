#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include <signal.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../include/scanner.h"
#include "../include/config.h"
#include "../include/ui.h"

// --- CONFIGURACIÓN DE VENTANA ---
#ifdef _WIN32
void ajustar_ventana_limpia(int ancho, int alto)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    // Definir tamaños
    COORD coord = {(short)ancho, (short)alto};
    SMALL_RECT rect = {0, 0, (short)(ancho - 1), (short)(alto - 1)};
    SMALL_RECT minRect = {0, 0, 1, 1};
    SetConsoleWindowInfo(hOut, TRUE, &minRect);

    // Ajustar el buffer
    SetConsoleScreenBufferSize(hOut, coord);

    // Ajustar la ventana real
    SetConsoleWindowInfo(hOut, TRUE, &rect);
}
#endif

void handle_sigint(int sig)
{
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    exit(0);
}

void limpiar_ruta_entrada(char *ruta)
{
    ruta[strcspn(ruta, "\n")] = 0;
    ruta[strcspn(ruta, "\r")] = 0;
    if (ruta[0] == '"')
    {
        memmove(ruta, ruta + 1, strlen(ruta));
        size_t l = strlen(ruta);
        if (l > 0 && ruta[l - 1] == '"')
            ruta[l - 1] = 0;
    }
    size_t len = strlen(ruta);
    if (len > 0 && (ruta[len - 1] == '/' || ruta[len - 1] == '\\'))
        ruta[len - 1] = '\0';
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);

    #ifdef _WIN32
        ajustar_ventana_limpia(90, 30);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
        SetConsoleOutputCP(CP_UTF8);
        printf("\033[H\033[J");
    #endif

    ui_imprimir_header();

    char carpeta_in[MAX_PATH_LEN];
    char carpeta_out[MAX_PATH_LEN + 20];

    if (argc >= 2)
    {
        strncpy(carpeta_in, argv[1], MAX_PATH_LEN - 1);
        limpiar_ruta_entrada(carpeta_in);
    }
    else
    {
        ui_mostrar_drop_zone();
        if (fgets(carpeta_in, MAX_PATH_LEN, stdin) == NULL)
            return 1;
        limpiar_ruta_entrada(carpeta_in);
        printf("\n");
    }

    if (strlen(carpeta_in) == 0)
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

    printf(RESET DIM "\n  Presiona ENTER para salir..." RESET);
    printf(SHOW_CURSOR);
    fflush(stdin);
    getchar();

    return 0;
}