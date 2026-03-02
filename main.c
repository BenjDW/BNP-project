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
#include <pwd.h>
#include <ctype.h>
#include <errno.h>

#include "include/config.h"
#include "include/file_monitor.h"
#include "include/auditd.h"
#include "include/util.h"

#define PIDFILE "build/file_monitor.pid"

/* Prototype explicite pour kill() (POSIX) afin d'eviter les warnings
 * d'implicit declaration avec certains niveaux de standard C. */
int kill(pid_t pid, int sig);

static FileMonitor *g_monitor = NULL;

/* --------------------------------------------------------------------------
 * Helpers pour les commandes "add" / "remove"
 * -------------------------------------------------------------------------- */

/* Résout un identifiant (user, pid, chemin) en chemin absolu à surveiller.
 *
 * Règles:
 *   - si arg commence par '/', il est traité comme un chemin.
 *   - si arg ne contient que des chiffres, il est traité comme un pid
 *     et on surveille /proc/<pid>/cwd.
 *   - sinon, il est traité comme un nom d'utilisateur (getpwnam_r),
 *     et on surveille son répertoire home.
 *
 * Retourne 0 si OK, -1 en cas d'erreur.
 */
static int resolve_watch_target(const char *arg, char *out, size_t outlen)
{
    size_t i;

    if (!arg || !out || outlen == 0)
        return -1;

    /* Chemin explicite */
    if (arg[0] == '/') {
        ft_strlcpy(out, arg, outlen);
        return 0;
    }

    /* PID numérique -> /proc/<pid>/cwd */
    int all_digits = 1;
    for (i = 0; arg[i]; i++) {
        if (!isdigit((unsigned char)arg[i])) {
            all_digits = 0;
            break;
        }
    }
    if (all_digits && i > 0) {
        char procpath[64];
        char buf[1024];
        ssize_t len;

        snprintf(procpath, sizeof(procpath), "/proc/%s/cwd", arg);
        len = readlink(procpath, buf, sizeof(buf) - 1);
        if (len < 0) {
            fprintf(stderr, "Erreur: impossible de resoudre le cwd du pid %s: %s\n",
                    arg, strerror(errno));
            return -1;
        }
        buf[len] = '\0';
        ft_strlcpy(out, buf, outlen);
        return 0;
    }

    /* Nom d'utilisateur -> home directory */
    {
        struct passwd pwd;
        struct passwd *result = NULL;
        long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (bufsize < 0)
            bufsize = 16384;
        char *buf = (char *)malloc((size_t)bufsize);
        int ret;

        if (!buf) {
            fprintf(stderr, "Erreur: allocation memoire getpwnam_r\n");
            return -1;
        }

        ret = getpwnam_r(arg, &pwd, buf, (size_t)bufsize, &result);
        if (ret != 0 || !result || !pwd.pw_dir || !pwd.pw_dir[0]) {
            fprintf(stderr, "Erreur: utilisateur introuvable: %s\n", arg);
            free(buf);
            return -1;
        }

        ft_strlcpy(out, pwd.pw_dir, outlen);
        free(buf);
        return 0;
    }
}

static int config_has_watch(const Config *cfg, const char *path)
{
    int i;

    if (!cfg || !path)
        return 0;
    for (i = 0; i < cfg->watch_count; i++) {
        if (strcmp(cfg->watch_paths[i], path) == 0)
            return 1;
    }
    return 0;
}

/* Réécrit le fichier de configuration avec les valeurs de cfg. */
static int config_write(const char *path, const Config *cfg)
{
    FILE *f;
    int i;

    if (!path || !cfg)
        return -1;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Erreur: impossible d'ecrire le fichier de config %s\n", path);
        return -1;
    }

    fprintf(f, "# Configuration du moniteur de fichiers\n");
    fprintf(f, "# Fichier genere automatiquement par les commandes 'add' / 'remove'\n\n");

    fprintf(f, "watch:\n");
    for (i = 0; i < cfg->watch_count; i++) {
        fprintf(f, "  - %s\n", cfg->watch_paths[i]);
    }
    fprintf(f, "\nexclude:\n");
    for (i = 0; i < cfg->exclude_count; i++) {
        fprintf(f, "  - %s\n", cfg->exclude_paths[i]);
    }
    fprintf(f, "\nlog_file: %s\n", cfg->log_file[0] ? cfg->log_file : "file_monitor.log");
    fprintf(f, "\nrecursive: %s\n",
            cfg->recursive ? "true" : "false");

    fclose(f);
    return 0;
}

/* Ajoute un chemin a la liste watch du fichier de config,
 * sans dupliquer les entrees existantes. */
static int config_add_watch_path(const char *config_path, const char *new_path)
{
    Config cfg;

    if (config_parse(config_path, &cfg) < 0) {
        fprintf(stderr, "Erreur: impossible de charger %s\n", config_path);
        return -1;
    }

    if (config_has_watch(&cfg, new_path)) {
        fprintf(stderr, "Info: le chemin %s est deja present dans watch.\n", new_path);
    } else {
        if (cfg.watch_count >= MAX_PATHS) {
            fprintf(stderr, "Erreur: nombre maximum de chemins surveilles atteint (%d)\n",
                    MAX_PATHS);
            return -1;
        }
        ft_strlcpy(cfg.watch_paths[cfg.watch_count], new_path, MAX_PATH_LEN);
        cfg.watch_count++;
    }

    if (config_write(config_path, &cfg) < 0)
        return -1;

    return 0;
}

/* Supprime un chemin de la liste watch du fichier de config. */
static int config_remove_watch_path(const char *config_path, const char *path)
{
    Config cfg;
    int i;
    int found = 0;

    if (config_parse(config_path, &cfg) < 0) {
        fprintf(stderr, "Erreur: impossible de charger %s\n", config_path);
        return -1;
    }

    for (i = 0; i < cfg.watch_count; i++) {
        if (strcmp(cfg.watch_paths[i], path) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Info: le chemin %s n'est pas present dans watch.\n", path);
        return 0;
    }

    for (; i < cfg.watch_count - 1; i++) {
        ft_strlcpy(cfg.watch_paths[i], cfg.watch_paths[i + 1], MAX_PATH_LEN);
    }
    if (cfg.watch_count > 0)
        cfg.watch_count--;

    if (config_write(config_path, &cfg) < 0)
        return -1;

    return 0;
}

static void signal_handler(int sig) {
    (void)sig;
    if (g_monitor) {
        g_monitor->stop(g_monitor);
    }
}

static int write_pidfile(const char *path)
{
    /* Ensure directory for pidfile exists (best-effort). */
    mkdir("build", 0755);

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

    /* Gestion des modes : add/remove/start/stop/foreground */
    if (argc > 1 && strcmp(argv[1], "add") == 0) {
        char target[1024];
        const char *id = NULL;

        if (argc < 3) {
            fprintf(stderr, "Usage: %s add <user|pid|chemin> [config.yaml]\n", argv[0]);
            return 1;
        }

        id = argv[2];
        if (argc > 3)
            config_path = argv[3];

        if (resolve_watch_target(id, target, sizeof(target)) < 0)
            return 1;

        printf("Ajout d'une surveillance sur: %s (config: %s)\n", target, config_path);

        if (config_add_watch_path(config_path, target) < 0)
            return 1;

        /* Tente d'installer une regle auditd pour ce chemin.
         * Cette operation n'est effective que sur un Linux natif
         * avec auditd/auditctl disponibles. */
        if (auditd_add_watch(target) != 0) {
            fprintf(stderr, "Attention: echec de l'appel a auditctl pour %s "
                            "(auditd peut etre indisponible, par ex. sous WSL2).\n",
                    target);
        }

        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "remove") == 0) {
        char target[1024];
        const char *id = NULL;

        if (argc < 3) {
            fprintf(stderr, "Usage: %s remove <user|pid|chemin> [config.yaml]\n", argv[0]);
            return 1;
        }

        id = argv[2];
        if (argc > 3)
            config_path = argv[3];

        if (resolve_watch_target(id, target, sizeof(target)) < 0)
            return 1;

        printf("Suppression de la surveillance sur: %s (config: %s)\n", target, config_path);

        if (config_remove_watch_path(config_path, target) < 0)
            return 1;

        /* Tente de supprimer la regle auditd pour ce chemin.
         * Cette operation n'est effective que sur un Linux natif
         * avec auditd/auditctl disponibles. */
        if (auditd_remove_watch(target) != 0) {
            fprintf(stderr, "Attention: echec de l'appel a auditctl (remove) pour %s "
                            "(auditd peut etre indisponible, par ex. sous WSL2).\n",
                    target);
        }

        return 0;
    }

    /* Gestion des modes : stop/start/foreground */
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
