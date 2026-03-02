/*
 * Logger - écriture des événements dans un fichier txt
 * Bibliothèque standard uniquement
 */
#include "include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/util.h"

static void log_event_impl(Logger *self, const char *event_type, const char *path, const char *detail) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[64];

    if (!self || !self->fp) return;

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    fprintf(self->fp, "[%s] %s | %s | %s\n", buf, event_type, path, detail ? detail : "");
    fflush(self->fp);
}

static void logger_close_impl(Logger *self) {
    if (self && self->fp) {
        fclose(self->fp);
        self->fp = NULL;
    }
}

Logger* logger_create(const char *log_path) {
    Logger *log = (Logger*)malloc(sizeof(Logger));
    if (!log) return NULL;

    memset(log, 0, sizeof(Logger));
    ft_strlcpy(log->path, log_path, sizeof(log->path));

    log->fp = fopen(log_path, "a");
    if (!log->fp) {
        free(log);
        return NULL;
    }

    log->log_event = log_event_impl;
    log->close = logger_close_impl;
    return log;
}

void logger_destroy(Logger *log) {
    if (log) {
        log->close(log);
        free(log);
    }
}
