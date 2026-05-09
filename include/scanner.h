#ifndef SCANNER_H
#define SCANNER_H

#include "types.h"

int es_directorio(const char *ruta);
int procesar_recursivo(const char *dir_in, const char *dir_out, Estadisticas *stats);
int contar_media_recursiva(const char *dir_in, int *acumulado);
int procesar_recursivo_con_progreso(const char *dir_in, const char *dir_out, Estadisticas *stats, int total_files, int *processed_count);
int procesar_archivo_unico(const char *ruta_in, const char *ruta_out, Estadisticas *stats);

#endif