/*
 * LoggerObj.h - Objective-C wrapper for the Logger C module
 *
 * C module:  logger.h / logger.c
 * ObjC class: LoggerObj
 *
 * Strategy (Option A): the original C files are kept untouched.
 * LoggerObj holds a heap-allocated Logger* (_c) created by logger_create()
 * and freed by logger_destroy() in -dealloc.
 */

#ifndef LOGGER_OBJ_H
#define LOGGER_OBJ_H

#import <Foundation/Foundation.h>
#include "logger.h"

@interface LoggerObj : NSObject
{
@private
    /* Heap-allocated Logger, owned exclusively by this object.
     * Created in -initWithPath:, destroyed in -dealloc via logger_destroy(). */
    Logger *_c;
}

/* ----------------------------------------------------------------------- *
 *  Designated initializer
 * ----------------------------------------------------------------------- */

/**
 * Maps to: logger_create(log_path)
 * Opens (or creates) the log file in append mode.
 * Returns nil if the file cannot be opened.
 * Ownership: the receiver owns _c; it is freed automatically in -dealloc.
 */
- (instancetype)initWithPath:(NSString *)path;

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/**
 * Maps to: Logger.log_event(self, event_type, path, detail)
 * Writes a timestamped entry to the log file.
 * detail may be nil (logged as empty string).
 */
- (void)logEvent:(NSString *)eventType
            path:(NSString *)path
          detail:(NSString *)detail;

@end

#endif /* LOGGER_OBJ_H */

