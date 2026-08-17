// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// FORMAT-3A transitional level persistence service.
// The including translation unit must already include
// PfhorgeNativeDocumentCodec.inc.

#import <Foundation/Foundation.h>
#import "LELevelData.h"

#define PFHORGE_FORMAT_3A_LEVEL_PERSISTENCE_BRIDGE 1

static NSError *PFH3APersistenceError(
    NSInteger code,
    NSString *reason)
{
    return [NSError
        errorWithDomain:@"org.pfhorge.format.persistence"
                   code:code
               userInfo:@{
                   NSLocalizedFailureReasonErrorKey:
                       reason ?: @"Pfhorge level persistence error."
               }];
}

static BOOL PfhorgeDataLooksLikeLegacyPfhlev(NSData *data)
{
    if (data.length < 10U) {
        return NO;
    }

    const uint8_t *bytes = data.bytes;
    const uint16_t sig1 =
        ((uint16_t)bytes[2] << 8U) | bytes[3];
    const uint16_t sig2 =
        ((uint16_t)bytes[4] << 8U) | bytes[5];
    const uint32_t sig3 =
        ((uint32_t)bytes[6] << 24U) |
        ((uint32_t)bytes[7] << 16U) |
        ((uint32_t)bytes[8] << 8U) |
        (uint32_t)bytes[9];

    return sig1 == 26743U &&
           sig2 == 34521U &&
           sig3 == 42296737U;
}

static LELevelData *PfhorgeLevelFromLegacyPfhlevData(
    NSData *data,
    NSError **outError)
{
    if (!PfhorgeDataLooksLikeLegacyPfhlev(data)) {
        if (outError != NULL) {
            *outError = PFH3APersistenceError(
                3101,
                @"The input is not a recognized legacy Pfhorge level.");
        }
        return nil;
    }

    NSData *archive =
        [data subdataWithRange:
            NSMakeRange(10U, data.length - 10U)];

    NSError *decodeError = nil;
    LELevelData *level =
        [NSKeyedUnarchiver
            unarchivedObjectOfClass:[LELevelData class]
                           fromData:archive
                              error:&decodeError];

    if (level == nil) {
        if (outError != NULL) {
            *outError = decodeError ?: PFH3APersistenceError(
                3102,
                @"The legacy Pfhorge archive could not be decoded.");
        }
        return nil;
    }

    [level updateCounts];
    [level setUpArrayPointersForEveryObject];
    [level setupDefaultObjects];

    return level;
}

static LELevelData *PfhorgeLevelFromAnyPfhlevData(
    NSData *data,
    NSError **outError)
{
    if (PfhorgeNative2ADataLooksLikePackage(data)) {
        return PfhorgeNative2ALoadLevel(
            data,
            nil,
            outError);
    }

    if (PfhorgeDataLooksLikeLegacyPfhlev(data)) {
        return PfhorgeLevelFromLegacyPfhlevData(
            data,
            outError);
    }

    if (outError != NULL) {
        *outError = PFH3APersistenceError(
            3103,
            @"The input is neither Pfhorge Native nor a supported "
             "legacy Pfhorge level.");
    }

    return nil;
}

static NSData *PfhorgeNativePackageForLevel(
    LELevelData *level,
    NSError **outError)
{
    if (level == nil) {
        if (outError != NULL) {
            *outError = PFH3APersistenceError(
                3104,
                @"Cannot persist a nil level.");
        }
        return nil;
    }

    NSObject *documentIdentity =
        [[NSObject alloc] init];

    return PfhorgeNative2ACreatePackage(
        documentIdentity,
        level,
        outError);
}

static NSData *PfhorgeNativePackageFromLegacyPfhlevData(
    NSData *legacyData,
    NSError **outError)
{
    NSError *decodeError = nil;
    LELevelData *level =
        PfhorgeLevelFromLegacyPfhlevData(
            legacyData,
            &decodeError);

    if (level == nil) {
        if (outError != NULL) {
            *outError = decodeError;
        }
        return nil;
    }

    return PfhorgeNativePackageForLevel(
        level,
        outError);
}
