#ifndef AUDITD_H
#define AUDITD_H

#include <sys/types.h>

/*
 * AuditMonitor - interrogation du sous-système audit Linux (netlink)
 * Permet de retrouver l'utilisateur (uid/username) associé à un événement
 * fichier en corrélant les enregistrements SYSCALL+PATH du noyau.
 */

#define AUDIT_CACHE_SIZE 64

typedef struct {
    char path[512];
    uid_t uid;
} AuditEntry;

/* Enregistrement partiel en attente de corrélation SYSCALL<->PATH */
#define AUDIT_PENDING_SIZE 32

typedef struct {
    unsigned long serial;
    uid_t uid;
    char path[512];
    int has_uid;
    int has_path;
} AuditPending;

typedef struct AuditMonitor AuditMonitor;

struct AuditMonitor {
    int fd;                               /* socket netlink AF_NETLINK/NETLINK_AUDIT */
    AuditEntry cache[AUDIT_CACHE_SIZE];   /* anneau (path, uid) récents */
    int cache_idx;                        /* prochain index d'écriture */
    AuditPending pending[AUDIT_PENDING_SIZE]; /* corrélation en cours */

    /* méthodes */
    void  (*process)(AuditMonitor *self);                              /* vider le socket (non-bloquant) */
    uid_t (*get_uid_for_path)(AuditMonitor *self, const char *path);   /* chercher uid dans le cache */
    void  (*close)(AuditMonitor *self);
};

/* Crée et initialise un AuditMonitor (ouvre le socket netlink audit) */
AuditMonitor *auditd_create(void);

/* Libère les ressources */
void auditd_destroy(AuditMonitor *am);

/* Ajoute une règle de surveillance auditd pour 'path' (fork+exec auditctl) */
int auditd_add_watch(const char *path);

/* Convertit un uid en nom d'utilisateur via getpwuid_r()
 * Retourne 0 si trouvé, -1 sinon (buf contient "uid:NNN" ou "unknown") */
int auditd_uid_to_username(uid_t uid, char *buf, size_t buflen);

#endif /* AUDITD_H */

