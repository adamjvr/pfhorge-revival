// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// FORMAT-3A direct-open helper.
// The including translation unit must already include
// PfhorgeMarathonMapIntakeBridge.inc and LEMapData.h.

#import <Cocoa/Cocoa.h>

#define PFHORGE_FORMAT_3A_UNIFIED_OPEN 1

static NSUInteger PFH3AChooseSingleMarathonLevel(
    const PfhMapProbeResult *probe,
    NSString *sourceDescription)
{
    if (probe == NULL || probe->directoryEntryCount == 0U) {
        return NSNotFound;
    }

    if (probe->directoryEntryCount == 1U) {
        return 0U;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(
        @"Choose a Level to Open",
        @"Direct-open multi-level Marathon map title");
    alert.informativeText = [NSString stringWithFormat:
        @"%@ contains %lu levels. File → Open creates one editable Pfhorge "
         "level document; choose the level to open. Scenario Import remains "
         "the multi-level workflow.",
        sourceDescription.length > 0U
            ? sourceDescription
            : @"This Marathon map",
        (unsigned long)probe->directoryEntryCount];
    alert.alertStyle = NSAlertStyleInformational;

    NSPopUpButton *popup =
        [[NSPopUpButton alloc]
            initWithFrame:NSMakeRect(0, 0, 520, 28)
                pullsDown:NO];

    for (NSUInteger ordinal = 0U;
         ordinal < probe->directoryEntryCount;
         ++ordinal) {

        const PfhMapDirectoryEntrySummary *entry =
            &probe->directoryEntries[ordinal];

        NSString *name = @"";
        if (entry->hasLevelName) {
            name = PfhorgeLegacyStringFromBytes(
                entry->levelName,
                sizeof(entry->levelName));
        }
        if (name.length == 0U) {
            name = [NSString stringWithFormat:
                @"Level %lu",
                (unsigned long)(ordinal + 1U)];
        }

        [popup addItemWithTitle:[NSString stringWithFormat:
            @"%@ — directory %lu, logical index %d",
            name,
            (unsigned long)entry->directoryOrdinal,
            entry->logicalIndex]];
    }

    alert.accessoryView = popup;
    [alert addButtonWithTitle:NSLocalizedString(
        @"Open Level",
        @"Direct-open selected Marathon level")];
    [alert addButtonWithTitle:NSLocalizedString(
        @"Cancel",
        @"Cancel direct-open Marathon level selection")];

    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return NSNotFound;
    }

    return (NSUInteger)popup.indexOfSelectedItem;
}

static LELevelData *PfhorgeOpenMarathonSourceURL(
    NSURL *sourceURL,
    NSError **outError)
{
    NSData *resolvedMapData = nil;
    NSData *resolvedResourceData = nil;
    PfhMapSourceEnvelopeKind envelopeKind =
        PfhMapSourceEnvelopeRaw;
    NSString *sourceDescription = nil;
    NSString *finderType = nil;
    NSError *intakeError = nil;

    if (!PfhorgeResolveMarathonSourceURL(
            sourceURL,
            &resolvedMapData,
            &resolvedResourceData,
            &envelopeKind,
            &sourceDescription,
            &finderType,
            &intakeError)) {

        if (outError != NULL) {
            *outError = intakeError;
        }
        return nil;
    }

    PfhMapProbeResult probe = {0};
    if (!PfhorgeProbeResolvedMapData(
            resolvedMapData,
            &probe,
            &intakeError)) {

        if (outError != NULL) {
            *outError = intakeError;
        }
        return nil;
    }

    probe.sourceEnvelopeKind = envelopeKind;

    NSUInteger ordinal =
        PFH3AChooseSingleMarathonLevel(
            &probe,
            sourceDescription);

    if (ordinal == NSNotFound) {
        if (outError != NULL) {
            *outError = [NSError
                errorWithDomain:NSCocoaErrorDomain
                           code:NSUserCancelledError
                       userInfo:@{
                           NSURLErrorKey: sourceURL,
                           NSLocalizedFailureReasonErrorKey:
                               @"Marathon level selection was cancelled."
                       }];
        }
        return nil;
    }

    NSMutableArray<NSString *> *names =
        [NSMutableArray array];

    NSArray<LELevelData *> *levels =
        [LEMapData
            convertMarathonDataToLevels:resolvedMapData
                              levelNames:names
                                   error:&intakeError];

    if (ordinal >= levels.count) {
        if (outError != NULL) {
            *outError = intakeError ?: [NSError
                errorWithDomain:NSCocoaErrorDomain
                           code:NSFileReadCorruptFileError
                       userInfo:@{
                           NSURLErrorKey: sourceURL,
                           NSLocalizedFailureReasonErrorKey:
                               @"The selected Marathon level could not be converted."
                       }];
        }
        return nil;
    }

    LELevelData *level = levels[ordinal];

    NSLog(
        @"FORMAT-3A direct open: source=%@ envelope=%ld "
         "finderType=%@ resourceBytes=%lu selected=%lu level=%@",
        sourceDescription ?: sourceURL.lastPathComponent,
        (long)envelopeKind,
        finderType ?: @"",
        (unsigned long)resolvedResourceData.length,
        (unsigned long)ordinal,
        level.levelName ?: @"");

    return level;
}
