// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// MAP-1B transactional import helpers. Included by
// PhPfhorgeScenarioLevelDoc.m after PfhorgeMarathonMapIntakeBridge.inc.

#import <Cocoa/Cocoa.h>
#import <CommonCrypto/CommonDigest.h>

static NSString *PfhorgeSHA256String(NSData *data)
{
    if (data == nil || data.length == 0U) {
        return @"";
    }

    unsigned char digest[CC_SHA256_DIGEST_LENGTH] = {0};
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);

    NSMutableString *result =
        [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH * 2U];
    for (NSUInteger index = 0; index < CC_SHA256_DIGEST_LENGTH; ++index) {
        [result appendFormat:@"%02x", digest[index]];
    }
    return result;
}

static NSString *PfhorgeOriginalLevelName(
    const PfhMapProbeResult *probe,
    NSUInteger ordinal,
    NSArray<NSString *> *convertedNames)
{
    if (ordinal < probe->directoryEntryCount) {
        const PfhMapDirectoryEntrySummary *entry =
            &probe->directoryEntries[ordinal];
        if (entry->hasLevelName) {
            NSString *name = PfhorgeLegacyStringFromBytes(
                entry->levelName,
                sizeof(entry->levelName));
            if (name.length > 0U) {
                return name;
            }
        }
    }

    if (ordinal < convertedNames.count && convertedNames[ordinal].length > 0U) {
        return convertedNames[ordinal];
    }

    return [NSString stringWithFormat:@"Level %lu", (unsigned long)(ordinal + 1U)];
}

static NSIndexSet *PfhorgeChooseMarathonEntries(
    NSWindow *parentWindow,
    const PfhMapProbeResult *probe,
    NSString *sourceDescription,
    NSString *finderType,
    NSUInteger resourceForkLength)
{
    (void)parentWindow;

    if (probe->directoryEntryCount == 0U) {
        return nil;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText =
        probe->mapKind == PfhMapKindMergedScenario
            ? NSLocalizedString(@"Merged Marathon Scenario Identified",
                                @"Merged Marathon scenario import title")
            : NSLocalizedString(@"Marathon Map Identified",
                                @"Marathon map import title");
    alert.informativeText = PfhorgeMapIdentificationText(
        probe,
        sourceDescription,
        finderType,
        resourceForkLength);
    alert.alertStyle = NSAlertStyleInformational;

    NSMutableArray<NSButton *> *entryButtons = [NSMutableArray array];

    if (probe->directoryEntryCount > 1U) {
        const CGFloat rowHeight = 26.0;
        const CGFloat contentWidth = 520.0;
        const CGFloat contentHeight =
            MIN(260.0, rowHeight * (CGFloat)probe->directoryEntryCount + 8.0);

        NSView *documentView = [[NSView alloc]
            initWithFrame:NSMakeRect(
                0.0,
                0.0,
                contentWidth,
                rowHeight * (CGFloat)probe->directoryEntryCount)];

        for (NSUInteger ordinal = 0U;
             ordinal < probe->directoryEntryCount;
             ++ordinal) {
            const PfhMapDirectoryEntrySummary *entry =
                &probe->directoryEntries[ordinal];
            NSString *name = PfhorgeLegacyStringFromBytes(
                entry->levelName,
                sizeof(entry->levelName));
            if (name.length == 0U) {
                name = [NSString stringWithFormat:@"Level %lu",
                    (unsigned long)(ordinal + 1U)];
            }

            NSString *title = [NSString stringWithFormat:
                @"%@  —  directory %lu, logical index %d",
                name,
                (unsigned long)entry->directoryOrdinal,
                entry->logicalIndex];

            NSButton *button = [[NSButton alloc]
                initWithFrame:NSMakeRect(
                    8.0,
                    documentView.bounds.size.height -
                        rowHeight * (CGFloat)(ordinal + 1U),
                    contentWidth - 16.0,
                    rowHeight)];
            button.buttonType = NSSwitchButton;
            button.title = title;
            button.state = NSControlStateValueOn;
            button.tag = (NSInteger)ordinal;
            [documentView addSubview:button];
            [entryButtons addObject:button];
        }

        NSScrollView *scrollView = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(0.0, 0.0, contentWidth, contentHeight)];
        scrollView.hasVerticalScroller =
            documentView.bounds.size.height > contentHeight;
        scrollView.borderType = NSBezelBorder;
        scrollView.documentView = documentView;
        alert.accessoryView = scrollView;
    }

    NSString *importTitle =
        probe->directoryEntryCount > 1U
            ? NSLocalizedString(@"Import Selected Levels",
                                @"Import selected Marathon levels")
            : NSLocalizedString(@"Import", @"Import Marathon map");
    [alert addButtonWithTitle:importTitle];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"Cancel map import")];

    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return nil;
    }

    if (probe->directoryEntryCount == 1U) {
        return [NSIndexSet indexSetWithIndex:0U];
    }

    NSMutableIndexSet *selected = [NSMutableIndexSet indexSet];
    for (NSButton *button in entryButtons) {
        if (button.state == NSControlStateValueOn) {
            [selected addIndex:(NSUInteger)button.tag];
        }
    }

    if (selected.count == 0U) {
        NSAlert *emptyAlert = [[NSAlert alloc] init];
        emptyAlert.messageText = NSLocalizedString(
            @"No Levels Selected",
            @"No Marathon levels selected title");
        emptyAlert.informativeText = NSLocalizedString(
            @"Select at least one level to import.",
            @"No Marathon levels selected explanation");
        emptyAlert.alertStyle = NSAlertStyleWarning;
        [emptyAlert runModal];
        return nil;
    }

    return selected;
}

static BOOL PfhorgeConfirmSourceDestinationSeparation(
    NSURL *sourceURL,
    NSString *destinationDirectory)
{
    NSString *sourceDirectory =
        [[[sourceURL URLByDeletingLastPathComponent] path]
            stringByStandardizingPath];
    NSString *destination =
        [destinationDirectory stringByStandardizingPath];

    if (![sourceDirectory isEqualToString:destination]) {
        return YES;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(
        @"The Pfhorge Project Is Beside the Historical Source",
        @"Import source/destination collision title");
    alert.informativeText = NSLocalizedString(
        @"Importing here will create Pfhorge .pfhlev files beside the original "
         "Marathon map or merged scenario. The historical source will not be "
         "overwritten, but keeping working files in a separate project folder "
         "is strongly recommended.",
        @"Import source/destination collision explanation");
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:NSLocalizedString(
        @"Cancel and Move Project",
        @"Cancel import to move project")];
    [alert addButtonWithTitle:NSLocalizedString(
        @"Import Here Anyway",
        @"Override source/destination warning")];

    return [alert runModal] == NSAlertSecondButtonReturn;
}

static NSString *PfhorgeSanitizedLevelName(NSString *name, NSUInteger ordinal)
{
    NSString *candidate =
        [name stringByTrimmingCharactersInSet:
            NSCharacterSet.whitespaceAndNewlineCharacterSet];

    if (candidate.length == 0U ||
        [candidate isEqualToString:@"."] ||
        [candidate isEqualToString:@".."]) {
        candidate = [NSString stringWithFormat:@"Level %lu",
            (unsigned long)(ordinal + 1U)];
    }

    NSMutableCharacterSet *forbidden =
        [NSCharacterSet.controlCharacterSet mutableCopy];
    [forbidden addCharactersInString:@"/:\\"];
    NSArray<NSString *> *components =
        [candidate componentsSeparatedByCharactersInSet:forbidden];
    candidate = [components componentsJoinedByString:@"-"];

    while ([candidate containsString:@"--"]) {
        candidate = [candidate stringByReplacingOccurrencesOfString:@"--"
                                                          withString:@"-"];
    }

    candidate = [candidate stringByTrimmingCharactersInSet:
        [NSCharacterSet characterSetWithCharactersInString:@" .-"]];

    if (candidate.length == 0U) {
        candidate = [NSString stringWithFormat:@"Level %lu",
            (unsigned long)(ordinal + 1U)];
    }

    if (candidate.length > 120U) {
        candidate = [candidate substringToIndex:120U];
    }

    return candidate;
}

static NSArray<NSString *> *PfhorgeUniqueNativeLevelNames(
    NSArray<NSString *> *originalNames,
    NSString *destinationDirectory)
{
    NSMutableArray<NSString *> *result =
        [NSMutableArray arrayWithCapacity:originalNames.count];
    NSMutableSet<NSString *> *reserved = [NSMutableSet set];
    NSFileManager *manager = NSFileManager.defaultManager;

    [originalNames enumerateObjectsUsingBlock:
        ^(NSString *name, NSUInteger ordinal, BOOL *stop) {
            (void)stop;
            NSString *base = PfhorgeSanitizedLevelName(name, ordinal);
            NSString *candidate = base;
            NSUInteger suffix = 2U;

            while ([reserved containsObject:candidate.lowercaseString] ||
                   [manager fileExistsAtPath:
                       [[[destinationDirectory stringByAppendingPathComponent:candidate]
                           stringByAppendingPathExtension:@"pfhlev"]
                           stringByStandardizingPath]]) {
                candidate = [NSString stringWithFormat:@"%@ (%lu)",
                    base,
                    (unsigned long)suffix++];
            }

            [reserved addObject:candidate.lowercaseString];
            [result addObject:candidate];
        }];

    return result;
}

static BOOL PfhorgeStageImportedLevelFiles(
    NSArray<LELevelData *> *levels,
    NSArray<NSString *> *nativeNames,
    NSString *destinationDirectory,
    NSError **error)
{
    if (levels.count != nativeNames.count || levels.count == 0U) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                nil,
                @"The converted level data and level-name lists do not match.");
        }
        return NO;
    }

    NSFileManager *manager = NSFileManager.defaultManager;
    NSString *stagingDirectory = [destinationDirectory
        stringByAppendingPathComponent:
            [@".pfhorge-import-staging-"
                stringByAppendingString:NSUUID.UUID.UUIDString]];

    if (![manager createDirectoryAtPath:stagingDirectory
             withIntermediateDirectories:NO
                              attributes:nil
                                   error:error]) {
        return NO;
    }

    NSMutableArray<NSString *> *stagedPaths = [NSMutableArray array];
    NSMutableArray<NSString *> *finalPaths = [NSMutableArray array];
    BOOL succeeded = YES;

    for (NSUInteger index = 0U; index < levels.count; ++index) {
        NSString *fileName = [nativeNames[index]
            stringByAppendingPathExtension:@"pfhlev"];
        NSString *stagedPath = [stagingDirectory
            stringByAppendingPathComponent:fileName];
        NSString *finalPath = [destinationDirectory
            stringByAppendingPathComponent:fileName];

        NSData *nativePackage =
            PfhorgeNativePackageForLevel(
                levels[index],
                error);

        if (nativePackage == nil ||
            ![nativePackage writeToFile:stagedPath
                         options:NSDataWritingAtomic
                           error:error]) {
            succeeded = NO;
            break;
        }

        (void)[manager setAttributes:@{
            NSFileHFSTypeCode: @((OSType)'PfhL'),
            NSFileHFSCreatorCode: @((OSType)'PFrg'),
        } ofItemAtPath:stagedPath error:NULL];

        [stagedPaths addObject:stagedPath];
        [finalPaths addObject:finalPath];
    }

    NSMutableArray<NSString *> *movedPaths = [NSMutableArray array];
    if (succeeded) {
        for (NSUInteger index = 0U; index < stagedPaths.count; ++index) {
            if (![manager moveItemAtPath:stagedPaths[index]
                                  toPath:finalPaths[index]
                                   error:error]) {
                succeeded = NO;
                break;
            }
            [movedPaths addObject:finalPaths[index]];
        }
    }

    if (!succeeded) {
        for (NSString *path in movedPaths) {
            [manager removeItemAtPath:path error:NULL];
        }
    }

    [manager removeItemAtPath:stagingDirectory error:NULL];
    return succeeded;
}

static BOOL PfhorgeKnownEditableMapTag(NSString *tag)
{
    static NSSet<NSString *> *known = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        known = [NSSet setWithArray:@[
            @"PNTS", @"EPNT", @"LINS", @"SIDS", @"POLY",
            @"LITE", @"OBJS", @"Minf", @"plat", @"medi",
            @"ambi", @"bonk", @"plac", @"NOTE", @"term"
        ]];
    });
    return [known containsObject:tag];
}

static BOOL PfhorgeWriteMarathonImportSnapshot(
    NSString *destinationDirectory,
    NSURL *sourceURL,
    NSData *mapData,
    NSData *resourceData,
    const PfhMapProbeResult *probe,
    PfhMapSourceEnvelopeKind envelopeKind,
    NSIndexSet *selectedOrdinals,
    NSArray<NSString *> *originalNames,
    NSArray<NSString *> *nativeNames,
    NSError **error)
{
    NSFileManager *manager = NSFileManager.defaultManager;
    NSString *importsDirectory =
        [destinationDirectory stringByAppendingPathComponent:@".pfhorge-imports"];
    if (![manager createDirectoryAtPath:importsDirectory
             withIntermediateDirectories:YES
                              attributes:nil
                                   error:error]) {
        return NO;
    }

    NSString *snapshotName = [NSString stringWithFormat:@"%@-%@",
        @((long long)(NSDate.date.timeIntervalSince1970)),
        NSUUID.UUID.UUIDString];
    NSString *snapshotDirectory =
        [importsDirectory stringByAppendingPathComponent:snapshotName];

    if (![manager createDirectoryAtPath:snapshotDirectory
             withIntermediateDirectories:NO
                              attributes:nil
                                   error:error]) {
        return NO;
    }

    NSString *mapPath =
        [snapshotDirectory stringByAppendingPathComponent:@"source-data-fork.bin"];
    if (![mapData writeToFile:mapPath options:NSDataWritingAtomic error:error]) {
        [manager removeItemAtPath:snapshotDirectory error:NULL];
        return NO;
    }

    if (resourceData.length > 0U) {
        NSString *resourcePath = [snapshotDirectory
            stringByAppendingPathComponent:@"source-resource-fork.bin"];
        if (![resourceData writeToFile:resourcePath
                               options:NSDataWritingAtomic
                                 error:error]) {
            [manager removeItemAtPath:snapshotDirectory error:NULL];
            return NO;
        }
    }

    NSMutableArray *levels = [NSMutableArray array];
    __block NSUInteger selectedPosition = 0U;
    [selectedOrdinals enumerateIndexesUsingBlock:
        ^(NSUInteger ordinal, BOOL *stop) {
            (void)stop;
            const PfhMapDirectoryEntrySummary *entry =
                ordinal < probe->directoryEntryCount
                    ? &probe->directoryEntries[ordinal]
                    : NULL;
            [levels addObject:@{
                @"directoryOrdinal": @(ordinal),
                @"logicalIndex": entry != NULL ? @(entry->logicalIndex) : @(-1),
                @"originalName": selectedPosition < originalNames.count
                    ? originalNames[selectedPosition]
                    : @"",
                @"nativeName": selectedPosition < nativeNames.count
                    ? nativeNames[selectedPosition]
                    : @"",
            }];
            ++selectedPosition;
        }];

    NSMutableOrderedSet<NSString *> *uniqueTags = [NSMutableOrderedSet orderedSet];
    for (size_t index = 0U; index < probe->tagCount; ++index) {
        [uniqueTags addObject:
            [NSString stringWithUTF8String:probe->tags[index].tag]];
    }

    NSMutableArray *lossLedger = [NSMutableArray array];
    for (NSString *tag in uniqueTags) {
        const BOOL known = PfhorgeKnownEditableMapTag(tag);
        [lossLedger addObject:@{
            @"tag": tag,
            @"recognized": @(known),
            @"decoded": @(known),
            @"editable": @(known),
            @"opaquePreserved": @YES,
            @"exportSafe": known ? @YES : @NO,
            @"possiblyInvalidatedByTopologyEdits": known ? @NO : @YES,
        }];
    }

    NSMutableArray *findings = [NSMutableArray array];
    for (size_t index = 0U; index < probe->findingCount; ++index) {
        const PfhMapFinding *finding = &probe->findings[index];
        [findings addObject:@{
            @"severity": [NSString stringWithUTF8String:
                PfhMapFindingSeverityName(finding->severity)],
            @"message": [NSString stringWithUTF8String:finding->message],
        }];
    }

    NSISO8601DateFormatter *dateFormatter =
        [[NSISO8601DateFormatter alloc] init];
    NSString *createdAt = [dateFormatter stringFromDate:[NSDate date]];

    NSDictionary *manifest = @{
        @"schema": @"org.pfhorge.import-snapshot.v1",
        @"createdAt": createdAt ?: @"",
        @"sourcePath": sourceURL.path ?: @"",
        @"sourceEnvelope": [NSString stringWithUTF8String:
            PfhMapEnvelopeKindName(envelopeKind)],
        @"mapKind": [NSString stringWithUTF8String:
            PfhMapMapKindName(probe->mapKind)],
        @"dialect": [NSString stringWithUTF8String:
            PfhMapDialectName(probe->dialect)],
        @"containerVersion": @(probe->header.containerVersion),
        @"dataVersion": @(probe->header.dataVersion),
        @"declaredChecksum": @(probe->header.declaredChecksum),
        @"computedChecksum": @(probe->computedChecksum),
        @"parentChecksum": @(probe->header.parentChecksum),
        @"dataForkSHA256": PfhorgeSHA256String(mapData),
        @"resourceForkSHA256": PfhorgeSHA256String(resourceData),
        @"selectedLevels": levels,
        @"lossLedger": lossLedger,
        @"findings": findings,
    };

    NSData *json = [NSJSONSerialization dataWithJSONObject:manifest
                                                   options:NSJSONWritingPrettyPrinted |
                                                           NSJSONWritingSortedKeys
                                                     error:error];
    if (json == nil) {
        [manager removeItemAtPath:snapshotDirectory error:NULL];
        return NO;
    }

    NSString *manifestPath =
        [snapshotDirectory stringByAppendingPathComponent:@"import-report.json"];
    if (![json writeToFile:manifestPath options:NSDataWritingAtomic error:error]) {
        [manager removeItemAtPath:snapshotDirectory error:NULL];
        return NO;
    }

    return YES;
}

static BOOL PfhorgeExtractRawScenarioResources(
    ScenarioResources *resources,
    NSString *imageDirectory,
    NSString *soundDirectory,
    PhProgress *progress,
    NSError **error)
{
    if (resources == nil) {
        return YES;
    }

    NSFileManager *manager = NSFileManager.defaultManager;
    BOOL isDirectory = NO;
    BOOL exists = [manager fileExistsAtPath:imageDirectory
                                isDirectory:&isDirectory];

    if (exists && !isDirectory) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                nil,
                @"A file named Images already exists in the scenario folder.");
        }
        return NO;
    }

    if (!exists &&
        ![manager createDirectoryAtPath:imageDirectory
             withIntermediateDirectories:YES
                              attributes:nil
                                   error:error]) {
        return NO;
    }

    isDirectory = NO;
    exists = [manager fileExistsAtPath:soundDirectory
                           isDirectory:&isDirectory];
    if (exists && !isDirectory) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                nil,
                @"A file named Sounds already exists in the scenario folder.");
        }
        return NO;
    }
    if (!exists &&
        ![manager createDirectoryAtPath:soundDirectory
             withIntermediateDirectories:YES
                              attributes:nil
                                   error:error]) {
        return NO;
    }

    __block NSError *firstError = nil;

    [resources iterateResourcesOfType:@"PICT"
                              progress:YES
                                 block:^(Resource *resource,
                                         NSData *data,
                                         PhProgress *resourceProgress) {
        if (firstError != nil) {
            return;
        }

        PhPictConversionBinaryFormat format;
        NSData *converted = [PhPictConversion convertPICTfromData:data
                                                   returnedFormat:&format
                                                           error:NULL];
        NSString *extension = nil;
        if (converted != nil) {
            switch (format) {
                case PhPictConversionBinaryFormatJPEG:
                    extension = @"jpeg";
                    break;
                case PhPictConversionBinaryFormatBitmap:
                    extension = @"bmp";
                    break;
                case PhPictConversionBinaryFormatPNG:
                    extension = @"png";
                    break;
                default:
                    break;
            }
        }

        NSData *output = converted;
        if (output == nil || extension == nil) {
            output = data;
            extension = @"pict";
        }

        NSString *path = [[imageDirectory
            stringByAppendingPathComponent:resource.resID.stringValue]
            stringByAppendingPathExtension:extension];
        [resourceProgress setInformationalText:
            [NSString stringWithFormat:@"Saving PICT resource %@…",
                resource.resID]];
        if (![output writeToFile:path options:NSDataWritingAtomic error:&firstError]) {
            return;
        }
    }];

    [resources iterateResourcesOfType:@"snd "
                              progress:YES
                                 block:^(Resource *resource,
                                         NSData *data,
                                         PhProgress *resourceProgress) {
        if (firstError != nil) {
            return;
        }
        NSString *path = [[soundDirectory
            stringByAppendingPathComponent:resource.resID.stringValue]
            stringByAppendingPathExtension:@"snd"];
        [resourceProgress setInformationalText:
            [NSString stringWithFormat:@"Saving sound resource %@…",
                resource.resID]];
        if (![data writeToFile:path options:NSDataWritingAtomic error:&firstError]) {
            return;
        }
    }];

    if (firstError != nil) {
        if (error != NULL) {
            *error = firstError;
        }
        return NO;
    }

    (void)progress;
    return YES;
}
