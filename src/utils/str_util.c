#include <string.h>
#include <stdio.h>

#include <str_util.h>
#include <config.h>

void limpiar_ruta(char *r)
{
    r[strcspn(r, "\n")] = 0;
    r[strcspn(r, "\r")] = 0;
    if (r[0] == '"')
    {
        memmove(r, r + 1, strlen(r));
        size_t l = strlen(r);
        if (l > 0 && r[l - 1] == '"')
            r[l - 1] = 0;
    }
    size_t len = strlen(r);
    if (len > 0 && (r[len - 1] == '/' || r[len - 1] == '\\'))
        r[len - 1] = '\0';
}

void generar_ruta_salida_archivo(const char *ruta_entrada, char *salida, size_t tam)
{
    const char *punto = strrchr(ruta_entrada, '.');
    if (punto)
    {
        size_t prefijo_len = (size_t)(punto - ruta_entrada);
        snprintf(salida, tam, "%.*s%s%s", (int)prefijo_len, ruta_entrada, SUFIJO_CARPETA, punto);
    }
    else
    {
        snprintf(salida, tam, "%s%s", ruta_entrada, SUFIJO_CARPETA);
    }
}
