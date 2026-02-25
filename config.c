/*
 * Parser YAML minimal - bibliothèque standard uniquement
 * Supporte: watch, exclude, log_file, recursive
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define MAX_LINE 512

static void trim(char *s) {
    char *start = s, *end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s) {
        end = s + strlen(s) - 1;
        while (end > s && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
        if (s != start)
            memmove(start, s, (size_t)(end - s + 2));
    } else {
        *start = '\0';
    }
}

void config_init(Config *cfg) {
    memset(cfg, 0, sizeof(Config));
    cfg->recursive = 1;
    strncpy(cfg->log_file, "file_monitor.log", MAX_LOG_PATH - 1);
}

int config_parse(const char *path, Config *cfg) {
    FILE *f;
    char line[MAX_LINE];
    char key[128], value[MAX_PATH_LEN];
    int in_watch = 0, in_exclude = 0;
    int i;

    config_init(cfg);

    f = fopen(path, "r");
    if (!f) return -1;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (!line[0] || line[0] == '#')
            continue;
        if (line[0] == '-' && in_watch) {
            strncpy(value, line + 1, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
            trim(value);
            if (cfg->watch_count < MAX_PATHS && strlen(value) > 0) {
                strncpy(cfg->watch_paths[cfg->watch_count], value, MAX_PATH_LEN - 1);
                cfg->watch_paths[cfg->watch_count][MAX_PATH_LEN - 1] = '\0';
                cfg->watch_count++;
            }
            continue;
        }
        if (line[0] == '-' && in_exclude) {
            strncpy(value, line + 1, sizeof(value) - 1);
            trim(value);
            if (cfg->exclude_count < MAX_PATHS && strlen(value) > 0) {
                strncpy(cfg->exclude_paths[cfg->exclude_count], value, MAX_PATH_LEN - 1);
                cfg->exclude_paths[cfg->exclude_count][MAX_PATH_LEN - 1] = '\0';
                cfg->exclude_count++;
            }
            continue;
        }

        in_watch = 0;
        in_exclude = 0;
        key[0] = value[0] = '\0';

        if (sscanf(line, "%127[^:]:%511[^\n]", key, value) >= 1) {
            trim(key);
            trim(value);
            for (i = 0; value[i] && value[i] != '#'; i++);
            value[i] = '\0';
            trim(value);
        }

        if (strcmp(key, "watch") == 0) {
            in_watch = 1;
            if (strlen(value) > 0 && cfg->watch_count < MAX_PATHS) {
                strncpy(cfg->watch_paths[cfg->watch_count], value, MAX_PATH_LEN - 1);
                cfg->watch_paths[cfg->watch_count][MAX_PATH_LEN - 1] = '\0';
                cfg->watch_count++;
            }
        } else if (strcmp(key, "exclude") == 0) {
            in_exclude = 1;
            if (strlen(value) > 0 && cfg->exclude_count < MAX_PATHS) {
                strncpy(cfg->exclude_paths[cfg->exclude_count], value, MAX_PATH_LEN - 1);
                cfg->exclude_paths[cfg->exclude_count][MAX_PATH_LEN - 1] = '\0';
                cfg->exclude_count++;
            }
        } else if (strcmp(key, "log_file") == 0) {
            if (strlen(value) > 0)
            {
                strncpy(cfg->log_file, value, MAX_LOG_PATH - 1);
                cfg->log_file[MAX_LOG_PATH - 1] = '\0';
            }
        } else if (strcmp(key, "recursive") == 0) {
            cfg->recursive = (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 || value[0] == '1');
        }
    }

    fclose(f);

    /* Si aucun watch, utiliser / par défaut */
    if (cfg->watch_count == 0) {
        strncpy(cfg->watch_paths[0], "/", MAX_PATH_LEN - 1);
        cfg->watch_paths[0][MAX_PATH_LEN - 1] = '\0';
        cfg->watch_count = 1;
    }

    return 0;
}

void config_print(const Config *cfg) {
    int i;
    printf("Config: watch_count=%d exclude_count=%d recursive=%d log=%s\n",
           cfg->watch_count, cfg->exclude_count, cfg->recursive, cfg->log_file);
    for (i = 0; i < cfg->watch_count; i++)
        printf("  watch: %s\n", cfg->watch_paths[i]);
    for (i = 0; i < cfg->exclude_count; i++)
        printf("  exclude: %s\n", cfg->exclude_paths[i]);
}
