#ifndef LOG_H
#define LOG_H

void log_init(void);
void log_printf(const char *fmt, ...);
void log_close(void);

#endif
