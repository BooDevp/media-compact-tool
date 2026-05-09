#include <string.h>
#include <stdio.h>

#include <str_util.h>

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
