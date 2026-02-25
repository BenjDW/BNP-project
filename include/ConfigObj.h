/*
 * ConfigObj.h - Objective-C wrapper for the Config C module
 *
 * C module:  config.h / config.c
 * ObjC class: ConfigObj
 *
 * Strategy (Option A): the original C files are kept untouched.
 * This class wraps the Config struct by value (_c ivar) and delegates
 * all logic to the C functions config_init() / config_parse() / config_print().
 */

#ifndef CONFIG_OBJ_H
#define CONFIG_OBJ_H

#import <Foundation/Foundation.h>
#include "config.h"

@interface ConfigObj : NSObject
{
@private
    /* Holds the underlying C Config struct by value.
     * Not a pointer: Config is a plain (non-opaque) struct. */
    Config _c;
}

/* ----------------------------------------------------------------------- *
 *  Initializers
 * ----------------------------------------------------------------------- */

/**
 * Maps to: config_init()
 * Creates a default config (recursive=1, log_file="file_monitor.log",
 * no watch/exclude paths).
 */
- (instancetype)init;

/**
 * Maps to: config_parse(path, cfg)
 * Parses a YAML config file.  Returns nil if the file cannot be opened
 * or is malformed.
 * Ownership: caller owns the returned object (standard ObjC rules).
 */
- (instancetype)initFromFile:(NSString *)path;

/**
 * Convenience factory.  Maps to: config_parse(path, cfg).
 * Returns nil on parse error.
 */
+ (instancetype)configFromFile:(NSString *)path;

/* ----------------------------------------------------------------------- *
 *  Accessors  (read-only views into the C struct)
 * ----------------------------------------------------------------------- */

/** watch_paths[] as an NSArray of NSStrings. */
- (NSArray *)watchPaths;

/** exclude_paths[] as an NSArray of NSStrings. */
- (NSArray *)excludePaths;

/** log_file as an NSString. */
- (NSString *)logFile;

/** recursive flag. */
- (BOOL)recursive;

/* ----------------------------------------------------------------------- *
 *  Debug
 * ----------------------------------------------------------------------- */

/**
 * Maps to: config_print(cfg)
 * Prints a human-readable summary to stdout.
 */
- (void)print;

/* ----------------------------------------------------------------------- *
 *  C interop
 * ----------------------------------------------------------------------- */

/**
 * Returns a pointer to the embedded C Config struct.
 * Ownership: the pointer is valid as long as this ConfigObj is alive.
 * Callers must NOT free the returned pointer.
 */
- (Config *)cConfig;

@end

#endif /* CONFIG_OBJ_H */

