#ifndef PROGRESO_H
#define PROGRESO_H

#include "types.h"

typedef void (*cb_progreso_total_t)(double pct, int procesados, int total, void *user);
typedef void (*cb_progreso_archivo_t)(const char *tipo, const char *nombre, double pct, void *user);
typedef void (*cb_fin_t)(int total, Estadisticas *stats, const char *destino, int abrir_explorador, void *user);

void progreso_registrar(cb_progreso_total_t cb_total,
                        cb_progreso_archivo_t cb_archivo,
                        cb_fin_t cb_fin,
                        void *userdata);

void progreso_barra(const char *tipo, const char *archivo, double porcentaje);
void progreso_loading(const char *archivo);
void progreso_animar(int actual);
void progreso_imprimir_fin(int total, Estadisticas stats, const char *destino, int abrir_explorador);
void progreso_barra_total(double porcentaje, int processed, int total);

#endif
