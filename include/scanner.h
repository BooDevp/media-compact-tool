#ifndef SCANNER_H
#define SCANNER_H

typedef struct {
    int imagenes;
    int videos;
} Stats;

int procesar_recursivo(const char *dir_in, const char *dir_out, Stats *stats);

int contar_media_recursiva(const char *dir_in, int *acumulado);

int procesar_recursivo_con_progreso(const char *dir_in, const char *dir_out, Stats *stats, int total_files, int *processed_count);

#endif