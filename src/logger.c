#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "logger.h"

LogLevel log_level = INFO;
FILE *logger = NULL;

char *LogLevelNames[] = { "INFO", "DEBUG", "WARNING", "ERROR", "CRITICAL", };

int log_init(const char *file_name, LogLevel level) {
	if (logger != NULL ) {
		fclose(logger);
	}
	logger = fopen(file_name, "a");
	if (logger == NULL ) {
		printf("File open error %s\n", file_name);
		return 0;
	}
	log_level = level;
	pthread_t flushing_thread = 0;
	pthread_create(&flushing_thread, NULL, &periodic_flush, NULL );

	return 1;
}

void *periodic_flush(void *arg) {
	while (1) {
		log_flush();
		sleep(2);
	}
	return NULL ;
}

void log_flush() {
	if (logger != NULL ) {
		fflush(logger);
	}
}

void log_critical(char *template, ...) {
	va_list args;
	va_start(args, template);
	log_msg(CRITICAL, template, args);
	log_flush();
}

void log_error(char *template, ...) {
	va_list args;
	va_start(args, template);
	log_msg(ERROR, template, args);
}

void log_warning(char *template, ...) {
	va_list args;
	va_start(args, template);
	log_msg(WARNING, template, args);
}

void log_debug(char *template, ...) {
	va_list args;
	va_start(args, template);
	log_msg(DEBUG, template, args);
}

void log_info(char *template, ...) {
	va_list args;
	va_start(args, template);
	log_msg(INFO, template, args);
}

void log_msg(LogLevel level, char *template, va_list args) {
	FILE *out = NULL;
	if (logger == NULL ) {
		out = stdout;
	} else {
		out = logger;
	}
	if ((int) level < (int) log_level) {
		return;
	}
	time_t t;
	struct tm *tinfo;
	time(&t);
	tinfo = localtime(&t);
	char buffer[100];
	strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", tinfo);
	fprintf(out, "[%s] - %s - ", LogLevelNames[(int) level], buffer);
	vfprintf(out, template, args);
	fprintf(out, "\n");
}
