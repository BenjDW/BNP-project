/*
 * FileMonitor - surveillance de fichiers via inotify
 * API Linux inotify (sys/inotify.h) - pas de bibliothèques tierces
 * Log tous les événements de modification/création/suppression
 */
#include "file_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

/* inotify - API Linux standard (glibc) */
#include <sys/inotify.h>

#define EVENT_BUF_LEN (10 * (sizeof(struct inotify_event) + 256))
#define MAX_WATCHES 65536

typedef struct {
    int wd;
    char path[512];
} WatchEntry;

static WatchEntry g_watches[MAX_WATCHES];
static int g_watch_count = 0;

static int add_watch_path(FileMonitor *mon, const char *path);
static int is_excluded(const Config *cfg, const char *path);
static void process_events(FileMonitor *mon, char *buf, int len);
static const char* event_name(uint32_t mask);

static int add_watch_recursive(FileMonitor *mon, const char *path) {
    DIR *d;
    struct dirent *e;
    char subpath[1024];
    struct stat st;

    if (add_watch_path(mon, path) < 0) return -1;

    d = opendir(path);
    if (!d) return 0;

    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;

        snprintf(subpath, sizeof(subpath), "%s/%s", path, e->d_name);

        if (stat(subpath, &st) < 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;
        if (is_excluded(mon->config, subpath)) continue;

        add_watch_recursive(mon, subpath);
    }
    closedir(d);
    return 0;
}

static int add_watch_path(FileMonitor *mon, const char *path) {
    int wd;

    if (g_watch_count >= MAX_WATCHES) return -1;

    wd = inotify_add_watch(mon->inotify_fd, path,
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM | IN_ATTRIB | IN_CLOSE_WRITE);
    if (wd < 0) return -1;

    g_watches[g_watch_count].wd = wd;
    strncpy(g_watches[g_watch_count].path, path, sizeof(g_watches[g_watch_count].path) - 1);
    g_watch_count++;
    return 0;
}

static int is_excluded(const Config *cfg, const char *path) {
    int i;
    size_t plen = strlen(path), elen;
    for (i = 0; i < cfg->exclude_count; i++) {
        elen = strlen(cfg->exclude_paths[i]);
        if (plen >= elen && strncmp(path, cfg->exclude_paths[i], elen) == 0) {
            if (plen == elen || path[elen] == '/') return 1;
        }
    }
    return 0;
}

static const char* watch_path_by_wd(int wd) {
    int i;
    for (i = 0; i < g_watch_count; i++)
        if (g_watches[i].wd == wd) return g_watches[i].path;
    return "?";
}

static const char* event_name(uint32_t mask) {
    if (mask & IN_MODIFY) return "MODIFY";
    if (mask & IN_CREATE) return "CREATE";
    if (mask & IN_DELETE) return "DELETE";
    if (mask & IN_MOVED_TO) return "MOVED_TO";
    if (mask & IN_MOVED_FROM) return "MOVED_FROM";
    if (mask & IN_ATTRIB) return "ATTRIB";
    if (mask & IN_CLOSE_WRITE) return "CLOSE_WRITE";
    return "UNKNOWN";
}

static void process_events(FileMonitor *mon, char *buf, int len) {
    struct inotify_event *ev;
    char fullpath[1024];
    const char *base;
    int i = 0;

    while (i < len) {
        ev = (struct inotify_event*)&buf[i];
        base = watch_path_by_wd(ev->wd);

        if (ev->len > 0) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", base, ev->name);
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s", base);
        }

        if (ev->len > 0 && is_excluded(mon->config, fullpath)) {
            i += sizeof(struct inotify_event) + ev->len;
            continue;
        }

        /* Ne pas logger les evenements sur le fichier de log (evite boucle feedback) */
        if (mon->config->log_file[0]) {
            const char *log_name = mon->config->log_file;
            const char *log_base = strrchr(log_name, '/');
            const char *full_base = strrchr(fullpath, '/');

            if (log_base)
                log_base++;
            else
                log_base = log_name;

            if (full_base)
                full_base++;
            else
                full_base = fullpath;

            if (strcmp(full_base, log_base) == 0) {
                i += sizeof(struct inotify_event) + ev->len;
                continue;
            }
        }

        if (mon->logger && mon->logger->log_event) {
            mon->logger->log_event(mon->logger, event_name(ev->mask), fullpath,
                ev->mask & IN_ISDIR ? "(directory)" : NULL);
        }

        /* si récursif et nouveau répertoire créé, l'ajouter */
        if (mon->config->recursive && (ev->mask & IN_CREATE) && (ev->mask & IN_ISDIR) && ev->len > 0) {
            if (!is_excluded(mon->config, fullpath))
                add_watch_path(mon, fullpath);
        }

        i += sizeof(struct inotify_event) + ev->len;
    }
}

int file_monitor_init_impl(FileMonitor *self, Config *cfg) {
    int i;

    self->config = cfg;
    self->inotify_fd = inotify_init();
    if (self->inotify_fd < 0) return -1;

    self->logger = logger_create(cfg->log_file);
    if (!self->logger) {
        close(self->inotify_fd);
        return -1;
    }

    self->logger->log_event(self->logger, "START", "file_monitor", "Surveillance demarree");

    g_watch_count = 0;
    for (i = 0; i < cfg->watch_count; i++) {
        if (!is_excluded(cfg, cfg->watch_paths[i])) {
            if (cfg->recursive)
                add_watch_recursive(self, cfg->watch_paths[i]);
            else
                add_watch_path(self, cfg->watch_paths[i]);
        }
    }

    self->running = 1;
    return 0;
}

void file_monitor_run_impl(FileMonitor *self) {
    char buf[EVENT_BUF_LEN];
    int len;

    while (self->running) {
        len = read(self->inotify_fd, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (len > 0)
            process_events(self, buf, len);
    }
}

void file_monitor_stop_impl(FileMonitor *self) {
    self->running = 0;
}

void file_monitor_destroy_impl(FileMonitor *self) {
    if (self) {
        if (self->logger) {
            self->logger->log_event(self->logger, "STOP", "file_monitor", "Surveillance arretee");
            logger_destroy(self->logger);
            self->logger = NULL;
        }
        if (self->inotify_fd >= 0) {
            close(self->inotify_fd);
            self->inotify_fd = -1;
        }
    }
}

FileMonitor* file_monitor_create(void) {
    FileMonitor *m = (FileMonitor*)malloc(sizeof(FileMonitor));
    if (!m) return NULL;
    memset(m, 0, sizeof(FileMonitor));
    m->inotify_fd = -1;
    m->init = file_monitor_init_impl;
    m->run = file_monitor_run_impl;
    m->stop = file_monitor_stop_impl;
    m->destroy = file_monitor_destroy_impl;
    return m;
}

int file_monitor_init(FileMonitor *self, Config *cfg) {
    return self->init(self, cfg);
}

void file_monitor_run(FileMonitor *self) {
    self->run(self);
}

void file_monitor_stop(FileMonitor *self) {
    self->stop(self);
}

void file_monitor_destroy(FileMonitor *self) {
    if (self) {
        self->destroy(self);
        free(self);
    }
}
