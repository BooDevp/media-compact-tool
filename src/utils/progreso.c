#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <progreso.h>
#include <config.h>

static cb_progreso_total_t g_cb_total = NULL;
static cb_progreso_archivo_t g_cb_archivo = NULL;
static cb_fin_t g_cb_fin = NULL;
static void *g_cb_user = NULL;

void progreso_registrar(cb_progreso_total_t cb_total,
                        cb_progreso_archivo_t cb_archivo,
                        cb_fin_t cb_fin,
                        void *userdata)
{
    g_cb_total = cb_total;
    g_cb_archivo = cb_archivo;
    g_cb_fin = cb_fin;
    g_cb_user = userdata;
}

void progreso_animar(int actual)
{
    if (g_cb_total)
        g_cb_total(0.0, actual, 0, g_cb_user);
}

void progreso_barra_total(double porcentaje, int processed, int total)
{
    if (g_cb_total)
    {
        g_cb_total(porcentaje, processed, total, g_cb_user);
        return;
    }
}

void progreso_loading(const char *archivo)
{
    if (g_cb_archivo)
        g_cb_archivo("", archivo, 0.0, g_cb_user);
}

void progreso_barra(const char *tipo, const char *archivo, double porcentaje)
{
    if (g_cb_archivo)
        g_cb_archivo(tipo, archivo, porcentaje, g_cb_user);
}

void progreso_imprimir_fin(int total, Estadisticas stats, const char *destino, int abrir_explorador)
{
    if (g_cb_fin)
        g_cb_fin(total, &stats, destino, abrir_explorador, g_cb_user);
}
