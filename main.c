/*
 * File Monitor - surveillance de fichiers depuis la racine
 * C orienté objet, bibliothèque standard + API Linux (inotify)
 * Configuration via config.yaml
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "file_monitor.h"

static FileMonitor *g_monitor = NULL;

static void signal_handler(int sig) {
    (void)sig;
    if (g_monitor) {
        g_monitor->stop(g_monitor);
    }
}

int main(int argc, char **argv) {
    Config cfg;
    FileMonitor *mon;
    const char *config_path = "config.yaml";

    if (argc > 1)
        config_path = argv[1];

    printf("File Monitor - Chargement config: %s\n", config_path);

    if (config_parse(config_path, &cfg) < 0) {
        fprintf(stderr, "Erreur: impossible de charger %s\n", config_path);
        return 1;
    }

    config_print(&cfg);

    mon = file_monitor_create();
    if (!mon) {
        fprintf(stderr, "Erreur: allocation du moniteur\n");
        return 1;
    }

    g_monitor = mon;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (file_monitor_init(mon, &cfg) < 0) {
        fprintf(stderr, "Erreur: initialisation du moniteur (inotify)\n");
        fprintf(stderr, "Note: surveiller / necessite les droits root\n");
        file_monitor_destroy(mon);
        return 1;
    }

    printf("Surveillance active. Log: %s (Ctrl+C pour arreter)\n", cfg.log_file);
    file_monitor_run(mon);

    file_monitor_destroy(mon);
    g_monitor = NULL;
    printf("Arrete.\n");
    return 0;
}
