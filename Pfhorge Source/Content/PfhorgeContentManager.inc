// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// CONTENT-1A / VM-SETTINGS-1A foundation. Included once by LEDelegate.m so the
// phase can land without rewriting the historical Xcode project file.

#import <AppKit/AppKit.h>
#import <CommonCrypto/CommonDigest.h>
#import <objc/message.h>
#import <Metal/Metal.h>
#include <math.h>

#import "PfhorgeVisualModeSettings.h"

static NSString *const PfhorgeContentManifestSchemaVersion = @"2";
static NSString *const PfhorgeContentSelectedProfileKey = @"selectedProfile";
static NSString *const PfhorgeContentActiveGamePreference = @"PfhorgeContentActiveGame";
static NSString *const PfhorgeContentActiveEnhancedPathPreference = @"PfhorgeContentActiveEnhancedPath";
static NSString *const PfhorgeContentSelectionDidChangeNotification = @"PfhorgeContentSelectionDidChangeNotification";
static NSString *const PfhorgeLegacyShapesPathPreference = @"VMShapesPath";
static const NSUInteger PfhorgeContentMaximumEntryCount = 50000U;
static const unsigned long long PfhorgeContentMaximumExpandedBytes =
    16ULL * 1024ULL * 1024ULL * 1024ULL;

static NSArray<NSString *> *PfhorgeContentGameKeys(void)
{
    return @[@"marathon", @"marathon2", @"infinity", @"custom"];
}

static NSString *PfhorgeContentGameDisplayName(NSString *gameKey)
{
    if ([gameKey isEqualToString:@"marathon"]) {
        return @"Classic Marathon";
    }
    if ([gameKey isEqualToString:@"marathon2"]) {
        return @"Marathon 2: Durandal";
    }
    if ([gameKey isEqualToString:@"infinity"]) {
        return @"Marathon Infinity";
    }
    return @"Custom Scenarios";
}

static NSString *PfhorgeOfficialAssetPrefix(NSString *gameKey)
{
    if ([gameKey isEqualToString:@"marathon"]) {
        return @"Marathon-";
    }
    if ([gameKey isEqualToString:@"marathon2"]) {
        return @"Marathon2-";
    }
    if ([gameKey isEqualToString:@"infinity"]) {
        return @"MarathonInfinity-";
    }
    return @"";
}


static NSString *PfhorgeExpectedOfficialAssetName(
    NSString *gameKey,
    NSString *releaseTag)
{
    NSString *version = [releaseTag hasPrefix:@"release-"]
        ? [releaseTag substringFromIndex:@"release-".length]
        : releaseTag;
    if (version.length == 0) {
        return @"";
    }
    if ([gameKey isEqualToString:@"marathon"]) {
        return [NSString stringWithFormat:@"Marathon-%@-Data.zip", version];
    }
    if ([gameKey isEqualToString:@"marathon2"]) {
        return [NSString stringWithFormat:@"Marathon2-%@-Data.zip", version];
    }
    if ([gameKey isEqualToString:@"infinity"]) {
        return [NSString stringWithFormat:@"MarathonInfinity-%@-Data.zip", version];
    }
    return @"";
}

static NSDictionary *PfhorgeEnhancedRecipeInfo(NSString *gameKey)
{
    if ([gameKey isEqualToString:@"marathon"]) {
        return @{
            @"id": @"m1-best-available",
            @"name": @"Marathon 1 Best-Available HD/3D",
            @"directory": @"marathon",
            @"output": @"Marathon-1-Best-Available-HD-Visual-Pack.zip",
            @"summary": @"TTEP environment textures, xBR monsters, HD weapons, 3D scenery, and an 8192-pixel landscape. This is a best-available collection, not a claim of 100% replacement coverage.",
            @"components": @[
                @"TTEP M1 Walls 1024 v7.0",
                @"xBR Monsters for M1 v1.3",
                @"Tacticus M1 Weapons Redux v1.1",
                @"3D Scenery for M1 v1.2",
                @"Tau Ceti Landscape 8192 v3.0",
            ],
        };
    }
    if ([gameKey isEqualToString:@"marathon2"]) {
        return @{
            @"id": @"m2-cfp-complete",
            @"name": @"Marathon 2 CFP Complete HD",
            @"directory": @"marathon2",
            @"output": @"Marathon-2-CFP-Complete-HD.zip",
            @"summary": @"A curated CFP profile covering environment textures, weapons, monsters, and scenery. Original Shapes remains the fallback for anything not replaced.",
            @"components": @[
                @"CFP Walls for Marathon 2",
                @"CFP Weapons for Marathon 2",
                @"CFP Monsters for Marathon 2",
                @"CFP Scenery for Marathon 2",
            ],
        };
    }
    if ([gameKey isEqualToString:@"infinity"]) {
        return @{
            @"id": @"infinity-cfp-complete",
            @"name": @"Marathon Infinity CFP Complete HD",
            @"directory": @"infinity",
            @"output": @"Marathon-Infinity-CFP-Complete-HD.zip",
            @"summary": @"A curated CFP profile for Infinity environment textures, weapons, monsters, and scenery. Original Shapes remains the canonical fallback.",
            @"components": @[
                @"CFP Walls for Marathon Infinity",
                @"CFP Weapons for Marathon Infinity",
                @"CFP Monsters for Marathon Infinity",
                @"CFP Scenery for Marathon Infinity",
            ],
        };
    }
    return nil;
}

static NSURL *PfhorgeApplicationSupportURL(void)
{
    NSArray<NSURL *> *urls = [[NSFileManager defaultManager]
        URLsForDirectory:NSApplicationSupportDirectory
               inDomains:NSUserDomainMask];
    NSURL *base = urls.firstObject;
    if (base == nil) {
        base = [NSURL fileURLWithPath:[@"~/Library/Application Support"
            stringByExpandingTildeInPath]
                         isDirectory:YES];
    }
    return [base URLByAppendingPathComponent:@"Pfhorge" isDirectory:YES];
}

static NSURL *PfhorgeContentRootURL(void)
{
    return [PfhorgeApplicationSupportURL()
        URLByAppendingPathComponent:@"Content" isDirectory:YES];
}

static BOOL PfhorgeEnsureDirectory(NSURL *url, NSError **error)
{
    return [[NSFileManager defaultManager]
        createDirectoryAtURL:url
  withIntermediateDirectories:YES
                   attributes:nil
                        error:error];
}

static NSURL *PfhorgeContentSubdirectory(NSString *name)
{
    NSURL *url = [PfhorgeContentRootURL()
        URLByAppendingPathComponent:name isDirectory:YES];
    PfhorgeEnsureDirectory(url, nil);
    return url;
}

static BOOL PfhorgeURLIsInsideDirectory(NSURL *candidate, NSURL *directory)
{
    NSString *candidatePath = [[candidate URLByResolvingSymlinksInPath].path stringByStandardizingPath];
    NSString *directoryPath = [[directory URLByResolvingSymlinksInPath].path stringByStandardizingPath];
    if (candidatePath.length == 0 || directoryPath.length == 0) {
        return NO;
    }
    if ([candidatePath isEqualToString:directoryPath]) {
        return YES;
    }
    NSString *prefix = [directoryPath stringByAppendingString:@"/"];
    return [candidatePath hasPrefix:prefix];
}

static NSURL *PfhorgeManifestURL(NSString *gameKey)
{
    return [[PfhorgeContentSubdirectory(@"Manifests")
        URLByAppendingPathComponent:gameKey]
        URLByAppendingPathExtension:@"json"];
}

static NSDictionary *PfhorgeReadManifest(NSString *gameKey)
{
    NSData *data = [NSData dataWithContentsOfURL:PfhorgeManifestURL(gameKey)];
    if (data == nil) {
        return nil;
    }
    id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [object isKindOfClass:NSDictionary.class] ? object : nil;
}

static BOOL PfhorgeWriteManifest(
    NSString *gameKey,
    NSDictionary *manifest,
    NSError **error)
{
    NSURL *directory = PfhorgeContentSubdirectory(@"Manifests");
    if (!PfhorgeEnsureDirectory(directory, error)) {
        return NO;
    }

    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest
                                                   options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
                                                     error:error];
    if (data == nil) {
        return NO;
    }
    return [data writeToURL:PfhorgeManifestURL(gameKey)
                    options:NSDataWritingAtomic
                      error:error];
}

static NSString *PfhorgeSHA256ForFile(NSURL *url, NSError **error)
{
    NSInputStream *stream = [NSInputStream inputStreamWithURL:url];
    [stream open];

    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);

    // NSURLSession delegate worker threads have comparatively small stacks.
    // A one-megabyte automatic array overflowed the worker stack on arm64.
    // Keep the streaming buffer on the heap and use a modest chunk size.
    NSMutableData *bufferStorage = [NSMutableData dataWithLength:64U * 1024U];
    uint8_t *buffer = (uint8_t *)bufferStorage.mutableBytes;
    const NSUInteger bufferLength = bufferStorage.length;

    while (YES) {
        NSInteger count = [stream read:buffer maxLength:bufferLength];
        if (count < 0) {
            if (error != NULL) {
                *error = stream.streamError;
            }
            [stream close];
            return nil;
        }
        if (count == 0) {
            break;
        }
        CC_SHA256_Update(&context, buffer, (CC_LONG)count);
    }

    [stream close];
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &context);

    NSMutableString *result = [NSMutableString stringWithCapacity:64];
    for (NSUInteger index = 0; index < CC_SHA256_DIGEST_LENGTH; ++index) {
        [result appendFormat:@"%02x", digest[index]];
    }
    return result;
}

static BOOL PfhorgeLooksLikeShapesFile(NSURL *url, NSNumber *fileSize)
{
    if (fileSize.unsignedLongLongValue < 1024ULL) {
        return NO;
    }

    NSString *name = url.lastPathComponent.lowercaseString;
    NSString *extension = url.pathExtension.lowercaseString;
    NSSet<NSString *> *extensions = [NSSet setWithArray:@[
        @"shpa", @"shp2", @"shp", @"shapes", @"shp∞"
    ]];

    if ([extensions containsObject:extension]) {
        return YES;
    }
    if ([name containsString:@"shapes"] &&
        ![extension isEqualToString:@"txt"] &&
        ![extension isEqualToString:@"md"] &&
        ![extension isEqualToString:@"html"] &&
        ![extension isEqualToString:@"xml"] &&
        ![extension isEqualToString:@"mml"]) {
        return YES;
    }
    return NO;
}

static NSDictionary *PfhorgeScanContentURL(NSURL *rootURL, NSError **error)
{
    NSFileManager *manager = [NSFileManager defaultManager];
    NSArray *keys = @[
        NSURLIsDirectoryKey,
        NSURLIsRegularFileKey,
        NSURLIsSymbolicLinkKey,
        NSURLFileSizeKey,
        NSURLIsHiddenKey,
    ];

    NSDirectoryEnumerator<NSURL *> *enumerator =
        [manager enumeratorAtURL:rootURL
     includingPropertiesForKeys:keys
                        options:0
                   errorHandler:^BOOL(NSURL *url, NSError *scanError) {
        NSLog(@"Content scan skipped %@: %@", url.path, scanError);
        return YES;
    }];

    NSMutableArray<NSString *> *shapes = [NSMutableArray array];
    NSMutableArray<NSString *> *mml = [NSMutableArray array];
    NSMutableArray<NSString *> *textures = [NSMutableArray array];
    NSMutableArray<NSString *> *plugins = [NSMutableArray array];
    NSMutableArray<NSString *> *builderRecipes = [NSMutableArray array];
    NSMutableArray<NSDictionary *> *shapeRecords = [NSMutableArray array];
    NSMutableArray<NSString *> *rightsDocuments = [NSMutableArray array];
    NSUInteger visited = 0;
    unsigned long long totalBytes = 0ULL;
    BOOL truncated = NO;

    NSSet<NSString *> *textureExtensions = [NSSet setWithArray:@[
        @"png", @"jpg", @"jpeg", @"dds", @"tga", @"bmp", @"tif", @"tiff"
    ]];

    for (NSURL *url in enumerator) {
        ++visited;
        if (visited > PfhorgeContentMaximumEntryCount) {
            truncated = YES;
            break;
        }

        NSDictionary *values = [url resourceValuesForKeys:keys error:nil];
        if ([values[NSURLIsSymbolicLinkKey] boolValue]) {
            if ([values[NSURLIsDirectoryKey] boolValue]) {
                [enumerator skipDescendants];
            }
            continue;
        }
        if ([values[NSURLIsHiddenKey] boolValue]) {
            if ([values[NSURLIsDirectoryKey] boolValue]) {
                [enumerator skipDescendants];
            }
            continue;
        }

        if ([values[NSURLIsDirectoryKey] boolValue]) {
            NSString *lower = url.lastPathComponent.lowercaseString;
            if ([lower isEqualToString:@"plugins"] ||
                [lower hasSuffix:@".plugin"] ||
                [lower containsString:@"texture pack"]) {
                if (plugins.count < 128U) {
                    [plugins addObject:url.path];
                }
            }
            continue;
        }

        if (![values[NSURLIsRegularFileKey] boolValue]) {
            continue;
        }

        NSNumber *size = values[NSURLFileSizeKey];
        totalBytes += size.unsignedLongLongValue;
        if (totalBytes > PfhorgeContentMaximumExpandedBytes) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1009
                                         userInfo:@{NSLocalizedDescriptionKey:
                    @"The selected content exceeds Pfhorge's 16 GiB safety limit."}];
            }
            return nil;
        }

        NSString *lowerName = url.lastPathComponent.lowercaseString;
        if ([lowerName containsString:@"license"] ||
            [lowerName containsString:@"licence"] ||
            [lowerName containsString:@"credits"] ||
            [lowerName containsString:@"copying"] ||
            [lowerName containsString:@"authors"]) {
            if (rightsDocuments.count < 64U) {
                [rightsDocuments addObject:url.path];
            }
        }

        if ([lowerName isEqualToString:@"build_pack.py"]) {
            if (builderRecipes.count < 32U) {
                [builderRecipes addObject:url.path];
            }
            continue;
        }

        if (PfhorgeLooksLikeShapesFile(url, size)) {
            if (shapes.count < 128U) {
                [shapes addObject:url.path];
            }
            if (shapeRecords.count < 32U) {
                NSError *digestError = nil;
                NSString *digest = PfhorgeSHA256ForFile(url, &digestError);
                [shapeRecords addObject:@{
                    @"path": url.path ?: @"",
                    @"size": size ?: @(0),
                    @"sha256": digest ?: @"",
                    @"hashError": digestError.localizedDescription ?: @"",
                }];
            }
            continue;
        }

        NSString *extension = url.pathExtension.lowercaseString;
        if ([extension isEqualToString:@"mml"] ||
            ([extension isEqualToString:@"xml"] &&
             [url.lastPathComponent.lowercaseString containsString:@"mml"])) {
            if (mml.count < 256U) {
                [mml addObject:url.path];
            }
        } else if ([textureExtensions containsObject:extension]) {
            if (textures.count < 512U) {
                [textures addObject:url.path];
            }
        }
    }

    if (truncated) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1019
                                     userInfo:@{NSLocalizedDescriptionKey:
                @"The selected content contains more than 50,000 entries and was not registered."}];
        }
        return nil;
    }

    if (shapes.count == 0 && textures.count == 0 && mml.count == 0 && builderRecipes.count == 0) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1001
                                     userInfo:@{
                NSLocalizedDescriptionKey:
                    @"No Shapes files, MML mappings, external texture images, or recognized builder recipes were found in the selected location."
            }];
        }
        return nil;
    }

    return @{
        @"rootPath": rootURL.path ?: @"",
        @"shapes": shapes,
        @"mml": mml,
        @"textures": textures,
        @"plugins": plugins,
        @"builderRecipes": builderRecipes,
        @"shapeRecords": shapeRecords,
        @"rightsDocuments": rightsDocuments,
        @"visitedFileCount": @(visited),
        @"totalFileBytes": @(totalBytes),
        @"truncated": @(truncated),
    };
}


static NSArray<NSString *> *PfhorgeAllShapesPathsInManifest(NSDictionary *manifest)
{
    NSMutableArray<NSString *> *paths = [NSMutableArray array];
    NSDictionary *scan = manifest[@"scan"];
    for (NSString *path in (scan[@"shapes"] ?: @[])) {
        if (path.length > 0) [paths addObject:path];
    }
    for (NSDictionary *profile in (manifest[@"textureProfiles"] ?: @[])) {
        NSDictionary *profileScan = profile[@"scan"];
        for (NSString *path in (profileScan[@"shapes"] ?: @[])) {
            if (path.length > 0) [paths addObject:path];
        }
    }
    return paths;
}

static NSString *PfhorgePreferredShapesPath(
    NSString *gameKey,
    NSDictionary *manifest)
{
    NSArray<NSString *> *paths = PfhorgeAllShapesPathsInManifest(manifest);
    NSString *best = nil;
    NSInteger bestScore = NSIntegerMax;
    NSFileManager *manager = [NSFileManager defaultManager];
    for (NSString *path in paths) {
        BOOL directory = NO;
        if (![manager fileExistsAtPath:path isDirectory:&directory] || directory) {
            continue;
        }
        NSString *name = path.lastPathComponent.lowercaseString;
        NSString *lowerPath = path.lowercaseString;
        NSInteger score = (NSInteger)MIN(path.length, 500U);
        if ([name isEqualToString:@"shapes.shpa"]) score -= 2000;
        if ([name isEqualToString:@"m1a1 shapes.shpa"]) score -= 2400;
        if ([gameKey isEqualToString:@"marathon"] && [name containsString:@"m1a1"]) score -= 500;
        if ([lowerPath containsString:@"/plugins/"] || [lowerPath containsString:@".plugin/"]) score += 3000;
        if ([lowerPath containsString:@"network"] || [lowerPath containsString:@"netpack"]) score += 1500;
        if (score < bestScore) {
            bestScore = score;
            best = path;
        }
    }
    return best;
}

static NSDictionary *PfhorgeSelectedEnhancedProfile(NSDictionary *manifest)
{
    NSArray<NSDictionary *> *profiles = manifest[@"textureProfiles"] ?: @[];
    NSString *selectedID = manifest[@"selectedEnhancedProfileID"];
    if (selectedID.length > 0) {
        for (NSDictionary *profile in profiles) {
            if ([profile[@"id"] isEqualToString:selectedID]) return profile;
        }
    }
    for (NSDictionary *profile in profiles.reverseObjectEnumerator) {
        if (![profile[@"kind"] isEqualToString:@"builderRecipe"]) return profile;
    }
    return nil;
}

static void PfhorgeReloadLegacyTextureConsumers(void)
{
    dispatch_block_t reload = ^{
        Class repositoryClass = NSClassFromString(@"TextureRepository");
        if (repositoryClass == Nil) {
            repositoryClass = NSClassFromString(@"Pfhorge.TextureRepository");
        }
        SEL sharedSelector = NSSelectorFromString(@"sharedTextureRepository");
        SEL loadSelector = NSSelectorFromString(@"loadAllTextures");
        if (repositoryClass != Nil &&
            [repositoryClass respondsToSelector:sharedSelector]) {
            id repository = ((id (*)(id, SEL))objc_msgSend)(
                (id)repositoryClass,
                sharedSelector);
            if (repository != nil && [repository respondsToSelector:loadSelector]) {
                ((void (*)(id, SEL))objc_msgSend)(repository, loadSelector);
            }
        }

        id delegate = NSApp.delegate;
        SEL updateSelector = NSSelectorFromString(@"updateTextureMenuContents");
        if (delegate != nil && [delegate respondsToSelector:updateSelector]) {
            ((void (*)(id, SEL))objc_msgSend)(delegate, updateSelector);
        }
        [[NSNotificationCenter defaultCenter]
            postNotificationName:PfhorgeContentSelectionDidChangeNotification
                          object:nil];
    };
    if ([NSThread isMainThread]) reload();
    else dispatch_async(dispatch_get_main_queue(), reload);
}

static BOOL PfhorgeActivateContentProfile(
    NSString *gameKey,
    NSMutableDictionary *manifest,
    NSError **error)
{
    NSString *profile = manifest[PfhorgeContentSelectedProfileKey] ?: @"Original";
    if ([profile isEqualToString:@"Distribution Default"]) {
        profile = @"Original";
        manifest[PfhorgeContentSelectedProfileKey] = profile;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:gameKey forKey:PfhorgeContentActiveGamePreference];

    if ([profile isEqualToString:@"Untextured Diagnostic"]) {
        [defaults setBool:YES forKey:PfhorgeVMUntexturedDiagnosticPreference];
        [defaults removeObjectForKey:PfhorgeContentActiveEnhancedPathPreference];
        PfhorgeReloadLegacyTextureConsumers();
        return YES;
    }

    NSString *shapesPath = PfhorgePreferredShapesPath(gameKey, manifest);
    if (shapesPath.length == 0) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1040
                                     userInfo:@{NSLocalizedDescriptionKey:
                @"The selected appearance requires an original Shapes file. Install or choose the original game distribution first."}];
        }
        return NO;
    }

    [defaults setURL:[NSURL fileURLWithPath:shapesPath]
              forKey:PfhorgeLegacyShapesPathPreference];
    [defaults setBool:NO forKey:PfhorgeVMUntexturedDiagnosticPreference];
    manifest[@"activeShapesPath"] = shapesPath;

    if ([profile isEqualToString:@"Enhanced"]) {
        NSDictionary *enhanced = PfhorgeSelectedEnhancedProfile(manifest);
        NSString *path = enhanced[@"path"];
        if (enhanced == nil || path.length == 0) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1041
                                         userInfo:@{NSLocalizedDescriptionKey:
                    @"No enhanced texture profile is installed for this game. Install the recommended enhanced pack or import a compatible pack first."}];
            }
            return NO;
        }
        manifest[@"selectedEnhancedProfileID"] = enhanced[@"id"] ?: @"";
        [defaults setObject:path forKey:PfhorgeContentActiveEnhancedPathPreference];
    } else {
        [defaults removeObjectForKey:PfhorgeContentActiveEnhancedPathPreference];
    }

    PfhorgeReloadLegacyTextureConsumers();
    return YES;
}

static void PfhorgeRestoreActiveContentSelection(void)
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *gameKey = [defaults stringForKey:PfhorgeContentActiveGamePreference];
    if (gameKey.length == 0) return;
    NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
    if (manifest == nil) return;
    NSError *error = nil;
    if (PfhorgeActivateContentProfile(gameKey, manifest, &error)) {
        PfhorgeWriteManifest(gameKey, manifest, nil);
    } else {
        NSLog(@"Pfhorge content activation failed: %@", error.localizedDescription);
    }
}

static BOOL PfhorgeAuditManagedTree(NSURL *rootURL, NSError **error)
{
    NSFileManager *manager = [NSFileManager defaultManager];
    NSArray *keys = @[
        NSURLIsDirectoryKey,
        NSURLIsRegularFileKey,
        NSURLIsSymbolicLinkKey,
        NSURLFileSizeKey,
    ];
    __block NSError *enumerationError = nil;
    NSDirectoryEnumerator<NSURL *> *enumerator =
        [manager enumeratorAtURL:rootURL
     includingPropertiesForKeys:keys
                        options:0
                   errorHandler:^BOOL(NSURL *url, NSError *scanError) {
        (void)url;
        if (enumerationError == nil) {
            enumerationError = scanError;
        }
        return NO;
    }];

    NSUInteger entries = 0U;
    unsigned long long bytes = 0ULL;
    for (NSURL *url in enumerator) {
        if (++entries > PfhorgeContentMaximumEntryCount) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1014
                                         userInfo:@{NSLocalizedDescriptionKey:
                    @"The imported content contains too many files."}];
            }
            return NO;
        }
        NSDictionary *values = [url resourceValuesForKeys:keys error:error];
        if (values == nil) {
            return NO;
        }
        if ([values[NSURLIsSymbolicLinkKey] boolValue]) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1015
                                         userInfo:@{NSLocalizedDescriptionKey:
                    @"Managed content may not contain symbolic links."}];
            }
            return NO;
        }
        if ([values[NSURLIsRegularFileKey] boolValue]) {
            bytes += [values[NSURLFileSizeKey] unsignedLongLongValue];
            if (bytes > PfhorgeContentMaximumExpandedBytes) {
                if (error != NULL) {
                    *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                                 code:1016
                                             userInfo:@{NSLocalizedDescriptionKey:
                        @"The imported content exceeds Pfhorge's 16 GiB safety limit."}];
                }
                return NO;
            }
        }
    }
    if (enumerationError != nil) {
        if (error != NULL) {
            *error = enumerationError;
        }
        return NO;
    }
    return YES;
}

static NSString *PfhorgeRunTask(
    NSString *launchPath,
    NSArray<NSString *> *arguments,
    int *terminationStatus,
    NSError **error)
{
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = launchPath;
    task.arguments = arguments;
    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    task.standardError = pipe;

    @try {
        [task launch];
    } @catch (NSException *exception) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1002
                                     userInfo:@{NSLocalizedDescriptionKey: exception.reason ?: @"Could not launch archive utility."}];
        }
        return nil;
    }

    // Drain the pipe while the process is running so a verbose utility cannot
    // block on a full pipe. The termination status is only valid after exit.
    NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
    [task waitUntilExit];
    if (terminationStatus != NULL) {
        *terminationStatus = task.terminationStatus;
    }
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
}

static BOOL PfhorgeValidateZipArchive(NSURL *archiveURL, NSError **error)
{
    int status = 0;
    NSString *listing = PfhorgeRunTask(
        @"/usr/bin/unzip",
        @[@"-Z1", archiveURL.path],
        &status,
        error);
    if (listing == nil || status != 0) {
        if (error != NULL && *error == nil) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1003
                                     userInfo:@{NSLocalizedDescriptionKey: @"The downloaded ZIP archive could not be inspected."}];
        }
        return NO;
    }

    NSArray<NSString *> *entries = [listing componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet];
    if (entries.count > PfhorgeContentMaximumEntryCount) {
        if (error != NULL) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1004
                                     userInfo:@{NSLocalizedDescriptionKey: @"The archive contains too many entries."}];
        }
        return NO;
    }

    NSMutableSet<NSString *> *caseFoldedEntries = [NSMutableSet set];
    NSString *nulCharacter = [NSString stringWithFormat:@"%C", (unichar)0];
    for (NSString *entry in entries) {
        if (entry.length == 0) {
            continue;
        }
        if ([entry hasPrefix:@"/"] || [entry containsString:@"\\"] ||
            [entry containsString:nulCharacter]) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1005
                                         userInfo:@{NSLocalizedDescriptionKey: @"The archive contains an unsafe absolute or malformed path."}];
            }
            return NO;
        }
        NSArray<NSString *> *components = [entry pathComponents];
        if ([components containsObject:@".."]) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1006
                                         userInfo:@{NSLocalizedDescriptionKey: @"The archive contains a path-traversal entry."}];
            }
            return NO;
        }
        NSString *folded = [[entry stringByStandardizingPath]
            precomposedStringWithCanonicalMapping].lowercaseString;
        if ([caseFoldedEntries containsObject:folded]) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1017
                                         userInfo:@{NSLocalizedDescriptionKey:
                    @"The archive contains duplicate or case-colliding paths."}];
            }
            return NO;
        }
        [caseFoldedEntries addObject:folded];
    }

    NSString *longListing = PfhorgeRunTask(
        @"/usr/bin/zipinfo",
        @[@"-l", archiveURL.path],
        &status,
        error);
    if (longListing == nil || status != 0) {
        return NO;
    }
    unsigned long long expandedBytes = 0ULL;
    NSCharacterSet *whitespace = NSCharacterSet.whitespaceCharacterSet;
    for (NSString *line in [longListing componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
        if ([line hasPrefix:@"l"] || [line hasPrefix:@"h"]) {
            if (error != NULL) {
                *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                             code:1007
                                         userInfo:@{NSLocalizedDescriptionKey: @"The archive contains symbolic or hard links, which Pfhorge does not install."}];
            }
            return NO;
        }
        if (![line hasPrefix:@"-"] && ![line hasPrefix:@"d"]) {
            continue;
        }
        NSMutableArray<NSString *> *fields = [NSMutableArray array];
        for (NSString *field in [line componentsSeparatedByCharactersInSet:whitespace]) {
            if (field.length > 0) {
                [fields addObject:field];
            }
        }
        if (fields.count >= 4U) {
            expandedBytes += [fields[3] longLongValue];
            if (expandedBytes > PfhorgeContentMaximumExpandedBytes) {
                if (error != NULL) {
                    *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                                 code:1018
                                             userInfo:@{NSLocalizedDescriptionKey:
                        @"The archive expands beyond Pfhorge's 16 GiB safety limit."}];
                }
                return NO;
            }
        }
    }
    return YES;
}

static BOOL PfhorgeExtractZipArchive(
    NSURL *archiveURL,
    NSURL *destinationURL,
    NSError **error)
{
    if (!PfhorgeValidateZipArchive(archiveURL, error)) {
        return NO;
    }
    if (!PfhorgeEnsureDirectory(destinationURL, error)) {
        return NO;
    }

    int status = 0;
    NSString *output = PfhorgeRunTask(
        @"/usr/bin/ditto",
        @[@"-x", @"-k", @"--sequesterRsrc", @"--rsrc",
          archiveURL.path, destinationURL.path],
        &status,
        error);
    if (status != 0) {
        if (error != NULL && *error == nil) {
            *error = [NSError errorWithDomain:@"org.pfhorge.content"
                                         code:1008
                                     userInfo:@{NSLocalizedDescriptionKey:
                         output.length > 0 ? output : @"The ZIP archive could not be extracted."}];
        }
        return NO;
    }
    return PfhorgeAuditManagedTree(destinationURL, error);
}

static NSString *PfhorgeHumanFileSize(unsigned long long size)
{
    return [NSByteCountFormatter stringFromByteCount:(long long)size
                                          countStyle:NSByteCountFormatterCountStyleFile];
}

static NSTextField *PfhorgeLabel(NSRect frame, NSString *text, CGFloat size, BOOL bold)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    field.editable = NO;
    field.selectable = YES;
    field.bezeled = NO;
    field.drawsBackground = NO;
    field.stringValue = text ?: @"";
    field.font = bold
        ? [NSFont boldSystemFontOfSize:size]
        : [NSFont systemFontOfSize:size];
    return field;
}

@class PfhorgeContentManagerController;

@interface PfhorgeVisualModeSettingsController : NSWindowController
- (NSView *)buildKeyBindingsView:(NSRect)frame;
- (NSView *)buildMouseLookView:(NSRect)frame;
- (NSView *)buildCameraView:(NSRect)frame;
- (NSView *)buildDisplayView:(NSRect)frame;
- (NSView *)buildContentView:(NSRect)frame;
- (NSView *)buildDiagnosticsView:(NSRect)frame;
@end

@interface PfhorgeContentManagerController : NSWindowController
    <NSTableViewDataSource, NSTableViewDelegate, NSURLSessionDownloadDelegate>
- (void)beginRecommendedEnhancedBuildForGame:(NSString *)gameKey
                                      recipe:(NSDictionary *)recipe
                                      script:(NSURL *)script
                                      python:(NSString *)python;
- (void)finishRecommendedEnhancedBuild:(NSTask *)task
                                 recipe:(NSDictionary *)recipe;
@end

@implementation PfhorgeContentManagerController {
    NSTableView *_gameTable;
    NSTextField *_titleField;
    NSTextField *_statusField;
    NSTextField *_originalStatusField;
    NSTextField *_enhancedStatusField;
    NSPopUpButton *_profilePopup;
    NSProgressIndicator *_progress;
    NSTextField *_progressLabel;
    NSButton *_installButton;
    NSButton *_installEnhancedButton;
    NSButton *_cancelDownloadButton;
    NSURLSession *_downloadSession;
    NSURLSessionDownloadTask *_downloadTask;
    NSDictionary *_downloadAsset;
    NSString *_downloadGameKey;
    NSString *_downloadReleaseTag;
    NSTask *_builderTask;
    NSFileHandle *_builderLogHandle;
    NSURL *_builderLogURL;
    NSURL *_builderOutputURL;
    NSString *_builderGameKey;
    BOOL _refreshingGame;
}

+ (instancetype)sharedController
{
    static PfhorgeContentManagerController *controller = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        controller = [[PfhorgeContentManagerController alloc] init];
    });
    return controller;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 920, 620)
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"Pfhorge Content Manager";
    window.minSize = NSMakeSize(860, 580);

    self = [super initWithWindow:window];
    if (self == nil) return nil;
    PfhorgeEnsureDirectory(PfhorgeContentRootURL(), nil);

    NSView *content = window.contentView;
    NSScrollView *sidebar = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 220, 620)];
    sidebar.autoresizingMask = NSViewHeightSizable;
    sidebar.hasVerticalScroller = YES;
    sidebar.borderType = NSBezelBorder;
    _gameTable = [[NSTableView alloc] initWithFrame:sidebar.bounds];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"game"];
    column.width = 216;
    [_gameTable addTableColumn:column];
    _gameTable.headerView = nil;
    _gameTable.rowHeight = 66;
    _gameTable.delegate = self;
    _gameTable.dataSource = self;
    sidebar.documentView = _gameTable;
    [content addSubview:sidebar];

    const CGFloat x = 244.0;
    _titleField = PfhorgeLabel(NSMakeRect(x, 566, 640, 34), @"", 24, YES);
    _titleField.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:_titleField];

    NSTextField *intro = PfhorgeLabel(NSMakeRect(x, 530, 640, 30),
        @"Choose the authentic original game appearance, an enhanced replacement pack, or a custom source. Enhanced artwork always falls back to the selected original Shapes file.", 12, NO);
    intro.usesSingleLineMode = NO;
    intro.lineBreakMode = NSLineBreakByWordWrapping;
    intro.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:intro];

    NSBox *originalBox = [[NSBox alloc] initWithFrame:NSMakeRect(x, 354, 642, 164)];
    originalBox.title = @"Original game appearance";
    originalBox.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _originalStatusField = PfhorgeLabel(NSMakeRect(18, 55, 600, 80), @"", 12, NO);
    _originalStatusField.usesSingleLineMode = NO;
    _originalStatusField.lineBreakMode = NSLineBreakByWordWrapping;
    [originalBox.contentView addSubview:_originalStatusField];
    _installButton = [[NSButton alloc] initWithFrame:NSMakeRect(18, 14, 204, 32)];
    _installButton.title = @"Install Official Original…";
    _installButton.bezelStyle = NSBezelStyleRounded;
    _installButton.target = self;
    _installButton.action = @selector(installOfficial:);
    [originalBox.contentView addSubview:_installButton];
    NSButton *choose = [[NSButton alloc] initWithFrame:NSMakeRect(232, 14, 180, 32)];
    choose.title = @"Choose Existing…";
    choose.bezelStyle = NSBezelStyleRounded;
    choose.target = self;
    choose.action = @selector(chooseExisting:);
    [originalBox.contentView addSubview:choose];
    [content addSubview:originalBox];

    NSBox *enhancedBox = [[NSBox alloc] initWithFrame:NSMakeRect(x, 196, 642, 146)];
    enhancedBox.title = @"Enhanced appearance (optional)";
    enhancedBox.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _enhancedStatusField = PfhorgeLabel(NSMakeRect(18, 50, 600, 68), @"", 12, NO);
    _enhancedStatusField.usesSingleLineMode = NO;
    _enhancedStatusField.lineBreakMode = NSLineBreakByWordWrapping;
    [enhancedBox.contentView addSubview:_enhancedStatusField];
    _installEnhancedButton = [[NSButton alloc] initWithFrame:NSMakeRect(18, 10, 250, 32)];
    _installEnhancedButton.title = @"Install Recommended Enhanced…";
    _installEnhancedButton.bezelStyle = NSBezelStyleRounded;
    _installEnhancedButton.target = self;
    _installEnhancedButton.action = @selector(installRecommendedEnhanced:);
    [enhancedBox.contentView addSubview:_installEnhancedButton];
    NSButton *import = [[NSButton alloc] initWithFrame:NSMakeRect(278, 10, 190, 32)];
    import.title = @"Import Enhanced Pack…";
    import.bezelStyle = NSBezelStyleRounded;
    import.target = self;
    import.action = @selector(importTexturePack:);
    [enhancedBox.contentView addSubview:import];
    [content addSubview:enhancedBox];

    [content addSubview:PfhorgeLabel(NSMakeRect(x, 153, 136, 24), @"Active appearance:", 13, YES)];
    _profilePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x + 140, 149, 220, 30) pullsDown:NO];
    [_profilePopup addItemsWithTitles:@[@"Original", @"Enhanced", @"Custom", @"Untextured Diagnostic"]];
    _profilePopup.target = self;
    _profilePopup.action = @selector(profileChanged:);
    [content addSubview:_profilePopup];

    NSPopUpButton *advanced = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x + 382, 149, 210, 30) pullsDown:YES];
    [advanced addItemWithTitle:@"Advanced Actions"];
    NSArray *advancedItems = @[
        @[@"Scan This Mac…", NSStringFromSelector(@selector(scanThisMac:))],
        @[@"Verify Registered Content", NSStringFromSelector(@selector(verifyContent:))],
        @[@"Repair / Reinstall…", NSStringFromSelector(@selector(repairContent:))],
        @[@"Reveal Content in Finder", NSStringFromSelector(@selector(revealContent:))],
        @[@"Open Manifest…", NSStringFromSelector(@selector(openManifest:))],
        @[@"Remove / Unregister…", NSStringFromSelector(@selector(removeContent:))],
    ];
    for (NSArray *definition in advancedItems) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:definition[0]
                                                     action:NSSelectorFromString(definition[1])
                                              keyEquivalent:@""];
        item.target = self;
        [advanced.menu addItem:item];
    }
    [content addSubview:advanced];

    _statusField = PfhorgeLabel(NSMakeRect(x, 100, 642, 42), @"", 11, NO);
    _statusField.usesSingleLineMode = NO;
    _statusField.lineBreakMode = NSLineBreakByWordWrapping;
    _statusField.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:_statusField];

    _progressLabel = PfhorgeLabel(NSMakeRect(x, 68, 510, 22), @"Ready", 12, NO);
    [content addSubview:_progressLabel];
    _progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(x, 42, 510, 20)];
    _progress.minValue = 0; _progress.maxValue = 1; _progress.doubleValue = 0;
    [content addSubview:_progress];
    _cancelDownloadButton = [[NSButton alloc] initWithFrame:NSMakeRect(x + 520, 34, 120, 32)];
    _cancelDownloadButton.title = @"Cancel Task";
    _cancelDownloadButton.bezelStyle = NSBezelStyleRounded;
    _cancelDownloadButton.target = self;
    _cancelDownloadButton.action = @selector(cancelDownload:);
    _cancelDownloadButton.hidden = YES;
    [content addSubview:_cancelDownloadButton];

    [_gameTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [self refreshSelectedGame];
    [window center];
    return self;
}

- (NSString *)selectedGameKey
{
    NSInteger row = _gameTable.selectedRow;
    if (row < 0 || row >= (NSInteger)PfhorgeContentGameKeys().count) {
        row = 0;
    }
    return PfhorgeContentGameKeys()[(NSUInteger)row];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    (void)tableView;
    return (NSInteger)PfhorgeContentGameKeys().count;
}

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    (void)tableView;
    (void)tableColumn;
    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(8, 6, 190, 46)];
    field.editable = NO;
    field.selectable = NO;
    field.bezeled = NO;
    field.drawsBackground = NO;
    field.usesSingleLineMode = NO;
    field.font = [NSFont systemFontOfSize:13];

    NSString *gameKey = PfhorgeContentGameKeys()[(NSUInteger)row];
    NSDictionary *manifest = PfhorgeReadManifest(gameKey);
    NSUInteger shapes = [manifest[@"scan"][@"shapes"] count];
    NSUInteger enhanced = [manifest[@"textureProfiles"] count];
    NSString *status = [NSString stringWithFormat:@"Original: %@  •  Enhanced: %@",
        shapes > 0 ? @"Ready" : @"Missing",
        enhanced > 0 ? @"Ready" : @"Missing"];
    field.stringValue = [NSString stringWithFormat:@"%@\n%@",
        PfhorgeContentGameDisplayName(gameKey), status];
    return field;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    (void)notification;
    if (!_refreshingGame) {
        [self refreshSelectedGame];
    }
}

- (void)refreshSelectedGame
{
    _refreshingGame = YES;
    const NSInteger selectedRow = MAX(0, _gameTable.selectedRow);
    NSString *gameKey = self.selectedGameKey;
    NSDictionary *manifest = PfhorgeReadManifest(gameKey);
    NSDictionary *recipe = PfhorgeEnhancedRecipeInfo(gameKey);
    _titleField.stringValue = PfhorgeContentGameDisplayName(gameKey);
    _installButton.enabled = ![gameKey isEqualToString:@"custom"];
    _installEnhancedButton.enabled = recipe != nil;

    NSDictionary *scan = manifest[@"scan"] ?: @{};
    NSArray *shapes = scan[@"shapes"] ?: @[];
    NSArray *mml = scan[@"mml"] ?: @[];
    NSArray *textures = scan[@"textures"] ?: @[];
    NSArray *profiles = manifest[@"textureProfiles"] ?: @[];
    NSString *rootPath = manifest[@"rootPath"] ?: @"";
    NSString *sourceType = manifest[@"sourceType"] ?: @"none";

    if (shapes.count > 0) {
        NSString *activeShapes = manifest[@"activeShapesPath"] ?: PfhorgePreferredShapesPath(gameKey, manifest) ?: @"";
        _originalStatusField.stringValue = [NSString stringWithFormat:
            @"Ready — %lu Shapes candidate(s) found.\nSource: %@\nSelected Shapes: %@",
            (unsigned long)shapes.count,
            rootPath.length > 0 ? rootPath : sourceType,
            activeShapes.length > 0 ? activeShapes.lastPathComponent : @"Select Original to activate"];
    } else {
        _originalStatusField.stringValue = [gameKey isEqualToString:@"custom"]
            ? @"No original Shapes source is registered. Choose an existing scenario folder or Shapes file."
            : @"Missing. Install the official original distribution or choose an existing installation. This supplies the canonical Shapes catalog and fallback textures.";
    }

    if (recipe != nil) {
        _enhancedStatusField.stringValue = [NSString stringWithFormat:
            @"%@\n%@\nInstalled enhanced profiles: %lu",
            recipe[@"name"], recipe[@"summary"], (unsigned long)profiles.count];
    } else {
        _enhancedStatusField.stringValue = [NSString stringWithFormat:
            @"Custom scenario replacements can be imported as an Aleph One plugin directory or ZIP. Installed profiles: %lu",
            (unsigned long)profiles.count];
    }

    NSString *profile = manifest[PfhorgeContentSelectedProfileKey] ?: @"Untextured Diagnostic";
    if ([profile isEqualToString:@"Distribution Default"]) profile = @"Original";
    if ([_profilePopup itemWithTitle:profile] != nil) [_profilePopup selectItemWithTitle:profile];
    else [_profilePopup selectItemWithTitle:@"Untextured Diagnostic"];

    _statusField.stringValue = [NSString stringWithFormat:
        @"Active: %@  •  Original external assets: %lu MML / %lu images  •  Verification: %@",
        profile, (unsigned long)mml.count, (unsigned long)textures.count,
        manifest[@"verificationStatus"] ?: @"not registered"];

    [_gameTable reloadData];
    if (_gameTable.selectedRow != selectedRow &&
        selectedRow < _gameTable.numberOfRows) {
        [_gameTable selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)selectedRow]
                byExtendingSelection:NO];
    }
    _refreshingGame = NO;
}

- (void)presentContentError:(NSError *)error
{
    NSAlert *alert = [NSAlert alertWithError:error];
    [alert beginSheetModalForWindow:self.window completionHandler:nil];
}

- (void)profileChanged:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
    if (manifest == nil) {
        NSBeep();
        [_profilePopup selectItemWithTitle:@"Untextured Diagnostic"];
        return;
    }
    NSString *previous = manifest[PfhorgeContentSelectedProfileKey] ?: @"Untextured Diagnostic";
    manifest[PfhorgeContentSelectedProfileKey] = _profilePopup.titleOfSelectedItem ?: @"Original";
    NSError *error = nil;
    if (!PfhorgeActivateContentProfile(gameKey, manifest, &error) ||
        !PfhorgeWriteManifest(gameKey, manifest, &error)) {
        manifest[PfhorgeContentSelectedProfileKey] = previous;
        [_profilePopup selectItemWithTitle:previous];
        [self presentContentError:error];
        return;
    }
    PfhorgePostVisualModeSettingsChanged();
    [self refreshSelectedGame];
}

- (void)chooseExisting:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Choose Marathon Distribution, Scenario Folder, or Shapes File";
    panel.prompt = @"Inspect";
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = YES;
    panel.allowsMultipleSelection = NO;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) {
            return;
        }
        NSURL *url = panel.URL;
        NSNumber *isDirectory = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        if (![isDirectory boolValue]) {
            url = [url URLByDeletingLastPathComponent];
        }
        [self registerExistingURL:url gameKey:self.selectedGameKey];
    }];
}

- (void)registerExistingURL:(NSURL *)sourceURL gameKey:(NSString *)gameKey
{
    _progressLabel.stringValue = @"Scanning selected content…";
    [_progress startAnimation:nil];

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *scanError = nil;
        NSDictionary *scan = PfhorgeScanContentURL(sourceURL, &scanError);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_progress stopAnimation:nil];
            if (scan == nil) {
                [self presentContentError:scanError];
                self->_progressLabel.stringValue = @"Scan failed";
                return;
            }

            NSAlert *choice = [[NSAlert alloc] init];
            choice.messageText = @"Register Marathon Content";
            choice.informativeText = [NSString stringWithFormat:
                @"Found %lu Shapes candidate(s), %lu MML file(s), %lu external texture image(s), and %lu builder recipe(s).\n\n"
                 "Use in Place leaves the source untouched and references it directly. "
                 "Copy into Pfhorge creates a managed copy under Application Support.",
                (unsigned long)[scan[@"shapes"] count],
                (unsigned long)[scan[@"mml"] count],
                (unsigned long)[scan[@"textures"] count],
                (unsigned long)[scan[@"builderRecipes"] count]];
            [choice addButtonWithTitle:@"Use in Place"];
            [choice addButtonWithTitle:@"Copy into Pfhorge"];
            [choice addButtonWithTitle:@"Cancel"];
            [choice beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
                if (result == NSAlertThirdButtonReturn) {
                    return;
                }
                if (result == NSAlertFirstButtonReturn) {
                    [self saveManifestForGame:gameKey
                                    sourceURL:sourceURL
                                  sourceType:@"external"
                                         scan:scan
                                archiveSHA256:nil
                                   sourceURLString:nil
                                   releaseTag:nil];
                    return;
                }
                [self copyExistingURLIntoManagedStorage:sourceURL gameKey:gameKey];
            }];
        });
    });
}

- (void)copyExistingURLIntoManagedStorage:(NSURL *)sourceURL gameKey:(NSString *)gameKey
{
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = @"Copying content into Pfhorge managed storage…";

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSFileManager *manager = [NSFileManager defaultManager];
        NSURL *gameDirectory = [PfhorgeContentSubdirectory(@"Distributions")
            URLByAppendingPathComponent:gameKey isDirectory:YES];
        NSError *error = nil;
        PfhorgeEnsureDirectory(gameDirectory, &error);
        NSURL *destination = [gameDirectory URLByAppendingPathComponent:
            [NSString stringWithFormat:@"%@-%@",
                sourceURL.lastPathComponent.length > 0 ? sourceURL.lastPathComponent : @"Content",
                NSUUID.UUID.UUIDString]
            isDirectory:YES];

        BOOL copied = error == nil && [manager copyItemAtURL:sourceURL toURL:destination error:&error];
        if (copied && !PfhorgeAuditManagedTree(destination, &error)) {
            copied = NO;
            [manager removeItemAtURL:destination error:nil];
        }
        NSDictionary *scan = copied ? PfhorgeScanContentURL(destination, &error) : nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
            if (!copied || scan == nil) {
                [self presentContentError:error];
                self->_progressLabel.stringValue = @"Copy failed";
                return;
            }
            [self saveManifestForGame:gameKey
                            sourceURL:destination
                          sourceType:@"managedCopy"
                                 scan:scan
                        archiveSHA256:nil
                     sourceURLString:sourceURL.absoluteString
                           releaseTag:nil];
        });
    });
}

- (void)saveManifestForGame:(NSString *)gameKey
                  sourceURL:(NSURL *)sourceURL
                sourceType:(NSString *)sourceType
                       scan:(NSDictionary *)scan
              archiveSHA256:(NSString *)archiveSHA256
           sourceURLString:(NSString *)sourceURLString
                 releaseTag:(NSString *)releaseTag
{
    NSDictionary *old = PfhorgeReadManifest(gameKey);
    NSArray *profiles = old[@"textureProfiles"] ?: @[];
    NSString *oldRootPath = old[@"rootPath"];
    NSString *oldSourceType = old[@"sourceType"];
    NSMutableDictionary *manifest = [NSMutableDictionary dictionary];
    manifest[@"schemaVersion"] = PfhorgeContentManifestSchemaVersion;
    manifest[@"game"] = gameKey;
    manifest[@"displayName"] = PfhorgeContentGameDisplayName(gameKey);
    manifest[@"sourceType"] = sourceType;
    manifest[@"rootPath"] = sourceURL.path ?: @"";
    manifest[@"scan"] = scan;
    manifest[@"installedAt"] = [NSDate date].description;
    manifest[@"verificationStatus"] = @"valid";
    manifest[@"rightsNotice"] =
        @"Pfhorge does not relicense Marathon game data or third-party artwork. "
         "All content retains its upstream license, distribution terms, credits, and ownership.";
    NSString *previousProfile = old[PfhorgeContentSelectedProfileKey];
    if ([previousProfile isEqualToString:@"Distribution Default"]) previousProfile = @"Original";
    manifest[PfhorgeContentSelectedProfileKey] = previousProfile ?: @"Original";
    manifest[@"textureProfiles"] = profiles;
    if ([scan[@"shapes"] count] > 0) {
        NSError *activationError = nil;
        if (!PfhorgeActivateContentProfile(gameKey, manifest, &activationError)) {
            NSLog(@"Content registered but could not be activated: %@", activationError.localizedDescription);
        }
    }
    if (archiveSHA256.length > 0) {
        manifest[@"archiveSHA256"] = archiveSHA256;
    }
    if (sourceURLString.length > 0) {
        manifest[@"sourceURL"] = sourceURLString;
    }
    if (releaseTag.length > 0) {
        manifest[@"releaseTag"] = releaseTag;
    }

    NSError *error = nil;
    if (!PfhorgeWriteManifest(gameKey, manifest, &error)) {
        [self presentContentError:error];
        return;
    }
    NSSet<NSString *> *managedSourceTypes = [NSSet setWithArray:@[
        @"officialDownload", @"managedCopy"
    ]];
    if ([managedSourceTypes containsObject:sourceType] &&
        oldSourceType != nil &&
        [managedSourceTypes containsObject:oldSourceType] &&
        oldRootPath.length > 0 &&
        ![oldRootPath isEqualToString:sourceURL.path]) {
        NSURL *oldRootURL = [NSURL fileURLWithPath:oldRootPath isDirectory:YES];
        if (PfhorgeURLIsInsideDirectory(
                oldRootURL,
                PfhorgeContentSubdirectory(@"Distributions"))) {
            [[NSFileManager defaultManager] removeItemAtURL:oldRootURL error:nil];
        }
    }

    _progress.doubleValue = 1;
    _progressLabel.stringValue = @"Content registered successfully";
    [self refreshSelectedGame];
    PfhorgePostVisualModeSettingsChanged();
}

- (void)installOfficial:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    if ([gameKey isEqualToString:@"custom"]) {
        return;
    }

    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = @"Checking the latest official Aleph One release…";

    NSURL *apiURL = [NSURL URLWithString:@"https://api.github.com/repos/Aleph-One-Marathon/alephone/releases/latest"];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:apiURL];
    request.timeoutInterval = 30.0;
    [request setValue:@"Pfhorge-Content-Manager/1" forHTTPHeaderField:@"User-Agent"];
    [request setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];

    [[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:
      ^(NSData *data, NSURLResponse *response, NSError *requestError) {
        (void)response;
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
        });
        if (requestError != nil || data == nil) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self presentContentError:requestError ?: [NSError errorWithDomain:@"org.pfhorge.content" code:1010 userInfo:@{NSLocalizedDescriptionKey: @"The official release manifest could not be downloaded."}]];
            });
            return;
        }

        NSError *jsonError = nil;
        NSDictionary *release = [NSJSONSerialization JSONObjectWithData:data options:0 error:&jsonError];
        NSArray *assets = [release isKindOfClass:NSDictionary.class] ? release[@"assets"] : nil;
        NSString *tag = release[@"tag_name"] ?: @"";
        NSString *expectedName = PfhorgeExpectedOfficialAssetName(gameKey, tag);
        NSString *prefix = PfhorgeOfficialAssetPrefix(gameKey);
        NSDictionary *selected = nil;
        for (NSDictionary *asset in assets) {
            if ([asset[@"name"] isEqualToString:expectedName]) {
                selected = asset;
                break;
            }
        }
        if (selected == nil) {
            for (NSDictionary *asset in assets) {
                NSString *name = asset[@"name"];
                if ([name hasPrefix:prefix] && [name hasSuffix:@"-Data.zip"]) {
                    selected = asset;
                    break;
                }
            }
        }

        NSString *digest = selected[@"digest"];
        if (selected == nil || ![digest hasPrefix:@"sha256:"]) {
            NSError *error = jsonError ?: [NSError errorWithDomain:@"org.pfhorge.content"
                                                               code:1011
                                                           userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:
                    @"The latest official release did not provide the expected %@ archive with a published SHA-256 digest.",
                    expectedName.length > 0 ? expectedName : PfhorgeContentGameDisplayName(gameKey)]}];
            dispatch_async(dispatch_get_main_queue(), ^{ [self presentContentError:error]; });
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            self->_downloadAsset = selected;
            self->_downloadGameKey = gameKey;
            self->_downloadReleaseTag = release[@"tag_name"] ?: @"unknown";

            NSAlert *confirmation = [[NSAlert alloc] init];
            confirmation.messageText = [NSString stringWithFormat:@"Install %@ Content", PfhorgeContentGameDisplayName(gameKey)];
            confirmation.informativeText = [NSString stringWithFormat:
                @"Pfhorge will download the official %@ data archive from the Aleph One GitHub release, verify its published SHA-256 digest, inspect the ZIP for unsafe paths and links, extract it into Application Support, and register its Shapes and external texture content.\n\nArchive: %@\nDownload size: %@\nDigest: %@\n\nThe Aleph One engine is GPL, while original Marathon game artwork and data retain their own distribution terms. Pfhorge does not relicense that content.",
                PfhorgeContentGameDisplayName(gameKey),
                selected[@"name"],
                PfhorgeHumanFileSize([selected[@"size"] unsignedLongLongValue]),
                digest];
            [confirmation addButtonWithTitle:@"Download and Install"];
            [confirmation addButtonWithTitle:@"Cancel"];
            [confirmation beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
                if (result == NSAlertFirstButtonReturn) {
                    [self beginOfficialDownload];
                }
            }];
        });
    }] resume];
}

- (void)beginOfficialDownload
{
    NSURL *url = [NSURL URLWithString:_downloadAsset[@"browser_download_url"]];
    if (url == nil) {
        return;
    }
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
    configuration.HTTPAdditionalHeaders = @{@"User-Agent": @"Pfhorge-Content-Manager/1"};
    _downloadSession = [NSURLSession sessionWithConfiguration:configuration
                                                    delegate:self
                                               delegateQueue:[NSOperationQueue mainQueue]];
    _downloadTask = [_downloadSession downloadTaskWithURL:url];
    _progress.indeterminate = NO;
    _progress.doubleValue = 0;
    _progressLabel.stringValue = [NSString stringWithFormat:@"Downloading %@…", _downloadAsset[@"name"]];
    _cancelDownloadButton.hidden = NO;
    [_downloadTask resume];
}

- (void)cancelDownload:(id)sender
{
    (void)sender;
    if (_builderTask != nil && _builderTask.isRunning) {
        [_builderTask terminate];
        _progressLabel.stringValue = @"Enhanced pack build cancelled";
    }
    [_downloadTask cancel];
    _downloadTask = nil;
    _cancelDownloadButton.hidden = YES;
    if (_builderTask == nil) _progressLabel.stringValue = @"Download cancelled";
}

- (void)URLSession:(NSURLSession *)session
      downloadTask:(NSURLSessionDownloadTask *)downloadTask
      didWriteData:(int64_t)bytesWritten
 totalBytesWritten:(int64_t)totalBytesWritten
 totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    (void)session;
    (void)downloadTask;
    (void)bytesWritten;
    if (totalBytesExpectedToWrite > 0) {
        _progress.doubleValue = (double)totalBytesWritten / (double)totalBytesExpectedToWrite;
        _progressLabel.stringValue = [NSString stringWithFormat:@"Downloading… %.0f%%", _progress.doubleValue * 100.0];
    }
}

- (void)URLSession:(NSURLSession *)session
      downloadTask:(NSURLSessionDownloadTask *)downloadTask
 didFinishDownloadingToURL:(NSURL *)location
{
    (void)session;
    (void)downloadTask;
    _cancelDownloadButton.hidden = YES;
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = @"Verifying and extracting official content…";

    NSString *assetName = [_downloadAsset[@"name"] lastPathComponent] ?: @"Marathon-Data.zip";
    NSString *expectedDigest = [_downloadAsset[@"digest"] substringFromIndex:@"sha256:".length];
    NSString *gameKey = [_downloadGameKey copy];
    NSString *releaseTag = [_downloadReleaseTag copy];
    NSString *sourceURL = _downloadAsset[@"browser_download_url"];

    NSURL *downloads = PfhorgeContentSubdirectory(@"Downloads");
    NSURL *archiveURL = [downloads URLByAppendingPathComponent:assetName];
    [[NSFileManager defaultManager] removeItemAtURL:archiveURL error:nil];
    NSError *copyError = nil;
    if (![[NSFileManager defaultManager] copyItemAtURL:location toURL:archiveURL error:&copyError]) {
        [self presentContentError:copyError];
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        NSString *actualDigest = PfhorgeSHA256ForFile(archiveURL, &error);
        if (actualDigest == nil || ![actualDigest.lowercaseString isEqualToString:expectedDigest.lowercaseString]) {
            error = error ?: [NSError errorWithDomain:@"org.pfhorge.content"
                                                 code:1012
                                             userInfo:@{NSLocalizedDescriptionKey: @"The downloaded archive did not match the official SHA-256 digest."}];
        }

        NSFileManager *manager = [NSFileManager defaultManager];
        NSURL *stagingRoot = [PfhorgeContentSubdirectory(@"Staging")
            URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
        NSDictionary *scan = nil;
        NSURL *finalURL = nil;

        if (error == nil && PfhorgeExtractZipArchive(archiveURL, stagingRoot, &error)) {
            scan = PfhorgeScanContentURL(stagingRoot, &error);
        }

        if (error == nil && scan != nil) {
            NSURL *gameRoot = [PfhorgeContentSubdirectory(@"Distributions")
                URLByAppendingPathComponent:gameKey isDirectory:YES];
            PfhorgeEnsureDirectory(gameRoot, &error);
            NSString *baseName = [assetName stringByDeletingPathExtension];
            NSString *installName = [NSString stringWithFormat:
                @"%@-%@", baseName, NSUUID.UUID.UUIDString];
            finalURL = [gameRoot URLByAppendingPathComponent:installName isDirectory:YES];
            if (![manager moveItemAtURL:stagingRoot toURL:finalURL error:&error]) {
                finalURL = nil;
            } else {
                scan = PfhorgeScanContentURL(finalURL, &error);
            }
        }

        if (error != nil) {
            [manager removeItemAtURL:stagingRoot error:nil];
            if (finalURL != nil &&
                PfhorgeURLIsInsideDirectory(
                    finalURL,
                    PfhorgeContentSubdirectory(@"Distributions"))) {
                [manager removeItemAtURL:finalURL error:nil];
            }
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
            if (error != nil || finalURL == nil || scan == nil) {
                [self presentContentError:error ?: [NSError errorWithDomain:@"org.pfhorge.content" code:1013 userInfo:@{NSLocalizedDescriptionKey: @"The official content could not be installed."}]];
                self->_progressLabel.stringValue = @"Installation failed";
                return;
            }
            [self saveManifestForGame:gameKey
                            sourceURL:finalURL
                          sourceType:@"officialDownload"
                                 scan:scan
                        archiveSHA256:actualDigest
                     sourceURLString:sourceURL
                           releaseTag:releaseTag];
        });
    });
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
 didCompleteWithError:(NSError *)error
{
    (void)session;
    (void)task;
    if (error != nil && error.code != NSURLErrorCancelled) {
        [self presentContentError:error];
        _progressLabel.stringValue = @"Download failed";
        _cancelDownloadButton.hidden = YES;
    }
}

- (void)verifyContent:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
    if (manifest == nil) {
        NSBeep();
        return;
    }

    NSString *rootPath = manifest[@"rootPath"] ?: @"";
    NSArray<NSDictionary *> *profiles = manifest[@"textureProfiles"] ?: @[];
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = @"Verifying registered content and texture profiles…";

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        NSDictionary *rootScan = nil;
        if (rootPath.length > 0) {
            rootScan = PfhorgeScanContentURL(
                [NSURL fileURLWithPath:rootPath isDirectory:YES],
                &error);
        }

        NSMutableArray<NSDictionary *> *verifiedProfiles =
            [NSMutableArray arrayWithCapacity:profiles.count];
        if (error == nil) {
            for (NSDictionary *profile in profiles) {
                NSMutableDictionary *verified = [profile mutableCopy];
                NSString *profilePath = profile[@"path"];
                if (profilePath.length > 0) {
                    NSDictionary *profileScan = PfhorgeScanContentURL(
                        [NSURL fileURLWithPath:profilePath isDirectory:YES],
                        &error);
                    if (profileScan == nil) {
                        break;
                    }
                    verified[@"scan"] = profileScan;
                    verified[@"verifiedAt"] = [NSDate date].description;
                }
                [verifiedProfiles addObject:verified];
            }
        }

        BOOL hasVerifiableContent = rootPath.length > 0 || profiles.count > 0;
        BOOL valid = error == nil &&
            hasVerifiableContent &&
            (rootPath.length == 0 || rootScan != nil) &&
            verifiedProfiles.count == profiles.count;

        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
            if (!valid) {
                manifest[@"verificationStatus"] = @"invalid";
                PfhorgeWriteManifest(gameKey, manifest, nil);
                [self presentContentError:error ?: [NSError errorWithDomain:@"org.pfhorge.content"
                                                               code:1020
                                                           userInfo:@{NSLocalizedDescriptionKey:
                    @"No verifiable distribution or texture profile remains registered."}]];
                [self refreshSelectedGame];
                return;
            }
            if (rootScan != nil) {
                manifest[@"scan"] = rootScan;
            }
            manifest[@"textureProfiles"] = verifiedProfiles;
            manifest[@"verificationStatus"] = @"valid";
            manifest[@"verifiedAt"] = [NSDate date].description;
            NSError *writeError = nil;
            if (!PfhorgeWriteManifest(gameKey, manifest, &writeError)) {
                [self presentContentError:writeError];
                return;
            }
            self->_progressLabel.stringValue = @"Verification passed";
            [self refreshSelectedGame];
        });
    });
}

- (void)repairContent:(id)sender
{
    (void)sender;
    NSDictionary *manifest = PfhorgeReadManifest(self.selectedGameKey);
    if (manifest == nil) {
        NSBeep();
        return;
    }
    if ([manifest[@"sourceType"] isEqualToString:@"officialDownload"]) {
        [self installOfficial:nil];
        return;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Re-select Source to Repair";
    alert.informativeText =
        @"Pfhorge can reinstall official downloads automatically. For an external or managed-copy source, select the original distribution again so Pfhorge can rescan it or create a fresh managed copy.";
    [alert addButtonWithTitle:@"Choose Source…"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result == NSAlertFirstButtonReturn) {
            [self chooseExisting:nil];
        }
    }];
}

- (void)openManifest:(id)sender
{
    (void)sender;
    NSURL *manifestURL = PfhorgeManifestURL(self.selectedGameKey);
    if (![[NSFileManager defaultManager] fileExistsAtPath:manifestURL.path]) {
        NSBeep();
        return;
    }
    [[NSWorkspace sharedWorkspace] openURL:manifestURL];
}

- (void)revealContent:(id)sender
{
    (void)sender;
    NSDictionary *manifest = PfhorgeReadManifest(self.selectedGameKey);
    NSString *path = manifest[@"rootPath"];
    if (path.length == 0) {
        NSBeep();
        return;
    }
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:path]]];
}

- (void)removeContent:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    NSDictionary *manifest = PfhorgeReadManifest(gameKey);
    if (manifest == nil) {
        return;
    }
    BOOL managed = [manifest[@"sourceType"] isEqualToString:@"managedCopy"] ||
                   [manifest[@"sourceType"] isEqualToString:@"officialDownload"];

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = managed ? @"Remove Managed Content?" : @"Unregister External Content?";
    alert.informativeText = managed
        ? @"This removes Pfhorge's managed copy and manifest. It never deletes a separately installed Marathon application or source folder."
        : @"This removes only Pfhorge's registration. The external source is not modified.";
    [alert addButtonWithTitle:managed ? @"Remove Managed Copy" : @"Unregister"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSAlertFirstButtonReturn) {
            return;
        }
        NSFileManager *manager = [NSFileManager defaultManager];
        NSURL *profileRoot = PfhorgeContentSubdirectory(@"Texture Profiles");
        for (NSDictionary *profile in (manifest[@"textureProfiles"] ?: @[])) {
            NSString *profilePath = profile[@"path"];
            NSURL *profileURL = profilePath.length > 0 ? [NSURL fileURLWithPath:profilePath] : nil;
            if (profileURL != nil && PfhorgeURLIsInsideDirectory(profileURL, profileRoot)) {
                [manager removeItemAtURL:profileURL error:nil];
            }
        }
        if (managed) {
            NSString *rootPath = manifest[@"rootPath"];
            NSURL *rootURL = rootPath.length > 0 ? [NSURL fileURLWithPath:rootPath] : nil;
            NSURL *managedRoot = PfhorgeContentSubdirectory(@"Distributions");
            if (rootURL == nil || !PfhorgeURLIsInsideDirectory(rootURL, managedRoot)) {
                NSError *safetyError = [NSError errorWithDomain:@"org.pfhorge.content"
                                                          code:1019
                                                      userInfo:@{NSLocalizedDescriptionKey:
                    @"The manifest's managed path is outside Pfhorge's content directory. Nothing was deleted."}];
                [self presentContentError:safetyError];
                return;
            }
            [manager removeItemAtURL:rootURL error:nil];
        }
        [manager removeItemAtURL:PfhorgeManifestURL(gameKey) error:nil];
        [self refreshSelectedGame];
        PfhorgePostVisualModeSettingsChanged();
    }];
}

- (void)scanThisMac:(id)sender
{
    (void)sender;
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = @"Scanning common Marathon installation locations…";

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSArray<NSString *> *roots = @[
            @"/Applications",
            [@"~/Applications" stringByExpandingTildeInPath],
            [@"~/Games" stringByExpandingTildeInPath],
            [@"~/Library/Application Support/Steam/steamapps/common" stringByExpandingTildeInPath],
        ];
        NSMutableOrderedSet<NSString *> *candidates = [NSMutableOrderedSet orderedSet];
        NSFileManager *manager = [NSFileManager defaultManager];
        NSUInteger inspected = 0;
        for (NSString *rootPath in roots) {
            NSDirectoryEnumerator<NSString *> *enumerator = [manager enumeratorAtPath:rootPath];
            for (NSString *relative in enumerator) {
                if (++inspected > 80000U) {
                    break;
                }
                if (relative.pathComponents.count > 7U) {
                    [enumerator skipDescendants];
                    continue;
                }
                NSString *full = [rootPath stringByAppendingPathComponent:relative];
                BOOL directory = NO;
                if (![manager fileExistsAtPath:full isDirectory:&directory] || directory) {
                    continue;
                }
                NSDictionary *attributes = [manager attributesOfItemAtPath:full error:nil];
                if (PfhorgeLooksLikeShapesFile([NSURL fileURLWithPath:full], attributes[NSFileSize])) {
                    NSString *candidate = [full stringByDeletingLastPathComponent];
                    [candidates addObject:candidate];
                    if (candidates.count >= 40U) {
                        break;
                    }
                }
            }
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
            if (candidates.count == 0) {
                NSAlert *none = [[NSAlert alloc] init];
                none.messageText = @"No Marathon Content Found";
                none.informativeText = @"The common installation locations did not contain a recognizable Shapes file. Use Choose Existing… to select another folder.";
                [none beginSheetModalForWindow:self.window completionHandler:nil];
                self->_progressLabel.stringValue = @"No installations found";
                return;
            }

            NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 520, 28) pullsDown:NO];
            [popup addItemsWithTitles:candidates.array];
            NSAlert *found = [[NSAlert alloc] init];
            found.messageText = @"Choose Discovered Marathon Content";
            found.informativeText = @"Pfhorge found one or more directories containing Shapes candidates. Select one to inspect before registering it.";
            found.accessoryView = popup;
            [found addButtonWithTitle:@"Inspect"];
            [found addButtonWithTitle:@"Cancel"];
            [found beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
                if (result == NSAlertFirstButtonReturn) {
                    [self registerExistingURL:[NSURL fileURLWithPath:popup.titleOfSelectedItem isDirectory:YES]
                                     gameKey:self.selectedGameKey];
                }
            }];
        });
    });
}

- (void)importTexturePack:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Import Aleph One Plugin or Texture Pack";
    panel.prompt = @"Import";
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = YES;
    panel.allowedFileTypes = @[@"zip", @"mml", @"xml", @"png", @"dds"];
    panel.allowsOtherFileTypes = YES;
    panel.allowsMultipleSelection = NO;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSModalResponseOK) {
            return;
        }
        NSURL *source = panel.URL;
        NSNumber *isDirectory = nil;
        [source getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        NSURL *profileRoot = [PfhorgeContentSubdirectory(@"Texture Profiles")
            URLByAppendingPathComponent:gameKey isDirectory:YES];
        PfhorgeEnsureDirectory(profileRoot, nil);
        NSURL *destination = [profileRoot URLByAppendingPathComponent:
            [NSString stringWithFormat:@"%@-%@",
                source.lastPathComponent.stringByDeletingPathExtension,
                NSUUID.UUID.UUIDString]
            isDirectory:YES];

        self->_progress.indeterminate = YES;
        [self->_progress startAnimation:nil];
        self->_progressLabel.stringValue = @"Importing texture profile…";

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            NSError *error = nil;
            BOOL success = NO;
            if ([isDirectory boolValue]) {
                success = [[NSFileManager defaultManager] copyItemAtURL:source toURL:destination error:&error];
            } else if ([source.pathExtension.lowercaseString isEqualToString:@"zip"]) {
                success = PfhorgeExtractZipArchive(source, destination, &error);
            } else {
                PfhorgeEnsureDirectory(destination, &error);
                success = error == nil && [[NSFileManager defaultManager]
                    copyItemAtURL:source
                            toURL:[destination URLByAppendingPathComponent:source.lastPathComponent]
                            error:&error];
            }
            if (success && !PfhorgeAuditManagedTree(destination, &error)) {
                success = NO;
            }
            NSDictionary *scan = success ? PfhorgeScanContentURL(destination, &error) : nil;

            dispatch_async(dispatch_get_main_queue(), ^{
                self->_progress.indeterminate = NO;
                [self->_progress stopAnimation:nil];
                if (!success || scan == nil) {
                    [[NSFileManager defaultManager] removeItemAtURL:destination error:nil];
                    [self presentContentError:error];
                    return;
                }

                NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
                if (manifest == nil) {
                    manifest = [@{
                        @"schemaVersion": PfhorgeContentManifestSchemaVersion,
                        @"game": gameKey,
                        @"displayName": PfhorgeContentGameDisplayName(gameKey),
                        @"sourceType": @"textureProfilesOnly",
                        @"rootPath": @"",
                        @"scan": @{@"shapes": @[], @"mml": @[], @"textures": @[], @"plugins": @[], @"builderRecipes": @[], @"rightsDocuments": @[], @"shapeRecords": @[]},
                        @"verificationStatus": @"valid",
                        PfhorgeContentSelectedProfileKey: @"Enhanced",
                    } mutableCopy];
                }
                NSMutableArray *profiles = [manifest[@"textureProfiles"] mutableCopy] ?: [NSMutableArray array];
                BOOL builderOnly = [scan[@"builderRecipes"] count] > 0 &&
                    [scan[@"mml"] count] == 0 && [scan[@"textures"] count] == 0;
                NSString *profileID = NSUUID.UUID.UUIDString;
                [profiles addObject:@{
                    @"id": profileID,
                    @"name": source.lastPathComponent.stringByDeletingPathExtension ?: @"Texture Profile",
                    @"path": destination.path,
                    @"scan": scan,
                    @"kind": builderOnly ? @"builderRecipe" : @"texturePack",
                    @"importedAt": [NSDate date].description,
                }];
                manifest[@"textureProfiles"] = profiles;
                manifest[PfhorgeContentSelectedProfileKey] = builderOnly ? @"Custom" : @"Enhanced";
                if (!builderOnly) manifest[@"selectedEnhancedProfileID"] = profileID;
                NSError *writeError = nil;
                if ((!builderOnly && !PfhorgeActivateContentProfile(gameKey, manifest, &writeError)) ||
                    !PfhorgeWriteManifest(gameKey, manifest, &writeError)) {
                    [[NSFileManager defaultManager] removeItemAtURL:destination error:nil];
                    [self presentContentError:writeError];
                    return;
                }
                self->_progressLabel.stringValue = builderOnly
                    ? @"Builder recipe registered"
                    : @"Texture profile imported";
                [self refreshSelectedGame];
                PfhorgePostVisualModeSettingsChanged();
                if (builderOnly) {
                    NSAlert *notice = [[NSAlert alloc] init];
                    notice.messageText = @"Builder Recipe Registered";
                    notice.informativeText =
                        @"This archive contains a texture-pack builder rather than the generated textures. "
                         "Pfhorge will not execute imported code automatically. Run the builder yourself, then "
                         "import its generated pack through this same Content Manager.";
                    [notice beginSheetModalForWindow:self.window completionHandler:nil];
                }
            });
        });
    }];
}


- (NSURL *)recommendedBuilderScriptForGame:(NSString *)gameKey
{
    NSDictionary *recipe = PfhorgeEnhancedRecipeInfo(gameKey);
    if (recipe == nil) return nil;
    NSURL *root = [PfhorgeContentSubdirectory(@"Builder Recipes")
        URLByAppendingPathComponent:recipe[@"directory"] isDirectory:YES];
    NSURL *script = [root URLByAppendingPathComponent:@"build_pack.py"];
    return [[NSFileManager defaultManager] fileExistsAtPath:script.path] ? script : nil;
}

- (NSString *)pythonExecutable
{
    for (NSString *path in @[@"/usr/bin/python3", @"/opt/homebrew/bin/python3", @"/usr/local/bin/python3"]) {
        if ([[NSFileManager defaultManager] isExecutableFileAtPath:path]) return path;
    }
    return nil;
}

- (void)installRecommendedEnhanced:(id)sender
{
    (void)sender;
    NSString *gameKey = self.selectedGameKey;
    NSDictionary *recipe = PfhorgeEnhancedRecipeInfo(gameKey);
    NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
    if (recipe == nil) return;
    if (PfhorgePreferredShapesPath(gameKey, manifest ?: @{}) == nil) {
        [self presentContentError:[NSError errorWithDomain:@"org.pfhorge.content" code:1050
            userInfo:@{NSLocalizedDescriptionKey:
                @"Install or choose the original game distribution first. Enhanced packs are replacements layered over the original Shapes catalog."}]];
        return;
    }
    NSURL *script = [self recommendedBuilderScriptForGame:gameKey];
    NSString *python = [self pythonExecutable];
    if (script == nil || python.length == 0) {
        [self presentContentError:[NSError errorWithDomain:@"org.pfhorge.content" code:1051
            userInfo:@{NSLocalizedDescriptionKey:
                @"The reviewed enhanced-pack recipe or Python 3 could not be found. Reapply the CONTENT-1A.1 package, then try again."}]];
        return;
    }

    NSAlert *confirm = [[NSAlert alloc] init];
    confirm.messageText = [NSString stringWithFormat:@"Install %@", recipe[@"name"]];
    NSString *components = [recipe[@"components"] componentsJoinedByString:@"\n• "];
    confirm.informativeText = [NSString stringWithFormat:
        @"Pfhorge will run its reviewed builder recipe, download the listed upstream releases into a reusable cache, preserve their credits and rights notices, generate a profile ZIP, safely extract it, and register it as the Enhanced appearance.\n\n• %@\n\n%@\n\nSome upstream hosts may require a manual browser download. The build log and download cache remain available in Application Support.",
        components, recipe[@"summary"]];
    [confirm addButtonWithTitle:@"Build and Install"];
    [confirm addButtonWithTitle:@"Cancel"];
    [confirm beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSAlertFirstButtonReturn) return;
        [self beginRecommendedEnhancedBuildForGame:gameKey recipe:recipe script:script python:python];
    }];
}

- (void)beginRecommendedEnhancedBuildForGame:(NSString *)gameKey
                                      recipe:(NSDictionary *)recipe
                                      script:(NSURL *)script
                                      python:(NSString *)python
{
    NSURL *downloads = [[PfhorgeContentSubdirectory(@"Downloads")
        URLByAppendingPathComponent:@"Enhanced Sources" isDirectory:YES]
        URLByAppendingPathComponent:gameKey isDirectory:YES];
    NSURL *runRoot = [[PfhorgeContentSubdirectory(@"Staging")
        URLByAppendingPathComponent:@"Enhanced Builders" isDirectory:YES]
        URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
    _builderOutputURL = [runRoot URLByAppendingPathComponent:@"output" isDirectory:YES];
    _builderLogURL = [runRoot URLByAppendingPathComponent:@"builder.log"];
    PfhorgeEnsureDirectory(downloads, nil);
    PfhorgeEnsureDirectory(_builderOutputURL, nil);
    [[NSFileManager defaultManager] createFileAtPath:_builderLogURL.path contents:nil attributes:nil];
    _builderLogHandle = [NSFileHandle fileHandleForWritingAtPath:_builderLogURL.path];

    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:python];
    task.arguments = @[script.path, @"--output", _builderOutputURL.path,
                       @"--downloads", downloads.path];
    task.currentDirectoryURL = [script URLByDeletingLastPathComponent];
    task.standardOutput = _builderLogHandle;
    task.standardError = _builderLogHandle;
    _builderTask = task;
    _builderGameKey = [gameKey copy];
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    _progressLabel.stringValue = [NSString stringWithFormat:@"Building %@…", recipe[@"name"]];
    _cancelDownloadButton.hidden = NO;

    __weak typeof(self) weakSelf = self;
    task.terminationHandler = ^(NSTask *completed) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf finishRecommendedEnhancedBuild:completed recipe:recipe];
        });
    };
    NSError *launchError = nil;
    if (![task launchAndReturnError:&launchError]) {
        _builderTask = nil;
        [_builderLogHandle closeFile];
        _builderLogHandle = nil;
        _progress.indeterminate = NO;
        [_progress stopAnimation:nil];
        _cancelDownloadButton.hidden = YES;
        [self presentContentError:launchError];
    }
}

- (void)finishRecommendedEnhancedBuild:(NSTask *)task recipe:(NSDictionary *)recipe
{
    [_builderLogHandle closeFile];
    _builderLogHandle = nil;
    _cancelDownloadButton.hidden = YES;
    _progress.indeterminate = YES;
    [_progress startAnimation:nil];
    NSString *gameKey = [_builderGameKey copy];
    NSURL *outputRoot = [_builderOutputURL copy];
    NSURL *logURL = [_builderLogURL copy];
    _builderTask = nil;
    _builderGameKey = nil;

    if (task.terminationStatus != 0) {
        _progress.indeterminate = NO;
        [_progress stopAnimation:nil];
        NSAlert *failure = [[NSAlert alloc] init];
        failure.messageText = @"Enhanced Pack Builder Needs Attention";
        failure.informativeText = [NSString stringWithFormat:
            @"The builder exited with status %d. An upstream host may require a manual download. The complete log and reusable download cache were preserved.\n\nLog: %@",
            task.terminationStatus, logURL.path];
        [failure addButtonWithTitle:@"Reveal Log"];
        [failure addButtonWithTitle:@"OK"];
        [failure beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
            if (result == NSAlertFirstButtonReturn) [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[logURL]];
        }];
        _progressLabel.stringValue = @"Enhanced pack build stopped";
        return;
    }

    NSURL *archive = [outputRoot URLByAppendingPathComponent:recipe[@"output"]];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        NSString *digest = PfhorgeSHA256ForFile(archive, &error);
        NSURL *staging = [[PfhorgeContentSubdirectory(@"Staging")
            URLByAppendingPathComponent:@"Enhanced Imports" isDirectory:YES]
            URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
        NSDictionary *scan = nil;
        if (error == nil && PfhorgeExtractZipArchive(archive, staging, &error)) {
            scan = PfhorgeScanContentURL(staging, &error);
        }
        NSURL *finalURL = nil;
        if (error == nil && scan != nil) {
            NSURL *profileRoot = [[PfhorgeContentSubdirectory(@"Texture Profiles")
                URLByAppendingPathComponent:gameKey isDirectory:YES]
                URLByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@", recipe[@"id"], NSUUID.UUID.UUIDString]
                isDirectory:YES];
            PfhorgeEnsureDirectory([profileRoot URLByDeletingLastPathComponent], &error);
            if (error == nil && [[NSFileManager defaultManager] moveItemAtURL:staging toURL:profileRoot error:&error]) {
                finalURL = profileRoot;
                [[NSFileManager defaultManager] copyItemAtURL:archive
                    toURL:[profileRoot URLByAppendingPathComponent:archive.lastPathComponent]
                    error:nil];
                [[NSFileManager defaultManager] copyItemAtURL:logURL
                    toURL:[profileRoot URLByAppendingPathComponent:@"builder.log"]
                    error:nil];
                scan = PfhorgeScanContentURL(profileRoot, &error);
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_progress.indeterminate = NO;
            [self->_progress stopAnimation:nil];
            if (error != nil || finalURL == nil || scan == nil) {
                [self presentContentError:error ?: [NSError errorWithDomain:@"org.pfhorge.content" code:1052
                    userInfo:@{NSLocalizedDescriptionKey:@"The generated enhanced profile could not be installed."}]];
                self->_progressLabel.stringValue = @"Enhanced installation failed";
                return;
            }
            NSMutableDictionary *manifest = [PfhorgeReadManifest(gameKey) mutableCopy];
            NSMutableArray *profiles = [manifest[@"textureProfiles"] mutableCopy] ?: [NSMutableArray array];
            NSString *profileID = NSUUID.UUID.UUIDString;
            [profiles addObject:@{
                @"id": profileID,
                @"name": recipe[@"name"],
                @"path": finalURL.path,
                @"scan": scan,
                @"kind": @"recommendedEnhanced",
                @"recipeID": recipe[@"id"],
                @"archiveSHA256": digest ?: @"",
                @"installedAt": [NSDate date].description,
            }];
            manifest[@"textureProfiles"] = profiles;
            manifest[@"selectedEnhancedProfileID"] = profileID;
            manifest[PfhorgeContentSelectedProfileKey] = @"Enhanced";
            NSError *writeError = nil;
            if (!PfhorgeActivateContentProfile(gameKey, manifest, &writeError) ||
                !PfhorgeWriteManifest(gameKey, manifest, &writeError)) {
                [self presentContentError:writeError];
                return;
            }
            self->_progress.doubleValue = 1.0;
            self->_progressLabel.stringValue = @"Enhanced appearance installed and selected";
            [self refreshSelectedGame];
            PfhorgePostVisualModeSettingsChanged();
        });
    });
}

@end

@implementation PfhorgeVisualModeSettingsController {
    NSMutableArray<NSTextField *> *_keyFields;
    NSSlider *_mouseSensitivityX;
    NSSlider *_mouseSensitivityY;
    NSButton *_invertMouseX;
    NSButton *_invertMouseY;
    NSSlider *_lookSmoothing;
    NSSlider *_movementSpeed;
    NSSlider *_verticalMovementScale;
    NSSlider *_fieldOfView;
    NSSlider *_nearPlane;
    NSPopUpButton *_frameRate;
    NSButton *_vsync;
    NSPopUpButton *_renderScale;
    NSPopUpButton *_msaa;
    NSPopUpButton *_textureFiltering;
    NSPopUpButton *_anisotropy;
    NSPopUpButton *_gpu;
    NSButton *_diagnosticsOverlay;
    NSTextField *_contentSummary;
}

+ (instancetype)sharedController
{
    static PfhorgeVisualModeSettingsController *controller = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{ controller = [[PfhorgeVisualModeSettingsController alloc] init]; });
    return controller;
}

- (instancetype)init
{
    PfhorgeRegisterVisualModeDefaults();
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 700, 550)
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered defer:NO];
    window.title = @"Visual Mode & GPU Settings";
    self = [super initWithWindow:window];
    if (self == nil) return nil;

    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(14, 58, 672, 476)];
    [window.contentView addSubview:tabs];
    NSTabViewItem *bindings = [[NSTabViewItem alloc] initWithIdentifier:@"bindings"];
    bindings.label = @"Key Bindings";
    bindings.view = [self buildKeyBindingsView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:bindings];
    NSTabViewItem *mouse = [[NSTabViewItem alloc] initWithIdentifier:@"mouse"];
    mouse.label = @"Mouse Look";
    mouse.view = [self buildMouseLookView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:mouse];
    NSTabViewItem *camera = [[NSTabViewItem alloc] initWithIdentifier:@"camera"];
    camera.label = @"Camera";
    camera.view = [self buildCameraView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:camera];
    NSTabViewItem *display = [[NSTabViewItem alloc] initWithIdentifier:@"display"];
    display.label = @"Display & GPU";
    display.view = [self buildDisplayView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:display];
    NSTabViewItem *content = [[NSTabViewItem alloc] initWithIdentifier:@"textures"];
    content.label = @"Textures & Content";
    content.view = [self buildContentView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:content];
    NSTabViewItem *diagnostics = [[NSTabViewItem alloc] initWithIdentifier:@"diagnostics"];
    diagnostics.label = @"Diagnostics";
    diagnostics.view = [self buildDiagnosticsView:NSMakeRect(0, 0, 660, 440)];
    [tabs addTabViewItem:diagnostics];

    NSButton *apply = [[NSButton alloc] initWithFrame:NSMakeRect(580, 14, 104, 32)];
    apply.title = @"Apply"; apply.bezelStyle = NSBezelStyleRounded; apply.keyEquivalent = @"\r";
    apply.target = self; apply.action = @selector(applySettings:); [window.contentView addSubview:apply];
    NSButton *defaults = [[NSButton alloc] initWithFrame:NSMakeRect(14, 14, 130, 32)];
    defaults.title = @"Restore Defaults"; defaults.bezelStyle = NSBezelStyleRounded;
    defaults.target = self; defaults.action = @selector(restoreDefaults:); [window.contentView addSubview:defaults];
    [self loadSettings]; [window center]; return self;
}

- (NSView *)buildKeyBindingsView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    _keyFields = [NSMutableArray array];
    NSArray *labels = @[@"Forward", @"Backward", @"Strafe left", @"Strafe right", @"Fly down", @"Fly up", @"Reset camera", @"Orbit diagnostic", @"Diagnostics"];
    CGFloat y = 382;
    for (NSUInteger index = 0; index < labels.count; ++index) {
        [view addSubview:PfhorgeLabel(NSMakeRect(74, y, 220, 24), labels[index], 13, NO)];
        NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(310, y - 2, 120, 26)];
        field.alignment = NSTextAlignmentCenter; field.tag = (NSInteger)index;
        [view addSubview:field]; [_keyFields addObject:field]; y -= 38;
    }
    NSTextField *note = PfhorgeLabel(NSMakeRect(74, 24, 500, 34), @"Movement keys use continuous key state and are independent of macOS key-repeat timing. Each action must use a distinct key.", 12, NO);
    note.usesSingleLineMode = NO; note.lineBreakMode = NSLineBreakByWordWrapping; [view addSubview:note];
    return view;
}

- (NSView *)buildMouseLookView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    [view addSubview:PfhorgeLabel(NSMakeRect(58, 370, 210, 24), @"Horizontal sensitivity", 13, YES)];
    _mouseSensitivityX = [[NSSlider alloc] initWithFrame:NSMakeRect(282, 368, 300, 24)];
    _mouseSensitivityX.minValue = 0.002; _mouseSensitivityX.maxValue = 0.030; [view addSubview:_mouseSensitivityX];
    [view addSubview:PfhorgeLabel(NSMakeRect(58, 322, 210, 24), @"Vertical sensitivity", 13, YES)];
    _mouseSensitivityY = [[NSSlider alloc] initWithFrame:NSMakeRect(282, 320, 300, 24)];
    _mouseSensitivityY.minValue = 0.002; _mouseSensitivityY.maxValue = 0.030; [view addSubview:_mouseSensitivityY];
    _invertMouseX = [[NSButton alloc] initWithFrame:NSMakeRect(58, 270, 260, 24)];
    _invertMouseX.buttonType = NSSwitchButton; _invertMouseX.title = @"Invert horizontal look"; [view addSubview:_invertMouseX];
    _invertMouseY = [[NSButton alloc] initWithFrame:NSMakeRect(330, 270, 260, 24)];
    _invertMouseY.buttonType = NSSwitchButton; _invertMouseY.title = @"Invert vertical look"; [view addSubview:_invertMouseY];
    [view addSubview:PfhorgeLabel(NSMakeRect(58, 218, 210, 24), @"Look smoothing", 13, YES)];
    _lookSmoothing = [[NSSlider alloc] initWithFrame:NSMakeRect(282, 216, 300, 24)];
    _lookSmoothing.minValue = 0.0; _lookSmoothing.maxValue = 0.85; [view addSubview:_lookSmoothing];
    NSTextField *description = PfhorgeLabel(NSMakeRect(58, 76, 530, 116),
        @"Current interaction: drag inside Visual Mode to look in first-person mode or orbit in whole-map diagnostic mode. By default, dragging right turns right and dragging up looks up. Horizontal and vertical inversion are independent. Smoothing blends abrupt deltas; zero is raw immediate input.", 13, NO);
    description.usesSingleLineMode = NO; description.lineBreakMode = NSLineBreakByWordWrapping; [view addSubview:description];
    return view;
}

- (NSView *)buildCameraView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    NSArray *labels = @[@"Horizontal movement speed", @"Vertical movement multiplier", @"Field of view", @"Near clipping distance"];
    CGFloat rows[] = {366, 294, 222, 150};
    for (NSUInteger i=0;i<labels.count;++i) [view addSubview:PfhorgeLabel(NSMakeRect(58, rows[i], 220, 24), labels[i], 13, YES)];
    _movementSpeed = [[NSSlider alloc] initWithFrame:NSMakeRect(286, 364, 300, 24)]; _movementSpeed.minValue=0.5; _movementSpeed.maxValue=20.0; [view addSubview:_movementSpeed];
    _verticalMovementScale = [[NSSlider alloc] initWithFrame:NSMakeRect(286, 292, 300, 24)]; _verticalMovementScale.minValue=0.1; _verticalMovementScale.maxValue=3.0; [view addSubview:_verticalMovementScale];
    _fieldOfView = [[NSSlider alloc] initWithFrame:NSMakeRect(286, 220, 300, 24)]; _fieldOfView.minValue=40; _fieldOfView.maxValue=110; [view addSubview:_fieldOfView];
    _nearPlane = [[NSSlider alloc] initWithFrame:NSMakeRect(286, 148, 300, 24)]; _nearPlane.minValue=0.002; _nearPlane.maxValue=0.10; [view addSubview:_nearPlane];
    NSTextField *note = PfhorgeLabel(NSMakeRect(58, 54, 530, 66),
        @"WASD moves on the horizontal plane; Fly Up/Down uses the vertical multiplier. The scroll wheel changes the session movement multiplier in first-person mode and orbit distance in diagnostic mode.", 12, NO);
    note.usesSingleLineMode=NO; note.lineBreakMode=NSLineBreakByWordWrapping; [view addSubview:note]; return view;
}

- (NSView *)buildDisplayView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    NSArray *labels = @[@"Metal device", @"Frame-rate limit", @"Render scale", @"MSAA", @"Texture filtering", @"Anisotropic filtering"];
    CGFloat rows[] = {374, 318, 262, 206, 150, 94};
    for (NSUInteger i=0;i<labels.count;++i) [view addSubview:PfhorgeLabel(NSMakeRect(48, rows[i], 190, 24), labels[i], 13, YES)];
    _gpu=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,370,360,28) pullsDown:NO]; [_gpu addItemWithTitle:@"System Default"]; _gpu.itemArray.lastObject.representedObject=@(0ULL);
    for(id<MTLDevice> device in PfhorgeAvailableMetalDevices()){[_gpu addItemWithTitle:device.name?:@"Metal Device"];_gpu.itemArray.lastObject.representedObject=@(device.registryID);} [view addSubview:_gpu];
    _frameRate=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,314,180,28) pullsDown:NO]; for(NSNumber *fps in @[@30,@60,@120]){[_frameRate addItemWithTitle:[NSString stringWithFormat:@"%@ FPS",fps]];_frameRate.itemArray.lastObject.representedObject=fps;} [view addSubview:_frameRate];
    _renderScale=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,258,180,28) pullsDown:NO]; for(NSNumber *scale in @[@0.5,@0.75,@1.0,@1.5,@2.0]){[_renderScale addItemWithTitle:[NSString stringWithFormat:@"%.0f%%",scale.doubleValue*100.0]];_renderScale.itemArray.lastObject.representedObject=scale;} [view addSubview:_renderScale];
    _msaa=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,202,180,28) pullsDown:NO]; for(NSNumber *sample in @[@1,@2,@4,@8]){[_msaa addItemWithTitle:sample.integerValue==1?@"Off":[NSString stringWithFormat:@"%@×",sample]];_msaa.itemArray.lastObject.representedObject=sample;} [view addSubview:_msaa];
    _textureFiltering=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,146,180,28) pullsDown:NO]; NSArray *filters=@[@"Nearest",@"Linear",@"Trilinear"]; for(NSUInteger i=0;i<filters.count;++i){[_textureFiltering addItemWithTitle:filters[i]];_textureFiltering.itemArray.lastObject.representedObject=@(i);} [view addSubview:_textureFiltering];
    _anisotropy=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(242,90,180,28) pullsDown:NO]; for(NSNumber *level in @[@1,@2,@4,@8,@16]){[_anisotropy addItemWithTitle:[NSString stringWithFormat:@"%@×",level]];_anisotropy.itemArray.lastObject.representedObject=level;} [view addSubview:_anisotropy];
    _vsync=[[NSButton alloc] initWithFrame:NSMakeRect(242,50,230,24)];_vsync.buttonType=NSSwitchButton;_vsync.title=@"Vertical synchronization";[view addSubview:_vsync];
    NSTextField *note=PfhorgeLabel(NSMakeRect(48,12,570,28),@"Metal device and MSAA apply on the next Visual Mode entry. Filtering options are consumed by TEX-1A.",11,NO);[view addSubview:note]; return view;
}

- (NSView *)buildContentView:(NSRect)frame
{
    NSView *view=[[NSView alloc] initWithFrame:frame]; _contentSummary=PfhorgeLabel(NSMakeRect(46,128,560,260),@"",13,NO);_contentSummary.usesSingleLineMode=NO;_contentSummary.lineBreakMode=NSLineBreakByWordWrapping;[view addSubview:_contentSummary];
    NSButton *manage=[[NSButton alloc] initWithFrame:NSMakeRect(46,72,220,32)];manage.title=@"Open Content Manager…";manage.bezelStyle=NSBezelStyleRounded;manage.target=self;manage.action=@selector(openContentManager:);[view addSubview:manage]; return view;
}

- (NSView *)buildDiagnosticsView:(NSRect)frame
{
    NSView *view=[[NSView alloc] initWithFrame:frame];_diagnosticsOverlay=[[NSButton alloc] initWithFrame:NSMakeRect(46,370,330,24)];_diagnosticsOverlay.buttonType=NSSwitchButton;_diagnosticsOverlay.title=@"Show Visual Mode diagnostics overlay";[view addSubview:_diagnosticsOverlay];
    NSTextField *description=PfhorgeLabel(NSMakeRect(46,150,560,190),@"The overlay reports FPS, frame time, camera coordinates, seed polygon, visible polygon and surface counts, accepted portals, and rendered vertices. The Diagnostics key (I by default) prints the complete VM-3C snapshot to Terminal.",13,NO);description.usesSingleLineMode=NO;description.lineBreakMode=NSLineBreakByWordWrapping;[view addSubview:description];return view;
}

- (void)showWindow:(id)sender { [self loadSettings]; [super showWindow:sender]; [self.window makeKeyAndOrderFront:sender]; }

- (void)loadSettings
{
    PfhorgeRegisterVisualModeDefaults(); NSUserDefaults *d=NSUserDefaults.standardUserDefaults;
    NSArray *keys=@[PfhorgeVMForwardKeyPreference,PfhorgeVMBackwardKeyPreference,PfhorgeVMStrafeLeftKeyPreference,PfhorgeVMStrafeRightKeyPreference,PfhorgeVMFlyDownKeyPreference,PfhorgeVMFlyUpKeyPreference,PfhorgeVMResetKeyPreference,PfhorgeVMOrbitKeyPreference,PfhorgeVMDiagnosticsKeyPreference];
    for(NSUInteger i=0;i<keys.count;++i)_keyFields[i].stringValue=PfhorgeVisualModeKeyDisplayName(PfhorgeVisualModeKey(keys[i],'?'));
    _mouseSensitivityX.doubleValue=[d doubleForKey:PfhorgeVMMouseSensitivityXPreference]; _mouseSensitivityY.doubleValue=[d doubleForKey:PfhorgeVMMouseSensitivityYPreference];
    _invertMouseX.state=[d boolForKey:PfhorgeVMInvertMouseXPreference]?NSControlStateValueOn:NSControlStateValueOff; _invertMouseY.state=[d boolForKey:PfhorgeVMInvertMouseYPreference]?NSControlStateValueOn:NSControlStateValueOff;
    _lookSmoothing.doubleValue=[d doubleForKey:PfhorgeVMLookSmoothingPreference]; _movementSpeed.doubleValue=[d doubleForKey:PfhorgeVMMovementSpeedPreference]; _verticalMovementScale.doubleValue=[d doubleForKey:PfhorgeVMVerticalMovementScalePreference]; _fieldOfView.doubleValue=[d doubleForKey:PfhorgeVMFieldOfViewPreference]; _nearPlane.doubleValue=[d doubleForKey:PfhorgeVMNearPlanePreference];
    _vsync.state=[d boolForKey:PfhorgeVMVSyncPreference]?NSControlStateValueOn:NSControlStateValueOff; _diagnosticsOverlay.state=[d boolForKey:PfhorgeVMDiagnosticsOverlayPreference]?NSControlStateValueOn:NSControlStateValueOff;
    NSInteger fps=[d integerForKey:PfhorgeVMFrameRatePreference]; for(NSMenuItem *item in _frameRate.itemArray)if([item.representedObject integerValue]==fps){[_frameRate selectItem:item];break;}
    double scale=[d doubleForKey:PfhorgeVMRenderScalePreference];for(NSMenuItem *item in _renderScale.itemArray)if(fabs([item.representedObject doubleValue]-scale)<0.001){[_renderScale selectItem:item];break;}
    NSInteger samples=[d integerForKey:PfhorgeVMMSAASampleCountPreference];for(NSMenuItem *item in _msaa.itemArray)if([item.representedObject integerValue]==samples){[_msaa selectItem:item];break;}
    NSInteger filtering=[d integerForKey:PfhorgeVMTextureFilteringPreference];for(NSMenuItem *item in _textureFiltering.itemArray)if([item.representedObject integerValue]==filtering){[_textureFiltering selectItem:item];break;}
    NSInteger anisotropy=[d integerForKey:PfhorgeVMAnisotropyPreference];for(NSMenuItem *item in _anisotropy.itemArray)if([item.representedObject integerValue]==anisotropy){[_anisotropy selectItem:item];break;}
    unsigned long long registryID=[[d objectForKey:PfhorgeVMPreferredMetalRegistryIDPreference] unsignedLongLongValue];for(NSMenuItem *item in _gpu.itemArray)if([item.representedObject unsignedLongLongValue]==registryID){[_gpu selectItem:item];break;}
    NSMutableString *summary=[NSMutableString stringWithString:@"Each game has a canonical Original Shapes source and an optional Enhanced replacement profile. Enhanced never replaces the original catalog; missing replacement images fall back to Shapes.\n\n"];
    for(NSString *gameKey in @[@"marathon",@"marathon2",@"infinity"]){NSDictionary *m=PfhorgeReadManifest(gameKey);[summary appendFormat:@"%@: Original %@ • Enhanced %@ • Active %@\n",PfhorgeContentGameDisplayName(gameKey),[m[@"scan"][@"shapes"] count]>0?@"Ready":@"Missing",[m[@"textureProfiles"] count]>0?@"Ready":@"Missing",m[PfhorgeContentSelectedProfileKey]?:@"Untextured Diagnostic"];}
    _contentSummary.stringValue=summary;
}

- (void)applySettings:(id)sender
{
    (void)sender; if(![self.window makeFirstResponder:nil]){NSBeep();return;}
    NSArray *keys=@[PfhorgeVMForwardKeyPreference,PfhorgeVMBackwardKeyPreference,PfhorgeVMStrafeLeftKeyPreference,PfhorgeVMStrafeRightKeyPreference,PfhorgeVMFlyDownKeyPreference,PfhorgeVMFlyUpKeyPreference,PfhorgeVMResetKeyPreference,PfhorgeVMOrbitKeyPreference,PfhorgeVMDiagnosticsKeyPreference]; const unichar fallback[]={'w','s','a','d','q','e','r','p','i'};
    NSMutableSet *seen=[NSMutableSet set];NSMutableArray *values=[NSMutableArray array];for(NSUInteger i=0;i<keys.count;++i){unichar value=PfhorgeVisualModeKeyFromString(_keyFields[i].stringValue,fallback[i]);NSNumber *n=@(value);if([seen containsObject:n]){NSAlert *a=[[NSAlert alloc] init];a.messageText=@"Duplicate Visual Mode Binding";a.informativeText=@"Each action must have a distinct key.";[a beginSheetModalForWindow:self.window completionHandler:nil];return;}[seen addObject:n];[values addObject:n];}
    NSUserDefaults *d=NSUserDefaults.standardUserDefaults;for(NSUInteger i=0;i<keys.count;++i)[d setInteger:[values[i] integerValue] forKey:keys[i]];
    [d setDouble:_mouseSensitivityX.doubleValue forKey:PfhorgeVMMouseSensitivityXPreference];[d setDouble:_mouseSensitivityY.doubleValue forKey:PfhorgeVMMouseSensitivityYPreference];[d setBool:_invertMouseX.state==NSControlStateValueOn forKey:PfhorgeVMInvertMouseXPreference];[d setBool:_invertMouseY.state==NSControlStateValueOn forKey:PfhorgeVMInvertMouseYPreference];[d setDouble:_lookSmoothing.doubleValue forKey:PfhorgeVMLookSmoothingPreference];
    [d setDouble:_movementSpeed.doubleValue forKey:PfhorgeVMMovementSpeedPreference];[d setDouble:_verticalMovementScale.doubleValue forKey:PfhorgeVMVerticalMovementScalePreference];[d setDouble:_fieldOfView.doubleValue forKey:PfhorgeVMFieldOfViewPreference];[d setDouble:_nearPlane.doubleValue forKey:PfhorgeVMNearPlanePreference];
    [d setInteger:[_frameRate.selectedItem.representedObject integerValue] forKey:PfhorgeVMFrameRatePreference];[d setBool:_vsync.state==NSControlStateValueOn forKey:PfhorgeVMVSyncPreference];[d setDouble:[_renderScale.selectedItem.representedObject doubleValue] forKey:PfhorgeVMRenderScalePreference];[d setInteger:[_msaa.selectedItem.representedObject integerValue] forKey:PfhorgeVMMSAASampleCountPreference];[d setInteger:[_textureFiltering.selectedItem.representedObject integerValue] forKey:PfhorgeVMTextureFilteringPreference];[d setInteger:[_anisotropy.selectedItem.representedObject integerValue] forKey:PfhorgeVMAnisotropyPreference];[d setObject:_gpu.selectedItem.representedObject?:@(0ULL) forKey:PfhorgeVMPreferredMetalRegistryIDPreference];[d setBool:_diagnosticsOverlay.state==NSControlStateValueOn forKey:PfhorgeVMDiagnosticsOverlayPreference];
    PfhorgePostVisualModeSettingsChanged(); [self.window orderOut:nil];
}

- (void)restoreDefaults:(id)sender { (void)sender;NSUserDefaults *d=NSUserDefaults.standardUserDefaults;for(NSString *key in PfhorgeVisualModeDefaultValues())[d removeObjectForKey:key];[d removeObjectForKey:PfhorgeVMSettingsMigrationVersionPreference];PfhorgeRegisterVisualModeDefaults();PfhorgePostVisualModeSettingsChanged();[self loadSettings]; }
- (void)openContentManager:(id)sender { [[PfhorgeContentManagerController sharedController] showWindow:sender]; }

@end

@interface LEDelegate (PfhorgeContentAndVisualModeActions)
- (IBAction)showPfhorgeContentManager:(id)sender;
- (IBAction)showPfhorgeVisualModeSettings:(id)sender;
@end

@implementation LEDelegate (PfhorgeContentAndVisualModeActions)

- (IBAction)showPfhorgeContentManager:(id)sender
{
    [[PfhorgeContentManagerController sharedController] showWindow:sender];
}

- (IBAction)showPfhorgeVisualModeSettings:(id)sender
{
    [[PfhorgeVisualModeSettingsController sharedController] showWindow:sender];
}

@end

static NSMenu *PfhorgeFindMainMenu(NSString *title)
{
    for (NSMenuItem *item in NSApp.mainMenu.itemArray) {
        if ([item.title isEqualToString:title]) {
            return item.submenu;
        }
    }
    return nil;
}

static BOOL PfhorgeMenuContainsAction(NSMenu *menu, SEL action)
{
    for (NSMenuItem *item in menu.itemArray) {
        if (item.action == action) {
            return YES;
        }
    }
    return NO;
}

static void PfhorgeInstallContentAndVisualModeMenus(LEDelegate *target)
{
    PfhorgeRegisterVisualModeDefaults();

    NSMenu *appMenu = PfhorgeFindMainMenu(@"Pfhorge");
    if (appMenu != nil &&
        !PfhorgeMenuContainsAction(appMenu, @selector(showPfhorgeContentManager:))) {
        NSMenuItem *item = [[NSMenuItem alloc]
            initWithTitle:@"Content Manager…"
                   action:@selector(showPfhorgeContentManager:)
            keyEquivalent:@""];
        item.target = target;
        NSInteger index = MIN((NSInteger)appMenu.numberOfItems, 3);
        [appMenu insertItem:item atIndex:index];
    }

    NSMenu *viewMenu = PfhorgeFindMainMenu(@"View");
    if (viewMenu != nil &&
        !PfhorgeMenuContainsAction(viewMenu, @selector(showPfhorgeVisualModeSettings:))) {
        NSMenuItem *item = [[NSMenuItem alloc]
            initWithTitle:@"Visual Mode & GPU Settings…"
                   action:@selector(showPfhorgeVisualModeSettings:)
            keyEquivalent:@""];
        item.target = target;

        NSInteger insertionIndex = 0;
        for (NSInteger index = 0; index < viewMenu.numberOfItems; ++index) {
            NSMenuItem *existing = [viewMenu itemAtIndex:index];
            if (existing.action == @selector(enterVisualMode:)) {
                insertionIndex = index + 1;
                break;
            }
        }
        [viewMenu insertItem:item atIndex:insertionIndex];
    }

    // Reconnect the last selected managed Shapes source to the legacy texture
    // repository after the application delegate has finished creating menus.
    PfhorgeRestoreActiveContentSelection();
}
