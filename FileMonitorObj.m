/*
 * FileMonitorObj.m - Objective-C wrapper for the FileMonitor C module
 *
 * C module:  file_monitor.h / file_monitor.c
 * ObjC class: FileMonitorObj
 *
 * Memory model: MRC (Manual Reference Counting) — compatible with GNUstep.
 * _c is owned by this object:
 *   file_monitor_create() + file_monitor_init() in -initWithConfig:
 *   file_monitor_destroy()                        in -dealloc
 */

#import "include/FileMonitorObj.h"

@implementation FileMonitorObj

/* ----------------------------------------------------------------------- *
 *  Initializer / dealloc
 * ----------------------------------------------------------------------- */

/* Maps to: file_monitor_create() + file_monitor_init(self, cfg) */
- (instancetype)initWithConfig:(ConfigObj *)config
{
    self = [super init];
    if (self) {
        _c = file_monitor_create();
        if (!_c) {
            [self release];
            return nil;
        }
        if (file_monitor_init(_c, [config cConfig]) != 0) {
            file_monitor_destroy(_c);
            _c = NULL;
            [self release];
            return nil;
        }
    }
    return self;
}

/* Maps to: file_monitor_destroy(self) */
- (void)dealloc
{
    file_monitor_destroy(_c);
    _c = NULL;
    [super dealloc];
}

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/* Maps to: file_monitor_run(self) */
- (void)run
{
    if (_c) file_monitor_run(_c);
}

/* Maps to: file_monitor_stop(self) */
- (void)stop
{
    if (_c) file_monitor_stop(_c);
}

@end
