/*
 * AuditMonitorObj.m - Objective-C wrapper for the AuditMonitor C module
 *
 * C module:  auditd.h / auditd.c
 * ObjC class: AuditMonitorObj
 *
 * Memory model: MRC (Manual Reference Counting) — compatible with GNUstep.
 * _c is owned by this object: auditd_create() in -init, auditd_destroy() in -dealloc.
 */

#import "include/AuditMonitorObj.h"

@implementation AuditMonitorObj

/* ----------------------------------------------------------------------- *
 *  Initializer / dealloc
 * ----------------------------------------------------------------------- */

/* Maps to: auditd_create() */
- (instancetype)init
{
    self = [super init];
    if (self) {
        _c = auditd_create();
        if (!_c) {
            /* auditd unavailable (no root, or WSL2) — mirror C behaviour */
            [self release];
            return nil;
        }
    }
    return self;
}

/* Maps to: auditd_destroy(am) */
- (void)dealloc
{
    auditd_destroy(_c);
    _c = NULL;
    [super dealloc];
}

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/* Maps to: AuditMonitor.process(self) */
- (void)process
{
    if (_c && _c->process)
        _c->process(_c);
}

/* Maps to: AuditMonitor.get_uid_for_path(self, path) */
- (uid_t)uidForPath:(NSString *)path
{
    if (!_c || !_c->get_uid_for_path)
        return (uid_t)-1;
    return _c->get_uid_for_path(_c, [path fileSystemRepresentation]);
}

/* ----------------------------------------------------------------------- *
 *  Class methods  (wrap free functions)
 * ----------------------------------------------------------------------- */

/* Maps to: auditd_add_watch(path) */
+ (BOOL)addWatch:(NSString *)path
{
    return auditd_add_watch([path fileSystemRepresentation]) == 0 ? YES : NO;
}

/* Maps to: auditd_uid_to_username(uid, buf, buflen) */
+ (NSString *)usernameForUID:(uid_t)uid
{
    char buf[128];
    auditd_uid_to_username(uid, buf, sizeof(buf));
    return [NSString stringWithUTF8String:buf];
}

@end
