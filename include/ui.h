#ifndef UI_H
#define UI_H

#include "types.h"

#define BOLD "\033[1m"
#define RESET "\033[0m"
#define DIM "\033[2m"
#define BG_BLUE "\033[48;5;27m"
#define FG_WHITE "\033[38;5;255m"

#define TEXT_CYAN "\033[38;5;51m"
#define TEXT_GREEN "\033[38;5;82m"
#define TEXT_GRAY "\033[38;5;245m"
#define TEXT_BLUE "\033[38;5;33m"

#define CLR_RED "\033[38;5;196m"
#define CLR_RESET RESET

#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

void ui_imprimir_header(void);
void ui_mostrar_drop_zone(void);
void ui_barra_progreso(const char *tipo, const char *archivo, double porcentaje);
void ui_loading_imagen(const char *archivo);
void ui_finalizar_estado(void);
void ui_animar_analisis(int actual);
void ui_imprimir_final(int total, Estadisticas stats, const char *carpeta_out);
void ui_barra_progreso_total(double porcentaje, int processed, int total);

#endif