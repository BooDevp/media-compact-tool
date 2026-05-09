#include <stdio.h>
#include <stdlib.h>

#include <ui.h>
#include <config.h>

#define ROW_TOTAL 7
#define ROW_FILE 9
#define ROW_SUMMARY 12

void ui_imprimir_header(void)
{
    printf("\033[1;1H");
    printf("\n  " BG_BLUE FG_WHITE BOLD " MEDIA COMPACTOR v1.0 " RESET);
    printf(TEXT_GRAY " | HEVC & libvips\n" RESET);
    printf(DIM "  ───────────────────────────────────────────────────────────────\n" RESET);
}

void ui_mostrar_drop_zone(void)
{
    printf("\n" TEXT_GRAY "  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │                                                             │\n");
    printf("  │    " RESET BOLD "ARRASTRA UNA CARPETA AQUÍ Y PULSA ENTER PARA EMPEZAR" RESET TEXT_GRAY "     │\n");
    printf("  │                                                             │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n" RESET);
    printf(SHOW_CURSOR "\n  > " TEXT_CYAN);
}

void ui_animar_analisis(int actual)
{
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    printf("\033[%d;1H\033[K", ROW_TOTAL);
    printf("  " TEXT_GRAY "%s Analizando archivos... " RESET "(%d encontrados)", spinner[frame], actual);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

void ui_barra_progreso_total(double porcentaje, int processed, int total)
{
    int ancho_barra = 30;
    int rellenos = (int)(ancho_barra * porcentaje);

    printf("\033[%d;1H\033[K", ROW_TOTAL);
    printf("  "  "Progreso Total " RESET "[");
    for (int i = 0; i < ancho_barra; i++)
    {
        if (i < rellenos)
            printf(TEXT_CYAN "#" RESET);
        else
            printf(DIM "-" RESET);
    }
    printf("] " BOLD "%3.0f%%" RESET " (%d/%d)", porcentaje * 100, processed, total);
    fflush(stdout);
}

void ui_loading_imagen(const char *archivo)
{
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("  " TEXT_CYAN ">> " RESET "IMAGEN " TEXT_BLUE "[%s] " RESET TEXT_GRAY "Procesando: %-35.35s" RESET, spinner[frame], archivo);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje)
{
    int ancho_barra = 15;
    int rellenos = (int)(ancho_barra * porcentaje);
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("  " TEXT_CYAN ">> " RESET "%-7s " TEXT_BLUE "[", tipo);
    for (int i = 0; i < ancho_barra; i++)
    {
        printf(i < rellenos ? "█" : "░");
    }
    printf("] " RESET TEXT_GREEN "%3.0f%% " RESET TEXT_GRAY "%-25.25s" RESET, porcentaje * 100, archivo);
    fflush(stdout);
}

void ui_finalizar_estado(void)
{
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("\033[%d;1H  " TEXT_GREEN ">> PROCESO COMPLETADO" RESET "\n", ROW_SUMMARY - 2);
}

void ui_imprimir_final(int total, Estadisticas stats, const char *carpeta_out)
{
    printf("\033[%d;1H", ROW_SUMMARY);
    printf(DIM "  ───────────────────────────────────────────────────────────────\n" RESET);
    printf("\n");
    printf(BOLD "  RESUMEN DE OPERACIÓN\n\n" RESET);
    printf(TEXT_GRAY "  ┌───────────────────────────────────────────────────┐\n" RESET);
    printf(TEXT_GRAY "  │  " RESET "Imágenes optimizadas:   " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.imagenes);
    printf(TEXT_GRAY "  │  " RESET "Vídeos   optimizados:   " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.videos);
    printf(TEXT_GRAY "  │  " RESET "Total:                  " BOLD TEXT_CYAN "%-25d" RESET TEXT_GRAY "│\n" RESET, total);
    printf(TEXT_GRAY "  └───────────────────────────────────────────────────┘\n" RESET);
    if (stats.imagenes > 0 || stats.videos > 0)
    {
        printf("\n  " BOLD "Destino: " RESET TEXT_GRAY "%s\n" RESET, carpeta_out);
#ifdef _WIN32
        char comando[MAX_PATH_LEN + 20];
        snprintf(comando, sizeof(comando), "explorer \"%s\"", carpeta_out);
        system(comando);
#endif
    }
    else
    {
        printf("\n  " BOLD "No se procesaron archivos multimedia." RESET "\n");
    }
}