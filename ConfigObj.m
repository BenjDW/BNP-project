/*
 * ConfigObj.m - Objective-C wrapper for the Config C module
 *
 * C module:  config.h / config.c
 * ObjC class: ConfigObj
 *
 * Memory model: MRC (Manual Reference Counting) — compatible with GNUstep.
 * Config is embedded by value (_c), so no heap allocation or free needed
 * beyond the normal ObjC object lifecycle.
 */

#import "include/ConfigObj.h"
#include <string.h>

@implementation ConfigObj

/* ----------------------------------------------------------------------- *
 *  Initializers
 * ----------------------------------------------------------------------- */

/* Maps to: config_init() */
- (instancetype)init
{
    self = [super init];
    if (self) {
        config_init(&_c);
    }
    return self;
}

/* Maps to: config_parse(path, cfg) */
- (instancetype)initFromFile:(NSString *)path
{
    self = [super init];
    if (self) {
        if (config_parse([path fileSystemRepresentation], &_c) != 0) {
            [self release];
            return nil;
        }
    }
    return self;
}

+ (instancetype)configFromFile:(NSString *)path
{
    /* Maps to: config_parse() */
    return [[[self alloc] initFromFile:path] autorelease];
}

/* ----------------------------------------------------------------------- *
 *  Accessors
 * ----------------------------------------------------------------------- */

/* Reads from: Config.watch_paths[] / Config.watch_count */
- (NSArray *)watchPaths
{
    NSMutableArray *arr = [NSMutableArray arrayWithCapacity:(NSUInteger)_c.watch_count];
    for (int i = 0; i < _c.watch_count; i++)
        [arr addObject:[NSString stringWithUTF8String:_c.watch_paths[i]]];
    return arr;
}

/* Reads from: Config.exclude_paths[] / Config.exclude_count */
- (NSArray *)excludePaths
{
    NSMutableArray *arr = [NSMutableArray arrayWithCapacity:(NSUInteger)_c.exclude_count];
    for (int i = 0; i < _c.exclude_count; i++)
        [arr addObject:[NSString stringWithUTF8String:_c.exclude_paths[i]]];
    return arr;
}

/* Reads from: Config.log_file */
- (NSString *)logFile
{
    return [NSString stringWithUTF8String:_c.log_file];
}

/* Reads from: Config.recursive */
- (BOOL)recursive
{
    return _c.recursive ? YES : NO;
}

/* ----------------------------------------------------------------------- *
 *  Debug
 * ----------------------------------------------------------------------- */

/* Maps to: config_print(cfg) */
- (void)print
{
    config_print(&_c);
}

/* ----------------------------------------------------------------------- *
 *  C interop
 * ----------------------------------------------------------------------- */

/* Returns pointer to the embedded struct.
 * Caller must NOT free — lifetime is tied to this object. */
- (Config *)cConfig
{
    return &_c;
}

@end
