/*
 * AuditMonitorObj.h - Objective-C wrapper for the AuditMonitor C module
 *
 * C module:  auditd.h / auditd.c
 * ObjC class: AuditMonitorObj
 *
 * Strategy (Option A): the original C files are kept untouched.
 * AuditMonitorObj holds a heap-allocated AuditMonitor* (_c) created by
 * auditd_create() and freed by auditd_destroy() in -dealloc.
 *
 * Note: auditd_create() opens a NETLINK_AUDIT socket, which requires root
 * privileges on a native Linux kernel.  On WSL2 the socket is unavailable;
 * -init will return nil in that case and the caller should handle it
 * gracefully (the original C code already has the same behaviour).
 */

#ifndef AUDIT_MONITOR_OBJ_H
#define AUDIT_MONITOR_OBJ_H

#import <Foundation/Foundation.h>
#include <sys/types.h>
#include "auditd.h"

@interface AuditMonitorObj : NSObject
{
@private
    /* Heap-allocated AuditMonitor, owned exclusively by this object.
     * Created in -init via auditd_create(), destroyed in -dealloc via
     * auditd_destroy().  May be NULL if the netlink socket could not be
     * opened (no root / WSL2). */
    AuditMonitor *_c;
}

/* ----------------------------------------------------------------------- *
 *  Designated initializer
 * ----------------------------------------------------------------------- */

/**
 * Maps to: auditd_create()
 * Opens the NETLINK_AUDIT socket and initialises the AuditMonitor.
 * Returns nil if the socket cannot be opened (e.g. insufficient privileges
 * or running on WSL2 where the audit subsystem is absent).
 * Ownership: the receiver owns _c; it is freed in -dealloc.
 */
- (instancetype)init;

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/**
 * Maps to: AuditMonitor.process(self)
 * Drains the audit netlink socket in non-blocking mode and updates the
 * internal path→uid cache.  Call periodically before querying -uidForPath:.
 */
- (void)process;

/**
 * Maps to: AuditMonitor.get_uid_for_path(self, path)
 * Searches the recent audit cache for the most recent uid associated with
 * the given filesystem path.
 * Returns (uid_t)-1 if no matching entry is found.
 */
- (uid_t)uidForPath:(NSString *)path;

/* ----------------------------------------------------------------------- *
 *  Class (static) methods  —  wrap free functions in auditd.h
 * ----------------------------------------------------------------------- */

/**
 * Maps to: auditd_add_watch(path)
 * Installs an auditd watch rule for path via fork+exec of auditctl.
 * Requires auditctl to be installed and root privileges.
 * Returns YES on success, NO otherwise.
 */
+ (BOOL)addWatch:(NSString *)path;

/**
 * Maps to: auditd_uid_to_username(uid, buf, buflen)
 * Resolves a uid to a username via getpwuid_r().
 * Returns the username, or "uid:NNN" / "unknown" if not resolvable.
 * Ownership: caller owns the returned NSString (standard ObjC rules).
 */
+ (NSString *)usernameForUID:(uid_t)uid;

@end

#endif /* AUDIT_MONITOR_OBJ_H */

