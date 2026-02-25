#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATHS 256
#define MAX_PATH_LEN 512
#define MAX_LOG_PATH 256

typedef struct {
    char watch_paths[MAX_PATHS][MAX_PATH_LEN];
    int watch_count;
    char exclude_paths[MAX_PATHS][MAX_PATH_LEN];
    int exclude_count;
    char log_file[MAX_LOG_PATH];
    int recursive;
} Config;

/* Parse YAML config file - retourne 0 si OK, -1 si erreur */
int config_parse(const char *path, Config *cfg);

void config_init(Config *cfg);
void config_print(const Config *cfg);

#endif /* CONFIG_H */

