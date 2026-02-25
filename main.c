/*
 * File Monitor - surveillance de fichiers depuis la racine
 * C orienté objet, bibliothèque standard + API Linux (inotify)
 * Configuration via config.yaml
 *
 * Modes :
 *   ./file_monitor [config.yaml]         -> mode classique (foreground)
 *   ./file_monitor start [config.yaml]   -> daemon (background + pidfile)
 *   ./file_monitor stop                  -> arrêter le daemon via pidfile
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "file_monitor.h"

#define PIDFILE "file_monitor.pid"

/* Prototype explicite pour kill() (POSIX) afin d'eviter les warnings
 * d'implicit declaration avec certains niveaux de standard C. */
int kill(pid_t pid, int sig);

static FileMonitor *g_monitor = NULL;

static void signal_handler(int sig) {
    (void)sig;
    if (g_monitor) {
        g_monitor->stop(g_monitor);
    }
}

static int write_pidfile(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fprintf(f, "%d\n", (int)getpid());
    fclose(f);
    return 0;
}

static int read_pidfile(const char *path, pid_t *pid_out)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (fscanf(f, "%d", pid_out) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int daemonize(void)
{
    pid_t pid;
    int fd;

    pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        exit(0); /* parent */

    if (setsid() < 0)
        return -1;

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        exit(0); /* premier enfant */

    umask(0);

    /* Fermer les descripteurs standards et les rediriger vers /dev/null */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    fd = open("/dev/null", O_RDONLY);
    if (fd >= 0 && fd != STDIN_FILENO)
        dup2(fd, STDIN_FILENO);
    fd = open("/dev/null", O_WRONLY);
    if (fd >= 0 && fd != STDOUT_FILENO)
        dup2(fd, STDOUT_FILENO);
    fd = open("/dev/null", O_RDWR);
    if (fd >= 0 && fd != STDERR_FILENO)
        dup2(fd, STDERR_FILENO);

    return 0;
}

int main(int argc, char **argv) {
    Config cfg;
    FileMonitor *mon;
    const char *config_path = "config.yaml";
    int daemon_mode = 0;

    /* Gestion des modes : start/stop/foreground */
    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        pid_t pid;
        if (read_pidfile(PIDFILE, &pid) < 0) {
            fprintf(stderr, "Erreur: impossible de lire %s\n", PIDFILE);
            return 1;
        }
        if (kill(pid, SIGTERM) < 0) {
            perror("kill");
            return 1;
        }
        printf("Signal SIGTERM envoye au daemon (pid=%d)\n", (int)pid);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "start") == 0) {
        daemon_mode = 1;
        if (argc > 2)
            config_path = argv[2];
    } else if (argc > 1) {
        config_path = argv[1];
    }

    /* Message de démarrage toujours affiche dans le terminal */
    printf("File Monitor - Chargement config: %s\n", config_path);

    if (config_parse(config_path, &cfg) < 0) {
        fprintf(stderr, "Erreur: impossible de charger %s\n", config_path);
        return 1;
    }

    config_print(&cfg);

    if (daemon_mode) {
        if (daemonize() < 0) {
            fprintf(stderr, "Erreur: echec de la mise en daemon\n");
            return 1;
        }
        if (write_pidfile(PIDFILE) < 0) {
            /* On continue quand même, mais le stop sera indisponible */
        }
    }

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

    if (!daemon_mode)
        printf("Surveillance active. Log: %s (Ctrl+C pour arreter)\n", cfg.log_file);

    file_monitor_run(mon);

    file_monitor_destroy(mon);
    g_monitor = NULL;
    if (!daemon_mode)
        printf("Arrete.\n");
    return 0;
}
