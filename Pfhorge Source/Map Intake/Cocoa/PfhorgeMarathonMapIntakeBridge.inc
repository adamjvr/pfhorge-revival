// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#import <Cocoa/Cocoa.h>

#include "../Core/MarathonMapProbe.h"

// FORMAT-3A-LINK-1: this Cocoa bridge is included by more than one
// Objective-C translation unit. The core probe implementation must have
// exactly one external-linkage owner; secondary users request declarations
// only before including this bridge.
#ifndef PFHORGE_MARATHON_MAP_PROBE_DECLARATIONS_ONLY
#include "../Core/MarathonMapProbe.inc"
#endif

static NSString *PfhorgeLegacyStringFromBytes(
    const uint8_t *bytes,
    NSUInteger maximumLength)
{
    NSUInteger length = 0;
    while (length < maximumLength && bytes[length] != 0) {
        ++length;
    }

    if (length == 0) {
        return @"";
    }

    NSString *string =
        [[NSString alloc]
            initWithBytes:bytes
                   length:length
                 encoding:NSMacOSRomanStringEncoding];

    if (string == nil) {
        string =
            [[NSString alloc]
                initWithBytes:bytes
                       length:length
                     encoding:NSISOLatin1StringEncoding];
    }

    return string ?: @"";
}

static NSData *PfhorgeDataCopyFromMapView(PfhMapByteView view)
{
    if (view.bytes == NULL || view.length == 0) {
        return nil;
    }

    return [NSData dataWithBytes:view.bytes length:view.length];
}

static NSError *PfhorgeMapIntakeError(
    NSURL *url,
    NSString *reason)
{
    NSMutableDictionary *userInfo =
        [NSMutableDictionary dictionaryWithObject:reason
                                           forKey:NSLocalizedFailureReasonErrorKey];

    if (url != nil) {
        userInfo[NSURLErrorKey] = url;
    }

    return [NSError errorWithDomain:@"org.pfhorge.MapIntake"
                               code:1
                           userInfo:userInfo];
}

static NSURL *PfhorgeMainURLForAppleDoubleSidecar(NSURL *sidecarURL)
{
    NSString *name = sidecarURL.lastPathComponent;
    if (![name hasPrefix:@"._"] || name.length <= 2) {
        return nil;
    }

    NSString *mainName = [name substringFromIndex:2];
    return [[sidecarURL URLByDeletingLastPathComponent]
        URLByAppendingPathComponent:mainName];
}

static NSURL *PfhorgeAppleDoubleSidecarURLForMainURL(NSURL *mainURL)
{
    NSString *sidecarName =
        [@"._" stringByAppendingString:mainURL.lastPathComponent];

    return [[mainURL URLByDeletingLastPathComponent]
        URLByAppendingPathComponent:sidecarName];
}

static BOOL PfhorgeParseEnvelopeData(
    NSData *sourceData,
    PfhMapResolvedForks *resolved,
    NSError **error)
{
    PfhMapByteView sourceView = {
        .bytes = sourceData.bytes,
        .length = sourceData.length,
    };

    if (!PfhMapResolveContainedForks(sourceView, resolved)) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                nil,
                @"The classic Mac container is malformed or truncated.");
        }
        return NO;
    }

    return YES;
}

/**
 * Resolve the selected file into a Marathon data fork and optional resource
 * fork. The returned NSData objects own copies of any slices from an outer
 * AppleSingle, AppleDouble, or MacBinary container.
 */
static BOOL PfhorgeResolveMarathonSourceURL(
    NSURL *selectedURL,
    NSData **mapData,
    NSData **resourceData,
    PfhMapSourceEnvelopeKind *envelopeKind,
    NSString **sourceDescription,
    NSString **finderType,
    NSError **error)
{
    NSData *selectedData =
        [NSData dataWithContentsOfURL:selectedURL
                              options:NSDataReadingMappedIfSafe
                                error:error];

    if (selectedData == nil) {
        return NO;
    }

    PfhMapResolvedForks resolved = {0};
    if (!PfhorgeParseEnvelopeData(selectedData, &resolved, error)) {
        return NO;
    }

    NSURL *mainURL = selectedURL;
    NSData *mainData = selectedData;
    NSData *resolvedResourceData = nil;
    NSData *resolvedFinderInfo = nil;
    PfhMapSourceEnvelopeKind resolvedKind = resolved.envelopeKind;

    if (resolved.envelopeKind == PfhMapSourceEnvelopeAppleDouble &&
        resolved.dataFork.length == 0) {
        mainURL = PfhorgeMainURLForAppleDoubleSidecar(selectedURL);
        if (mainURL == nil) {
            if (error != NULL) {
                *error = PfhorgeMapIntakeError(
                    selectedURL,
                    @"AppleDouble sidecar name does not identify its data file.");
            }
            return NO;
        }

        mainData =
            [NSData dataWithContentsOfURL:mainURL
                                  options:NSDataReadingMappedIfSafe
                                    error:error];
        if (mainData == nil) {
            return NO;
        }

        resolvedResourceData = PfhorgeDataCopyFromMapView(resolved.resourceFork);
        resolvedFinderInfo = PfhorgeDataCopyFromMapView(resolved.finderInfo);
    } else if (resolved.envelopeKind == PfhMapSourceEnvelopeRaw) {
        NSURL *sidecarURL = PfhorgeAppleDoubleSidecarURLForMainURL(selectedURL);
        NSData *sidecarData =
            [NSData dataWithContentsOfURL:sidecarURL
                                  options:NSDataReadingMappedIfSafe
                                    error:NULL];

        if (sidecarData != nil) {
            PfhMapResolvedForks sidecarForks = {0};
            if (PfhorgeParseEnvelopeData(sidecarData, &sidecarForks, NULL) &&
                sidecarForks.envelopeKind == PfhMapSourceEnvelopeAppleDouble) {
                resolvedKind = PfhMapSourceEnvelopeAppleDouble;
                resolvedResourceData =
                    PfhorgeDataCopyFromMapView(sidecarForks.resourceFork);
                resolvedFinderInfo =
                    PfhorgeDataCopyFromMapView(sidecarForks.finderInfo);
            }
        }

        if (resolvedResourceData == nil) {
            NSString *namedForkPath =
                [selectedURL.path stringByAppendingPathComponent:@"..namedfork/rsrc"];
            resolvedResourceData = [NSData dataWithContentsOfFile:namedForkPath];
        }
    } else {
        mainData = PfhorgeDataCopyFromMapView(resolved.dataFork);
        resolvedResourceData = PfhorgeDataCopyFromMapView(resolved.resourceFork);
        resolvedFinderInfo = PfhorgeDataCopyFromMapView(resolved.finderInfo);
    }

    if (resolved.envelopeKind == PfhMapSourceEnvelopeRaw) {
        *mapData = mainData;
    } else if (resolved.envelopeKind == PfhMapSourceEnvelopeAppleDouble &&
               resolved.dataFork.length == 0) {
        *mapData = mainData;
    } else {
        *mapData = mainData;
    }

    if (*mapData == nil || (*mapData).length == 0) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                selectedURL,
                @"The selected source contains no readable data fork.");
        }
        return NO;
    }

    if (resourceData != NULL) {
        *resourceData = resolvedResourceData;
    }

    if (envelopeKind != NULL) {
        *envelopeKind = resolvedKind;
    }

    if (sourceDescription != NULL) {
        *sourceDescription =
            [NSString stringWithUTF8String:PfhMapEnvelopeKindName(resolvedKind)];
    }

    if (finderType != NULL) {
        NSString *type = @"";
        if (resolvedFinderInfo.length >= 4) {
            type =
                [[NSString alloc]
                    initWithBytes:resolvedFinderInfo.bytes
                           length:4
                         encoding:NSMacOSRomanStringEncoding] ?: @"";
        }
        *finderType = type;
    }

    return YES;
}

static BOOL PfhorgeProbeResolvedMapData(
    NSData *mapData,
    PfhMapProbeResult *probe,
    NSError **error)
{
    PfhMapByteView view = {
        .bytes = mapData.bytes,
        .length = mapData.length,
    };

    const BOOL succeeded = PfhMapProbeDataFork(view, probe);

    if (!probe->recognizedMarathonContainer) {
        if (error != NULL) {
            NSString *reason = @"The selected file is not a recognized Marathon map container.";
            if (probe->findingCount > 0) {
                reason = [NSString stringWithUTF8String:probe->findings[0].message];
            }
            *error = PfhorgeMapIntakeError(nil, reason);
        }
        return NO;
    }

    if (!succeeded || !probe->structurallyUsable) {
        if (error != NULL) {
            NSString *reason = @"The Marathon map container is damaged.";
            for (size_t index = 0; index < probe->findingCount; ++index) {
                if (probe->findings[index].severity == PfhMapFindingFatal) {
                    reason =
                        [NSString stringWithUTF8String:probe->findings[index].message];
                    break;
                }
            }
            *error = PfhorgeMapIntakeError(nil, reason);
        }
        return NO;
    }

    if (probe->contentKind != PfhMapContentMap) {
        if (error != NULL) {
            *error = PfhorgeMapIntakeError(
                nil,
                @"The selected Marathon container does not contain a complete map geometry tag set.");
        }
        return NO;
    }

    return YES;
}

static NSString *PfhorgeMapIdentificationText(
    const PfhMapProbeResult *probe,
    NSString *sourceDescription,
    NSString *finderType,
    NSUInteger resourceForkLength)
{
    NSMutableString *text = [NSMutableString string];
    NSString *internalName =
        PfhorgeLegacyStringFromBytes(
            probe->header.internalName,
            sizeof(probe->header.internalName));

    [text appendFormat:@"Detected dialect: %@\n",
        [NSString stringWithUTF8String:PfhMapDialectName(probe->dialect)]];
    [text appendFormat:@"Content: %@\n",
        [NSString stringWithUTF8String:PfhMapMapKindName(probe->mapKind)]];
    [text appendFormat:@"Source envelope: %@\n", sourceDescription ?: @"Unknown"];
    [text appendFormat:@"Marathon container: version %d\n",
        probe->header.containerVersion];
    [text appendFormat:@"Map data version: %d\n", probe->header.dataVersion];
    [text appendFormat:@"Entries/levels: %d\n", probe->header.entryCount];

    if (internalName.length > 0) {
        [text appendFormat:@"Internal name: %@\n", internalName];
    }

    if (finderType.length > 0) {
        [text appendFormat:@"Classic Finder type: %@\n", finderType];
    }

    [text appendFormat:@"Checksum: %@\n",
        [NSString stringWithUTF8String:
            PfhMapChecksumStatusName(probe->checksumStatus)]];

    if (probe->header.parentChecksum != 0) {
        [text appendFormat:@"Parent checksum: 0x%08X\n",
            probe->header.parentChecksum];
    }

    [text appendFormat:@"Resource fork: %@",
        resourceForkLength > 0
            ? [NSString stringWithFormat:@"found (%lu bytes)",
                (unsigned long)resourceForkLength]
            : @"not found"];

    if (probe->dialect == PfhMapDialectMarathon1) {
        [text appendString:
            @"\n\nImport status: Marathon 1 structural identification is supported; "
             "semantic conversion remains experimental in MAP-1A."];
    } else if (probe->dialect == PfhMapDialectAlephOneExtended) {
        [text appendString:
            @"\n\nImport status: Aleph One extended data is structurally recognized; "
             "semantic conversion may be partial."];
    }

    if (resourceForkLength > 0 &&
        ![sourceDescription isEqualToString:@"Raw data fork"]) {
        [text appendString:
            @"\nResource status: preserved for inspection; full resource decoding "
             "is scheduled for the content/resource phase."];
    }

    NSMutableOrderedSet<NSString *> *levelNames = [NSMutableOrderedSet orderedSet];
    for (size_t index = 0; index < probe->directoryEntryCount; ++index) {
        const PfhMapDirectoryEntrySummary *entry = &probe->directoryEntries[index];
        if (entry->hasLevelName) {
            NSString *name =
                PfhorgeLegacyStringFromBytes(
                    entry->levelName,
                    sizeof(entry->levelName));
            if (name.length > 0) {
                [levelNames addObject:name];
            }
        }
    }

    if (levelNames.count > 0) {
        [text appendString:@"\n\nLevel names:\n"];
        NSUInteger count = MIN(levelNames.count, 12);
        for (NSUInteger index = 0; index < count; ++index) {
            [text appendFormat:@"  • %@\n", levelNames[index]];
        }
        if (levelNames.count > count) {
            [text appendFormat:@"  • …and %lu more\n",
                (unsigned long)(levelNames.count - count)];
        }
        [text deleteCharactersInRange:NSMakeRange(text.length - 1, 1)];
    }

    NSMutableOrderedSet<NSString *> *tags = [NSMutableOrderedSet orderedSet];
    for (size_t index = 0; index < probe->tagCount; ++index) {
        [tags addObject:[NSString stringWithUTF8String:probe->tags[index].tag]];
    }

    if (tags.count > 0) {
        [text appendFormat:@"\n\nMap chunks: %@",
            [[tags array] componentsJoinedByString:@", "]];
    }

    if (probe->findingCount > 0) {
        [text appendString:@"\n\nFindings:"];
        const size_t maximum = MIN(probe->findingCount, (size_t)10);
        for (size_t index = 0; index < maximum; ++index) {
            const PfhMapFinding *finding = &probe->findings[index];
            [text appendFormat:@"\n  %@: %@",
                [NSString stringWithUTF8String:
                    PfhMapFindingSeverityName(finding->severity)],
                [NSString stringWithUTF8String:finding->message]];
        }
        if (probe->findingCount > maximum) {
            [text appendFormat:@"\n  …and %zu more findings", probe->findingCount - maximum];
        }
    }

    return text;
}

static BOOL PfhorgeConfirmMapIdentification(
    NSWindow *parentWindow,
    const PfhMapProbeResult *probe,
    NSString *sourceDescription,
    NSString *finderType,
    NSUInteger resourceForkLength)
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(
        @"Marathon Map Identified",
        @"Marathon map identification confirmation title");
    alert.informativeText =
        PfhorgeMapIdentificationText(
            probe,
            sourceDescription,
            finderType,
            resourceForkLength);
    alert.alertStyle = NSAlertStyleInformational;
    NSString *importTitle =
        (probe->dialect == PfhMapDialectMarathon1 ||
         probe->dialect == PfhMapDialectAlephOneExtended)
            ? NSLocalizedString(@"Import Experimentally", @"Experimental map import button")
            : NSLocalizedString(@"Import", @"Import map button");
    [alert addButtonWithTitle:importTitle];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"Cancel map import button")];

    (void)parentWindow;
    return [alert runModal] == NSAlertFirstButtonReturn;
}
