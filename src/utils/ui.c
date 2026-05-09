#include <stdio.h>
#include <stdlib.h>

#include <ui.h>
#include <config.h>

#define ROW_PROGRESO_LABEL  8
#define ROW_PROGRESO_BAR    10
#define ROW_ARCHIVO_LABEL   12
#define ROW_ARCHIVO_LINE    14
#define ROW_RESULTADOS      16

void ui_imprimir_header(void)
{
    printf("\033[1;1H");
    printf("\n");
    printf("  " CLR_BORDER "╭──────────────────────────────────────────────────╮" CLR_RESET "\n");
    printf("  " CLR_BORDER "│" CLR_RESET "  " CLR_BOLD CLR_HL_FG "MEDIA COMPACTOR v1.0      " CLR_RESET "      " CLR_MUTED "HEVC & libvips" CLR_RESET "  " CLR_BORDER "│" CLR_RESET "\n");
    printf("  " CLR_BORDER "╰──────────────────────────────────────────────────╯" CLR_RESET);
}

void ui_mostrar_drop_zone(void)
{
    printf("\n");
    printf("  " CLR_BORDER "╭──────────────────────────────────────────────────╮" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│" CLR_RESET "  " CLR_BOLD " ARRASTRA UNA CARPETA O ARCHIVO AQUI (ENTER)  " CLR_RESET "  " CLR_BORDER "│" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "│                                                  │" CLR_RESET "\n");
    printf("  " CLR_BORDER "╰──────────────────────────────────────────────────╯" CLR_RESET "\n");
    printf(SHOW_CURSOR "\n  " CLR_ACCENT "> " CLR_RESET);
}

void ui_animar_analisis(int actual)
{
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    printf("\033[%d;1H\033[K", ROW_PROGRESO_LABEL);
    printf("  " CLR_DIM CLR_BORDER "──" CLR_RESET " " CLR_MUTED "Analizando" CLR_RESET " " CLR_DIM CLR_BORDER "────────────────────────────────────────────" CLR_RESET);
    printf("\033[%d;1H\033[K", ROW_PROGRESO_BAR);
    printf("   " CLR_MUTED "%s Analizando archivos... " CLR_RESET "(" CLR_ACCENT "%d" CLR_RESET " encontrados)", spinner[frame], actual);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

void ui_barra_progreso_total(double porcentaje, int processed, int total)
{
    int ancho_barra = 30;
    int rellenos = (int)(ancho_barra * porcentaje);

    printf("\033[%d;1H\033[K", ROW_PROGRESO_LABEL);
    printf("  " CLR_DIM CLR_BORDER "──" CLR_RESET " " CLR_MUTED "Progreso" CLR_RESET " " CLR_DIM CLR_BORDER "────────────────────────────────────────────────" CLR_RESET);
    printf("\033[%d;1H\033[K", ROW_PROGRESO_BAR);
    printf("  " CLR_ACCENT "[");
    for (int i = 0; i < ancho_barra; i++)
        printf(i < rellenos ? "█" : CLR_MUTED "░" CLR_ACCENT);
    printf("]" CLR_RESET " " CLR_BOLD "%3.0f%%" CLR_RESET " (" CLR_INFO "%d/%d" CLR_RESET ")", porcentaje * 100, processed, total);
    fflush(stdout);
}

void ui_loading_imagen(const char *archivo)
{
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LABEL);
    printf("  " CLR_DIM CLR_BORDER "──" CLR_RESET " " CLR_MUTED "Archivo" CLR_RESET " " CLR_DIM CLR_BORDER "────────────────────────────────────────────────" CLR_RESET);
    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LINE);
    printf("   " CLR_MUTED "%s" CLR_RESET " " CLR_INFO "IMAGEN" CLR_RESET "  " CLR_MUTED "Procesando:" CLR_RESET " " CLR_ACCENT "%-35.35s" CLR_RESET, spinner[frame], archivo);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje)
{
    int ancho_barra = 15;
    int rellenos = (int)(ancho_barra * porcentaje);

    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LABEL);
    printf("  " CLR_DIM CLR_BORDER "──" CLR_RESET " " CLR_MUTED "Archivo" CLR_RESET " " CLR_DIM CLR_BORDER "────────────────────────────────────────────────" CLR_RESET);
    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LINE);
    printf("   " CLR_ACCENT ">" CLR_RESET " " CLR_BOLD "%-7s" CLR_RESET " " CLR_ACCENT "[", tipo);
    for (int i = 0; i < ancho_barra; i++)
        printf(i < rellenos ? "█" : CLR_MUTED "░" CLR_ACCENT);
    printf("]" CLR_RESET " " CLR_SUCCESS "%3.0f%%" CLR_RESET " " CLR_MUTED "%-25.25s" CLR_RESET, porcentaje * 100, archivo);
    fflush(stdout);
}

void ui_finalizar_estado(void)
{
    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LABEL);
    printf("\033[%d;1H\033[K", ROW_ARCHIVO_LINE);
    printf("\033[%d;1H  " CLR_SUCCESS " \xe2\x9c\x93 Proceso completado" CLR_RESET, ROW_ARCHIVO_LINE - 1);
}

void ui_imprimir_final(int total, Estadisticas stats, const char *destino, int abrir_explorador)
{
    printf("\033[%d;1H", ROW_RESULTADOS);
    printf("  " CLR_DIM CLR_BORDER "──" CLR_RESET " " CLR_BOLD "Resultados" CLR_RESET " " CLR_DIM CLR_BORDER "──────────────────────────────────────────────" CLR_RESET "\n");
    printf("\n");
    printf("  " CLR_BORDER "┌─────────────────────────────────────────────────┐" CLR_RESET "\n");
    printf("  " CLR_BORDER "│" CLR_RESET "                                                 " CLR_BORDER "│" CLR_RESET "\n");
    printf("  " CLR_BORDER "│" CLR_RESET "    " CLR_MUTED "Imagenes:" CLR_RESET "  " CLR_SUCCESS "%-3d" CLR_RESET "             " CLR_MUTED "Videos:" CLR_RESET "  " CLR_SUCCESS "%-3d" CLR_RESET "      " CLR_BORDER "│" CLR_RESET "\n", stats.imagenes, stats.videos);
    printf("  " CLR_BORDER "│" CLR_RESET "                                                 " CLR_BORDER "│" CLR_RESET "\n");
    printf("  " CLR_BORDER "│" CLR_RESET "    " CLR_MUTED "Total:" CLR_RESET " " CLR_BOLD CLR_ACCENT "%-3d" CLR_RESET "                                   " CLR_BORDER "│" CLR_RESET "\n", total);
    printf("  " CLR_BORDER "│" CLR_RESET "                                                 " CLR_BORDER "│" CLR_RESET "\n");
    printf("  " CLR_BORDER "└─────────────────────────────────────────────────┘" CLR_RESET "\n");
    if (stats.imagenes > 0 || stats.videos > 0)
    {
        printf("\n");
        printf("  " CLR_MUTED "Destino:" CLR_RESET " " CLR_INFO "%s" CLR_RESET "\n", destino);
#ifdef _WIN32
        if (abrir_explorador)
        {
            char comando[MAX_PATH_LEN + 20];
            snprintf(comando, sizeof(comando), "explorer \"%s\"", destino);
            system(comando);
        }
#endif
    }
    else
    {
        printf("\n  " CLR_MUTED " No se procesaron archivos multimedia." CLR_RESET "\n");
    }
}
