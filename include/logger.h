#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef struct Logger Logger;

struct Logger {
    FILE *fp;
    char path[256];

    /* méthodes */
    void (*log_event)(Logger *self, const char *event_type, const char *path, const char *detail);
    void (*close)(Logger *self);
};

Logger* logger_create(const char *log_path);
void logger_destroy(Logger *log);

#endif /* LOGGER_H */

