#ifndef SCANNER_H
#define SCANNER_H

typedef struct {
    int imagenes;
    int videos;
} Stats;

int procesar_recursivo(const char *dir_in, const char *dir_out, Stats *stats);

#endif