#include <stdio.h>
#include <stdlib.h>
#include "../../include/ui.h"

// Mapa de coordenadas para la interfaz de usuario en terminal
#define ROW_START    6   
#define ROW_TOTAL    7   // Fila del Progreso Total
#define ROW_FILE     9   // Fila del Archivo actual
#define ROW_SUMMARY  12  // El resumen empieza aquí

// Funciones para mostrar la interfaz de usuario en terminal con barras de progreso y estados dinámicos
void ui_imprimir_header() {    
    printf("\033[1;1H"); 
    printf("\n  " BG_BLUE FG_WHITE BOLD " MEDIA COMPACTOR v1.0 " RESET);
    printf(TEXT_GRAY " | HEVC & libvips\n" RESET);
    printf(DIM "  ───────────────────────────────────────────────────────────────\n" RESET);
}

// Muestra una zona de "drop" para que el usuario arrastre la carpeta y pulse ENTER
void ui_mostrar_drop_zone() {    
    printf("\n" TEXT_GRAY "  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │                                                             │\n");
    printf("  │    " RESET BOLD "ARRASTRA UNA CARPETA AQUÍ Y PULSA ENTER PARA EMPEZAR" RESET TEXT_GRAY "     │\n");
    printf("  │                                                             │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n" RESET);
    printf(SHOW_CURSOR "\n  > " TEXT_CYAN); 
}

void ui_animar_analisis(int actual) {
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    
    printf("\033[%d;1H\033[K", ROW_TOTAL); 
    printf("  " TEXT_GRAY "%s Analizando archivos... " RESET "(%d encontrados)", spinner[frame], actual);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

// Actualiza la barra de progreso para el archivo actual
void ui_barra_progreso_total(double porcentaje, int processed, int total) {
    int ancho_barra = 30;
    int rellenos = (int)(ancho_barra * porcentaje);

    printf("\033[%d;1H\033[K", ROW_TOTAL); 
    printf("  " TEXT_CYAN "Progreso Total " RESET "[");
    for (int i = 0; i < ancho_barra; i++) {
        if (i < rellenos) printf(TEXT_CYAN "#" RESET);
        else printf(DIM "-" RESET);
    }
    printf("] " BOLD "%3.0f%%" RESET " (%d/%d)", porcentaje * 100, processed, total);
    fflush(stdout);
}

// Actualiza el estado dinámico para el archivo que se está procesando
void ui_loading_imagen(const char *archivo) {
    static int frame = 0;
    const char *spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("  " TEXT_CYAN ">> " RESET "IMAGEN " TEXT_BLUE "[%s] " RESET TEXT_GRAY "Procesando: %-35.35s" RESET, spinner[frame], archivo);
    frame = (frame + 1) % 10;
    fflush(stdout);
}

// Actualiza la barra de progreso para un vídeo específico
void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje) {
    int ancho_barra = 15;
    int rellenos = (int)(ancho_barra * porcentaje);
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("  " TEXT_CYAN ">> " RESET "%-7s " TEXT_BLUE "[", tipo);
    for (int i = 0; i < ancho_barra; i++) {
        printf(i < rellenos ? "█" : "░");
    }
    printf("] " RESET TEXT_GREEN "%3.0f%% " RESET TEXT_GRAY "%-25.25s" RESET, porcentaje * 100, archivo);
    fflush(stdout);
}

//  Limpia la línea de estado al finalizar el proceso
void ui_finalizar_estado() {    
    printf("\033[%d;1H\033[K", ROW_FILE);
    printf("\033[%d;1H  " TEXT_GREEN ">> PROCESO COMPLETADO" RESET "\n", ROW_SUMMARY - 2);
}

// Imprime el resumen final con estadísticas de imágenes y vídeos procesados
void ui_imprimir_final(int total, Stats stats, const char *carpeta_out) {
    printf("\033[%d;1H", ROW_SUMMARY);
    printf(DIM "  ───────────────────────────────────────────────────────────────\n" RESET);
    printf("\n");
    printf(BOLD TEXT_CYAN "  RESUMEN DE OPERACIÓN\n" RESET);
    printf(TEXT_GRAY "  ┌───────────────────────────────────────────────────┐\n" RESET);
    printf(TEXT_GRAY "  │  " RESET "Imágenes optimizadas:   " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.imagenes);
    printf(TEXT_GRAY "  │  " RESET "Vídeos   optimizados:   " BOLD TEXT_GREEN "%-25d" RESET TEXT_GRAY "│\n" RESET, stats.videos);
    printf(TEXT_GRAY "  │  " RESET "Total:                  " BOLD TEXT_CYAN "%-25d" RESET TEXT_GRAY "│\n" RESET, total);
    printf(TEXT_GRAY "  └───────────────────────────────────────────────────┘\n" RESET);
    printf("\n  " BOLD "Destino: " RESET TEXT_GRAY "%s\n" RESET, carpeta_out);
}

// Muestra un mensaje de error si no se han proporcionado argumentos válidos
void ui_error_args() {    
    printf(SHOW_CURSOR "\n  \033[41m ERROR \033[0m No se ha proporcionado una carpeta válida.\n");
    printf(DIM "  Asegúrate de arrastrar la carpeta correctamente.\n\n" RESET);
}