#ifndef UI_H
#define UI_H

#include "types.h"

// ── Formato de texto ──
#define CLR_BOLD  "\033[1m"
#define CLR_DIM   "\033[2m"
#define CLR_RESET "\033[0m"

// ── Paleta Ocean (compatible Windows Terminal) ──
#define CLR_ACCENT  "\033[38;5;45m"    // Cian activo
#define CLR_INFO    "\033[38;5;75m"    // Azul informacion
#define CLR_SUCCESS "\033[38;5;48m"    // Verde exito
#define CLR_MUTED   "\033[38;5;243m"   // Gris secundario
#define CLR_BORDER  "\033[38;5;236m"   // Gris bordes
#define CLR_WARN    "\033[38;5;221m"   // Amarillo aviso
#define CLR_HL_BG   "\033[48;5;17m"    // Fondo header
#define CLR_HL_FG   "\033[38;5;255m"   // Texto header

// ── Alias retrocompatibles (para main.c y scanner.c) ──
#define BOLD      CLR_BOLD
#define DIM       CLR_DIM
#define RESET     CLR_RESET
#define TEXT_CYAN CLR_ACCENT
#define TEXT_GREEN CLR_SUCCESS
#define TEXT_GRAY CLR_MUTED
#define TEXT_BLUE CLR_INFO
#define BG_BLUE   CLR_HL_BG
#define FG_WHITE  CLR_HL_FG

// ── Control de cursor ──
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

// ── Funciones de UI ──
void ui_imprimir_header(void);
void ui_mostrar_drop_zone(void);
void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje);
void ui_loading_imagen(const char *archivo);
void ui_finalizar_estado(void);
void ui_animar_analisis(int actual);
void ui_imprimir_final(int total, Estadisticas stats, const char *destino, int abrir_explorador);
void ui_barra_progreso_total(double porcentaje, int processed, int total);

#endif
