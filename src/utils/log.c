#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>

#include <log.h>
#include <config.h>

static FILE *log_fp = NULL;

void log_init(void)
{
#ifdef _WIN32
    mkdir("log");
#else
    mkdir("log", 0777);
#endif

    char ruta[MAX_PATH_LEN];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    strftime(ruta, sizeof(ruta), "log/compactador_%Y%m%d_%H%M%S.log", tm);

    log_fp = fopen(ruta, "w");
    if (!log_fp) return;

    fprintf(log_fp, "=== INICIO DE SESION %s ===\n", ruta + 4);
    fflush(log_fp);
}

void log_printf(const char *fmt, ...)
{
    if (!log_fp) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    fprintf(log_fp, "[%s] ", ts);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_fp, fmt, args);
    va_end(args);

    fprintf(log_fp, "\n");
    fflush(log_fp);
}

void log_close(void)
{
    if (log_fp)
    {
        log_printf("=== FIN DE SESION ===");
        fclose(log_fp);
        log_fp = NULL;
    }
}
