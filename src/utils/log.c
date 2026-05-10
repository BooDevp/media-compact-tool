#include <config.h>

#if MODE_DEV

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <log.h>

static FILE *log_fp = NULL;

#ifdef _WIN32
static CRITICAL_SECTION g_log_lock;
static int g_log_lock_inicializada = 0;
#endif

void log_init(void)
{
#ifdef _WIN32
    InitializeCriticalSection(&g_log_lock);
    g_log_lock_inicializada = 1;
#endif

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

#ifdef _WIN32
    if (g_log_lock_inicializada) EnterCriticalSection(&g_log_lock);
#endif

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

#ifdef _WIN32
    if (g_log_lock_inicializada) LeaveCriticalSection(&g_log_lock);
#endif
}

void log_close(void)
{
    if (log_fp)
    {
        log_printf("=== FIN DE SESION ===");
        fclose(log_fp);
        log_fp = NULL;
    }

#ifdef _WIN32
    if (g_log_lock_inicializada)
    {
        DeleteCriticalSection(&g_log_lock);
        g_log_lock_inicializada = 0;
    }
#endif
}

#endif
