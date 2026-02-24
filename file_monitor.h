#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

#include "config.h"
#include "logger.h"
#include "auditd.h"

/*
 * FileMonitor - système de surveillance de fichiers (style objet C)
 * Utilise inotify (API Linux, sys/inotify.h - partie de glibc)
 * Intègre AuditMonitor pour retrouver l'utilisateur à l'origine de l'événement.
 */
typedef struct FileMonitor FileMonitor;

struct FileMonitor {
    Config *config;
    Logger *logger;
    AuditMonitor *audit;
    int inotify_fd;
    int running;

    /* méthodes */
    int (*init)(FileMonitor *self, Config *cfg);
    void (*run)(FileMonitor *self);
    void (*stop)(FileMonitor *self);
    void (*destroy)(FileMonitor *self);
};

FileMonitor* file_monitor_create(void);
int file_monitor_init(FileMonitor *self, Config *cfg);
void file_monitor_run(FileMonitor *self);
void file_monitor_stop(FileMonitor *self);
void file_monitor_destroy(FileMonitor *self);

#endif /* FILE_MONITOR_H */
