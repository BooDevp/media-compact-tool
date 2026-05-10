#ifndef LOG_H
#define LOG_H

#include <config.h>

#if MODE_DEV
void log_init(void);
void log_printf(const char *fmt, ...);
void log_close(void);
#else
#define log_init()    ((void)0)
#define log_printf(fmt, ...) ((void)0)
#define log_close()   ((void)0)
#endif

#endif
