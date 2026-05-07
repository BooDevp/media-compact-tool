#include <stdio.h>
#include <stdlib.h>
#include "../../include/ui.h"

void ui_imprimir_header()
{
    printf(HIDE_CURSOR "\033[H\033[J");
    printf("\n");
    printf("  ");
    printf(BG_BLUE FG_WHITE BOLD " MEDIA COMPACTOR v1.0 " RESET);
    printf(TEXT_GRAY " | HEVC & libvips\n" RESET);
    printf(DIM "  ───────────────────────────────────────────────────────────────\n\n" RESET);
}

void ui_mostrar_drop_zone()
{
    printf(TEXT_GRAY "  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │                                                             │\n");
    printf("  │    " RESET BOLD "ARRASTRA UNA CARPETA AQUÍ Y PULSA ENTER PARA EMPEZAR" RESET TEXT_GRAY "     │\n");
    printf("  │                                                             │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n" RESET);
    printf(SHOW_CURSOR "\n  > " TEXT_CYAN); // Mostramos el cursor para que sepa dónde cae la ruta
}

// Icono de carga para procesos rápidos (Imágenes)
void ui_loading_imagen(const char *archivo)
{
    // Frames de la animación
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int total_frames = 10;

    printf("\r\033[K  " TEXT_CYAN ">> " RESET BOLD "IMAGEN   " RESET);

    // Imprimimos el frame actual del spinner
    printf(TEXT_BLUE "[%s] " RESET, spinner[frame]);

    printf(TEXT_GRAY "Procesando: %-25.25s" RESET, archivo);

    // Avanzamos el frame para la próxima llamada
    frame = (frame + 1) % total_frames;

    fflush(stdout);
}

// Barra de progreso para procesos largos (Vídeos)
void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje)
{
    int ancho_barra = 20;
    int rellenos = (int)(ancho_barra * porcentaje);

    // Mantenemos estructura fija para evitar saltos visuales
    printf("\r  " TEXT_CYAN ">> " RESET BOLD "%-8s " RESET, tipo);

    // Dibujar la barra con bloques Unicode
    printf(TEXT_BLUE "[");
    for (int i = 0; i < ancho_barra; i++)
    {
        if (i < rellenos)
            printf("█");
        else if (i == rellenos)
            printf("▒");
        else
            printf("░");
    }
    printf("] " RESET);

    // Formato fijo: %3.0f%% para el número y %-25.25s para el nombre
    printf(TEXT_GREEN "%3.0f%% " RESET TEXT_GRAY "Procesando: %-25.25s" RESET, porcentaje * 100, archivo);

    fflush(stdout);
}

void ui_finalizar_estado()
{
    printf("\r\033[K  " TEXT_GREEN ">> FINALIZADO" RESET "\n" SHOW_CURSOR);
}

void ui_imprimir_final(int total, Stats stats, const char *carpeta_out)
{
    printf("\n" DIM "  ───────────────────────────────────────────────────────────────\n" RESET);

    if (total > 0)
    {
        printf(BOLD TEXT_CYAN "  RESUMEN DE OPERACIÓN\n" RESET);
        printf(TEXT_GRAY "  ┌──────────────────────────────────────────┐\n" RESET);
        printf(TEXT_GRAY "  │  " RESET "Imágenes:  " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.imagenes);
        printf(TEXT_GRAY "  │  " RESET "Vídeos:    " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.videos);
        printf(TEXT_GRAY "  │  " RESET "Total:     " BOLD TEXT_CYAN "%-25d" RESET TEXT_GRAY "│\n" RESET, total);
        printf(TEXT_GRAY "  └──────────────────────────────────────────┘\n" RESET);
        printf("\n  " BOLD "Destino: " RESET TEXT_GRAY "%s\n\n" RESET, carpeta_out);
    }
    else
    {
        printf("  " TEXT_CYAN "INFO: " RESET "No se detectaron archivos nuevos.\n\n");
    }
    printf(SHOW_CURSOR);
}

void ui_error_args()
{
    printf(SHOW_CURSOR "\n  \033[41m ERROR \033[0m Falta la carpeta de origen.\n");
    printf(DIM "  Uso: compactador.exe <ruta>\n\n" RESET);
}