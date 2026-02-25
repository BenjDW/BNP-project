/* Requis pour getpwuid_r (POSIX) avec -std=c99 */
#define _POSIX_C_SOURCE 200809L

/*
 * AuditMonitor - intégration avec le sous-système audit Linux
 *
 * Flux : kernel -> netlink(NETLINK_AUDIT) -> parse SYSCALL+PATH -> cache(path,uid)
 *
 * Corrélation : chaque groupe d'enregistrements partage le même numéro de série
 *   msg=audit(timestamp:SERIAL): ...
 * On accumule uid (AUDIT_SYSCALL) et path (AUDIT_PATH) par serial,
 * puis on flush dans le cache à la réception du marqueur EOE (AUDIT_EOE).
 *
 * auditd_add_watch() appelle auditctl pour installer la règle de surveillance.
 * auditd_uid_to_username() utilise getpwuid_r() (bibliothèque standard).
 */
#include "auditd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <linux/audit.h>
#include <pwd.h>
#include "../42/libft/libft.h"

/* --------------------------------------------------------------------------
 * Helpers: parsing des enregistrements texte audit
 * -------------------------------------------------------------------------- */

static unsigned long parse_serial(const char *record)
{
    const char *p = strstr(record, "msg=audit(");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    return strtoul(p + 1, NULL, 10);
}

/* Extrait la valeur d'un champ "key=value" ou "key=\"value\"" */
static int extract_field(const char *record, const char *key,
                         char *out, size_t outlen)
{
    const char *p;
    const char *end;
    size_t len;

    p = strstr(record, key);
    if (!p) return -1;
    p += strlen(key);

    if (*p == '"') {
        p++;
        end = strchr(p, '"');
        if (!end) return -1;
        len = (size_t)(end - p);
    } else {
        end = p;
        while (*end && *end != ' ' && *end != '\n' && *end != '\r')
            end++;
        len = (size_t)(end - p);
    }

    if (len == 0 || len >= outlen) return -1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

/* --------------------------------------------------------------------------
 * Gestion du buffer pending (corrélation SYSCALL <-> PATH par serial)
 * -------------------------------------------------------------------------- */

static AuditPending *find_pending(AuditMonitor *self, unsigned long serial)
{
    int i;
    for (i = 0; i < AUDIT_PENDING_SIZE; i++)
        if (self->pending[i].serial == serial)
            return &self->pending[i];
    return NULL;
}

static AuditPending *get_or_alloc_pending(AuditMonitor *self,
                                          unsigned long serial)
{
    AuditPending *pe;
    int i;

    pe = find_pending(self, serial);
    if (pe) return pe;

    /* Chercher un slot libre */
    for (i = 0; i < AUDIT_PENDING_SIZE; i++) {
        if (!self->pending[i].serial) {
            self->pending[i].serial = serial;
            return &self->pending[i];
        }
    }

    /* Tous occupés : réutiliser le slot 0 (le plus vieux) */
    memset(&self->pending[0], 0, sizeof(self->pending[0]));
    self->pending[0].serial = serial;
    return &self->pending[0];
}

/* --------------------------------------------------------------------------
 * Traitement d'un enregistrement audit
 * -------------------------------------------------------------------------- */

static void parse_audit_record(AuditMonitor *self,
                                const char *data, int type)
{
    unsigned long serial;
    AuditPending *pe;
    char val[512];
    uid_t uid;
    AuditEntry *entry;

    serial = parse_serial(data);
    if (!serial) return;

    if (type == AUDIT_SYSCALL) {
        uid = (uid_t)-1;

        /* Préférer auid (login uid) : identifie l'utilisateur réel */
        if (extract_field(data, " auid=", val, sizeof(val)) == 0) {
            uid_t auid = (uid_t)strtoul(val, NULL, 10);
            /* 4294967295 = -1 = non défini (processus sans session de login) */
            if (auid != (uid_t)4294967295U)
                uid = auid;
        }
        if (uid == (uid_t)-1) {
            if (extract_field(data, " uid=", val, sizeof(val)) == 0)
                uid = (uid_t)strtoul(val, NULL, 10);
        }
        if (uid == (uid_t)-1) return;

        pe = get_or_alloc_pending(self, serial);
        pe->uid = uid;
        pe->has_uid = 1;

    } else if (type == AUDIT_PATH) {
        if (extract_field(data, " name=", val, sizeof(val)) != 0) return;
        if (strcmp(val, "(null)") == 0 || val[0] == '\0') return;

        pe = get_or_alloc_pending(self, serial);
        /* Conserver seulement le premier chemin du groupe (item=0) */
        if (!pe->has_path) {
            ft_strlcpy(pe->path, val, sizeof(pe->path));
            pe->has_path = 1;
        }

    } else if (type == AUDIT_EOE) {
        /* Fin du groupe d'événements : flush dans le cache si complet */
        pe = find_pending(self, serial);
        if (!pe) return;

        if (pe->has_uid && pe->has_path) {
            entry = &self->cache[self->cache_idx % AUDIT_CACHE_SIZE];
            ft_strlcpy(entry->path, pe->path, sizeof(entry->path));
            entry->uid = pe->uid;
            self->cache_idx++;
        }
        memset(pe, 0, sizeof(*pe));
    }
}

/* --------------------------------------------------------------------------
 * Méthodes de AuditMonitor
 * -------------------------------------------------------------------------- */

static void auditd_process_impl(AuditMonitor *self)
{
    char buf[8192];
    ssize_t len;
    int remaining;
    struct nlmsghdr *nlh;
    char *data;
    int dlen;

    while (1) {
        len = recv(self->fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        if (len == 0) break;
        buf[len] = '\0';

        remaining = (int)len;
        nlh = (struct nlmsghdr *)buf;

        while (NLMSG_OK(nlh, (unsigned int)remaining)) {
            if (nlh->nlmsg_type == NLMSG_DONE ||
                nlh->nlmsg_type == NLMSG_ERROR) {
                break;
            }

            dlen = (int)(nlh->nlmsg_len) - NLMSG_HDRLEN;
            if (dlen > 0) {
                data = (char *)NLMSG_DATA(nlh);
                data[dlen - 1] = '\0';
                parse_audit_record(self, data, (int)nlh->nlmsg_type);
            }

            nlh = NLMSG_NEXT(nlh, remaining);
        }
    }
}

static uid_t auditd_get_uid_for_path_impl(AuditMonitor *self,
                                           const char *path)
{
    int total;
    int i;
    int idx;

    total = self->cache_idx < AUDIT_CACHE_SIZE
            ? self->cache_idx
            : AUDIT_CACHE_SIZE;

    /* Parcourir en ordre inverse (entrée la plus récente en premier) */
    for (i = 1; i <= total; i++) {
        idx = ((self->cache_idx - i) % AUDIT_CACHE_SIZE
               + AUDIT_CACHE_SIZE) % AUDIT_CACHE_SIZE;
        if (self->cache[idx].path[0]
            && strcmp(self->cache[idx].path, path) == 0)
            return self->cache[idx].uid;
    }
    return (uid_t)-1;
}

static void auditd_close_impl(AuditMonitor *self)
{
    if (self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
}

/* --------------------------------------------------------------------------
 * API publique
 * -------------------------------------------------------------------------- */

int auditd_uid_to_username(uid_t uid, char *buf, size_t buflen)
{
    struct passwd pw;
    struct passwd *result;
    char tmp[1024];

    if (uid == (uid_t)-1 || uid == (uid_t)4294967295U) {
        ft_strlcpy(buf, "unknown", buflen);
        return -1;
    }

    if (getpwuid_r(uid, &pw, tmp, sizeof(tmp), &result) == 0 && result) {
        ft_strlcpy(buf, result->pw_name, buflen);
        return 0;
    }

    /* Fallback : afficher l'uid brut */
    snprintf(buf, buflen, "uid:%u", (unsigned int)uid);
    return -1;
}

int auditd_add_watch(const char *path)
{
    pid_t pid;
    int status;
    char *argv[9];

    /* auditctl -w <path> -p rw -k file_monitor */
    argv[0] = "auditctl";
    argv[1] = "-w";
    argv[2] = (char *)path;
    argv[3] = "-p";
    argv[4] = "rw";
    argv[5] = "-k";
    argv[6] = "file_monitor";
    argv[7] = NULL;

    pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp("auditctl", argv);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

AuditMonitor *auditd_create(void)
{
    AuditMonitor *am;
    struct sockaddr_nl sa;

    am = (AuditMonitor *)malloc(sizeof(AuditMonitor));
    if (!am) return NULL;
    memset(am, 0, sizeof(AuditMonitor));
    am->fd = -1;

    /* Socket netlink audit - nécessite les droits root */
    am->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_AUDIT);
    if (am->fd < 0) {
        free(am);
        return NULL;
    }

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_pid    = (unsigned int)getpid();
    sa.nl_groups = 1; /* AUDIT_NLGRP_READLOG : réception des événements */

    if (bind(am->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(am->fd);
        free(am);
        return NULL;
    }

    am->process          = auditd_process_impl;
    am->get_uid_for_path = auditd_get_uid_for_path_impl;
    am->close            = auditd_close_impl;

    return am;
}

void auditd_destroy(AuditMonitor *am)
{
    if (am) {
        am->close(am);
        free(am);
    }
}
