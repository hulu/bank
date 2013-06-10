#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdarg.h>

typedef enum LogLevel {
	INFO = 0, DEBUG = 1, WARNING = 2, ERROR = 3, CRITICAL = 4,
} LogLevel;

extern char *LogLevelNames[];

void log_critical(char *template, ...);

void log_error(char *template, ...);

void log_warning(char *template, ...);

void log_debug(char *template, ...);

void log_info(char *template, ...);

int log_init(const char *file_name, LogLevel level);

void *periodic_flush(void *arg);

void log_flush();

void log_msg(LogLevel level, char *template, va_list args);

#endif /* LOGGER_H_ */
