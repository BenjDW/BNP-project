/*
 * FileMonitorObj.h - Objective-C wrapper for the FileMonitor C module
 *
 * C module:  file_monitor.h / file_monitor.c
 * ObjC class: FileMonitorObj
 *
 * Strategy (Option A): the original C files are kept untouched.
 * FileMonitorObj holds a heap-allocated FileMonitor* (_c) created by
 * file_monitor_create() + file_monitor_init() and freed by
 * file_monitor_destroy() in -dealloc.
 *
 * Lifecycle:
 *   FileMonitorObj *mon = [[FileMonitorObj alloc] initWithConfig:cfg];
 *   // run blocks until -stop is called (from another thread / signal handler)
 *   [mon run];
 *   [mon release];
 */

#ifndef FILE_MONITOR_OBJ_H
#define FILE_MONITOR_OBJ_H

#import <Foundation/Foundation.h>
#include "file_monitor.h"
#import "ConfigObj.h"

@interface FileMonitorObj : NSObject
{
@private
    /* Heap-allocated FileMonitor, owned exclusively by this object.
     * Created by file_monitor_create() + file_monitor_init() in
     * -initWithConfig:, destroyed in -dealloc via file_monitor_destroy(). */
    FileMonitor *_c;
}

/* ----------------------------------------------------------------------- *
 *  Designated initializer
 * ----------------------------------------------------------------------- */

/**
 * Maps to: file_monitor_create() + file_monitor_init(self, cfg)
 * Allocates a FileMonitor, opens the inotify fd, creates the Logger,
 * and registers inotify watches for all paths in config.
 * Returns nil if inotify_init() fails or logger_create() fails.
 * Ownership: the receiver owns _c; it is freed in -dealloc.
 */
- (instancetype)initWithConfig:(ConfigObj *)config;

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/**
 * Maps to: file_monitor_run(self)
 * Enters the inotify event loop.  Blocks the calling thread until -stop
 * is invoked (typically from a signal handler or a separate thread).
 */
- (void)run;

/**
 * Maps to: file_monitor_stop(self)
 * Sets the running flag to 0, causing -run to return on the next
 * iteration.  Safe to call from any thread.
 */
- (void)stop;

@end

#endif /* FILE_MONITOR_OBJ_H */

