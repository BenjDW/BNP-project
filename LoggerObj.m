/*
 * LoggerObj.m - Objective-C wrapper for the Logger C module
 *
 * C module:  logger.h / logger.c
 * ObjC class: LoggerObj
 *
 * Memory model: MRC (Manual Reference Counting) — compatible with GNUstep.
 * _c is owned by this object: logger_create() in -init, logger_destroy() in -dealloc.
 */

#import "include/LoggerObj.h"

@implementation LoggerObj

/* ----------------------------------------------------------------------- *
 *  Initializer / dealloc
 * ----------------------------------------------------------------------- */

/* Maps to: logger_create(log_path) */
- (instancetype)initWithPath:(NSString *)path
{
    self = [super init];
    if (self) {
        _c = logger_create([path fileSystemRepresentation]);
        if (!_c) {
            [self release];
            return nil;
        }
    }
    return self;
}

/* Maps to: logger_destroy(log) */
- (void)dealloc
{
    logger_destroy(_c);
    _c = NULL;
    [super dealloc];
}

/* ----------------------------------------------------------------------- *
 *  Instance methods
 * ----------------------------------------------------------------------- */

/* Maps to: Logger.log_event(self, event_type, path, detail) */
- (void)logEvent:(NSString *)eventType
            path:(NSString *)path
          detail:(NSString *)detail
{
    if (!_c || !_c->log_event) return;
    _c->log_event(_c,
                  [eventType UTF8String],
                  [path UTF8String],
                  detail ? [detail UTF8String] : NULL);
}

@end
