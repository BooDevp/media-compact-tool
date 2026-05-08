#include <stdio.h>
#include <string.h>
#include <vips/vips.h>
#include <signal.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

#include "../include/scanner.h"
#include "../include/config.h"
#include "../include/ui.h"

void handle_sigint(int sig) {
    printf(SHOW_CURSOR "\n\n  Abortado por el usuario.\n");
    exit(0);
}

void limpiar_ruta(char *r) {
    r[strcspn(r, "\n")] = 0; 
    r[strcspn(r, "\r")] = 0;
    if (r[0] == '"') {
        memmove(r, r + 1, strlen(r));
        size_t l = strlen(r);
        if (l > 0 && r[l-1] == '"') r[l-1] = 0;
    }
    size_t len = strlen(r);
    if (len > 0 && (r[len-1] == '/' || r[len-1] == '\\')) r[len-1] = '\0';
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(CP_UTF8);
#endif

    ui_imprimir_header();
    char c_in[MAX_PATH_LEN] = {0}, c_out[MAX_PATH_LEN + 20];

    if (argc >= 2) {
        strncpy(c_in, argv[1], MAX_PATH_LEN-1);
        limpiar_ruta(c_in);
    } else {
        ui_mostrar_drop_zone();
        if (fgets(c_in, MAX_PATH_LEN, stdin) == NULL) return 1;
        limpiar_ruta(c_in);
        printf("\n");
        printf("\n");
    }

    if (strlen(c_in) == 0) {
        ui_error_args();
        return 1;
    }
    
    printf("\033[H\033[J"); 
    ui_imprimir_header();     
    printf("\n  " TEXT_GRAY "Carpeta: " RESET TEXT_CYAN "%s" RESET "\n", c_in);

    if (VIPS_INIT(argv[0])) return 1;
    snprintf(c_out, sizeof(c_out), "%s%s", c_in, SUFIJO_CARPETA);

    printf(HIDE_CURSOR);

    int contador_temp = 0;
    int total_f = contar_media_recursiva(c_in, &contador_temp);    
    Stats st = {0, 0};
    int processed = 0;

    if (total_f > 0) {
        ui_barra_progreso_total(0.0, 0, total_f);
        procesar_recursivo_con_progreso(c_in, c_out, &st, total_f, &processed);        
    }

    ui_finalizar_estado();
    ui_imprimir_final(processed, st, c_out);
    vips_shutdown();

    printf(RESET DIM "\n  Presiona ENTER para salir..." RESET);
    printf(SHOW_CURSOR);

    while(_kbhit()) { _getch(); }
    getchar();

    return 0;
}