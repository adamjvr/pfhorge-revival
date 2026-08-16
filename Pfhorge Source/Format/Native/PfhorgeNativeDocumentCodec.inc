// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// FORMAT-2A native .pfhlev transport bridge.
// Included by PhPfhorgeSingleLevelDoc.m to avoid unrelated PBX project churn.

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <CommonCrypto/CommonDigest.h>
#import <dispatch/dispatch.h>
#include <stdint.h>
#include <limits.h>
#import "LELevelData.h"

#define PFHORGE_FORMAT_2A_NATIVE_DOCUMENT_BRIDGE 1

static NSString * const PFN2AErrorDomain = @"org.pfhorge.format.native.format2a";
static NSString * const PFN2AMimeType = @"application/vnd.pfhorge.package+zip";
static NSString * const PFN2AFormatVersion = @"1.0.0-draft.1";
static NSString * const PFN2ABridgePath = @"bridge/level.archive";
static NSString * const PFN2AExtensionId = @"org.pfhorge.format2a.snapshot-authority";
static const void *PFN2AStableIdKey = &PFN2AStableIdKey;

static NSError *PFN2AError(NSInteger code, NSString *message)
{
    return [NSError errorWithDomain:PFN2AErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey: message ?: @"Pfhorge Native FORMAT-2A error"}];
}

static NSString *PFN2AId(id object)
{
    if (!object) return nil;
    NSString *value = objc_getAssociatedObject(object, PFN2AStableIdKey);
    if (!value) {
        value = NSUUID.UUID.UUIDString.lowercaseString;
        objc_setAssociatedObject(object, PFN2AStableIdKey, value, OBJC_ASSOCIATION_COPY_NONATOMIC);
    }
    return value;
}

static void PFN2ASetId(id object, id value)
{
    if (!object || ![value isKindOfClass:[NSString class]]) return;
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:value];
    if (uuid) objc_setAssociatedObject(object, PFN2AStableIdKey,
                                       uuid.UUIDString.lowercaseString,
                                       OBJC_ASSOCIATION_COPY_NONATOMIC);
}

static NSArray *PFN2AArray(id object, NSString *key)
{
    @try {
        id value = [object valueForKey:key];
        return [value isKindOfClass:[NSArray class]] ? value : @[];
    } @catch (__unused NSException *exception) {
        return @[];
    }
}

static NSData *PFN2AJSON(id object, NSError **outError)
{
    NSJSONWritingOptions options = NSJSONWritingPrettyPrinted;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
    options |= NSJSONWritingSortedKeys;
#endif
    NSData *raw = [NSJSONSerialization dataWithJSONObject:object options:options error:outError];
    if (!raw) return nil;
    NSMutableData *data = [raw mutableCopy];
    const uint8_t lf = '\n';
    [data appendBytes:&lf length:1];
    return data;
}

static NSString *PFN2ASHA256(NSData *data)
{
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
    NSMutableString *s = [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH * 2];
    for (NSUInteger i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) [s appendFormat:@"%02x", digest[i]];
    return s;
}

static uint32_t PFN2ACRC32(NSData *data)
{
    static uint32_t table[256];
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
            table[i] = c;
        }
    });
    uint32_t crc = 0xFFFFFFFFU;
    const uint8_t *p = data.bytes;
    for (NSUInteger i = 0; i < data.length; ++i)
        crc = table[(crc ^ p[i]) & 0xFFU] ^ (crc >> 8U);
    return crc ^ 0xFFFFFFFFU;
}

static void PFN2AU16(NSMutableData *d, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8U)};
    [d appendBytes:b length:2];
}

static void PFN2AU32(NSMutableData *d, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8U), (uint8_t)(v >> 16U), (uint8_t)(v >> 24U)};
    [d appendBytes:b length:4];
}

static uint16_t PFN2AR16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t PFN2AR32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static BOOL PFN2ASafePath(NSString *path)
{
    if (!path.length || [path hasPrefix:@"/"] || [path containsString:@"\\"]) return NO;
    for (NSString *part in [path componentsSeparatedByString:@"/"])
        if (!part.length || [part isEqual:@"."] || [part isEqual:@".."]) return NO;
    return YES;
}

static NSData *PFN2AWriteZip(NSArray<NSDictionary *> *entries, NSError **outError)
{
    NSMutableData *zip = [NSMutableData data];
    NSMutableArray *central = [NSMutableArray array];

    for (NSDictionary *entry in entries) {
        NSString *name = entry[@"name"];
        NSData *payload = entry[@"data"];
        NSData *nameData = [name dataUsingEncoding:NSUTF8StringEncoding];
        if (!PFN2ASafePath(name) || !payload || nameData.length > UINT16_MAX ||
            payload.length > UINT32_MAX || zip.length > UINT32_MAX) {
            if (outError) *outError = PFN2AError(2201, @"Invalid FORMAT-2A ZIP entry.");
            return nil;
        }

        uint32_t offset = (uint32_t)zip.length;
        uint32_t crc = PFN2ACRC32(payload);

        PFN2AU32(zip, 0x04034b50U);
        PFN2AU16(zip, 20); PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU32(zip, crc);
        PFN2AU32(zip, (uint32_t)payload.length);
        PFN2AU32(zip, (uint32_t)payload.length);
        PFN2AU16(zip, (uint16_t)nameData.length);
        PFN2AU16(zip, 0);
        [zip appendData:nameData];
        [zip appendData:payload];

        [central addObject:@{@"name":nameData, @"crc":@(crc),
                             @"size":@(payload.length), @"offset":@(offset)}];
    }

    uint32_t cdOffset = (uint32_t)zip.length;
    for (NSDictionary *entry in central) {
        NSData *nameData = entry[@"name"];
        uint32_t crc = [entry[@"crc"] unsignedIntValue];
        uint32_t size = [entry[@"size"] unsignedIntValue];
        uint32_t offset = [entry[@"offset"] unsignedIntValue];

        PFN2AU32(zip, 0x02014b50U);
        PFN2AU16(zip, 20); PFN2AU16(zip, 20);
        PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU32(zip, crc);
        PFN2AU32(zip, size); PFN2AU32(zip, size);
        PFN2AU16(zip, (uint16_t)nameData.length);
        PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU16(zip, 0); PFN2AU16(zip, 0);
        PFN2AU32(zip, 0); PFN2AU32(zip, offset);
        [zip appendData:nameData];
    }
    uint32_t cdSize = (uint32_t)zip.length - cdOffset;

    PFN2AU32(zip, 0x06054b50U);
    PFN2AU16(zip, 0); PFN2AU16(zip, 0);
    PFN2AU16(zip, (uint16_t)central.count); PFN2AU16(zip, (uint16_t)central.count);
    PFN2AU32(zip, cdSize); PFN2AU32(zip, cdOffset); PFN2AU16(zip, 0);
    return zip;
}

static NSDictionary<NSString *, NSData *> *PFN2AReadZip(NSData *zip, NSError **outError)
{
    const uint8_t *b = zip.bytes;
    NSUInteger n = zip.length;
    if (n < 22) {
        if (outError) *outError = PFN2AError(2210, @"Native package is truncated.");
        return nil;
    }

    NSUInteger lower = n > 65557 ? n - 65557 : 0;
    NSInteger eocd = -1;
    for (NSInteger i = (NSInteger)n - 22; i >= (NSInteger)lower; --i) {
        if (PFN2AR32(b + i) == 0x06054b50U) { eocd = i; break; }
    }
    if (eocd < 0) {
        if (outError) *outError = PFN2AError(2211, @"ZIP end directory not found.");
        return nil;
    }

    uint16_t count = PFN2AR16(b + eocd + 10);
    uint32_t cdSize = PFN2AR32(b + eocd + 12);
    uint32_t cdOffset = PFN2AR32(b + eocd + 16);
    if (count > 4096 || (uint64_t)cdOffset + cdSize > n) {
        if (outError) *outError = PFN2AError(2212, @"ZIP directory bounds are invalid.");
        return nil;
    }

    NSMutableDictionary *files = [NSMutableDictionary dictionary];
    NSUInteger p = cdOffset;
    for (uint16_t i = 0; i < count; ++i) {
        if (p + 46 > n || PFN2AR32(b + p) != 0x02014b50U) {
            if (outError) *outError = PFN2AError(2213, @"Invalid ZIP directory entry.");
            return nil;
        }
        uint16_t flags = PFN2AR16(b + p + 8);
        uint16_t method = PFN2AR16(b + p + 10);
        uint32_t crc = PFN2AR32(b + p + 16);
        uint32_t csize = PFN2AR32(b + p + 20);
        uint32_t usize = PFN2AR32(b + p + 24);
        uint16_t nameLen = PFN2AR16(b + p + 28);
        uint16_t extraLen = PFN2AR16(b + p + 30);
        uint16_t commentLen = PFN2AR16(b + p + 32);
        uint32_t local = PFN2AR32(b + p + 42);

        if ((flags & 1U) || method != 0 || p + 46U + nameLen + extraLen + commentLen > n) {
            if (outError) *outError = PFN2AError(
                2214, @"FORMAT-2A Cocoa reader accepts unencrypted ZIP Store entries only.");
            return nil;
        }

        NSString *name = [[NSString alloc] initWithBytes:b + p + 46
                                                  length:nameLen
                                                encoding:NSUTF8StringEncoding];
        if (!PFN2ASafePath(name) || files[name]) {
            if (outError) *outError = PFN2AError(2215, @"Unsafe or duplicate ZIP path.");
            return nil;
        }

        if (local + 30 > n || PFN2AR32(b + local) != 0x04034b50U) {
            if (outError) *outError = PFN2AError(2216, @"Invalid ZIP local header.");
            return nil;
        }
        uint16_t localName = PFN2AR16(b + local + 26);
        uint16_t localExtra = PFN2AR16(b + local + 28);
        uint64_t dataOffset = (uint64_t)local + 30U + localName + localExtra;
        if (dataOffset + csize > n || usize != csize || usize > 64U * 1024U * 1024U) {
            if (outError) *outError = PFN2AError(2217, @"ZIP resource bounds are invalid.");
            return nil;
        }

        NSData *payload = [zip subdataWithRange:NSMakeRange((NSUInteger)dataOffset, usize)];
        if (PFN2ACRC32(payload) != crc) {
            if (outError) *outError = PFN2AError(2218, @"ZIP CRC validation failed.");
            return nil;
        }
        files[name] = payload;
        p += 46U + nameLen + extraLen + commentLen;
    }
    return files;
}

static NSDictionary *PFN2ACounts(LELevelData *level)
{
    return @{
        @"points": @(PFN2AArray(level, @"points").count),
        @"lines": @(PFN2AArray(level, @"lines").count),
        @"polygons": @(PFN2AArray(level, @"polygons").count),
        @"objects": @(PFN2AArray(level, @"theMapObjects").count),
        @"sides": @(PFN2AArray(level, @"sides").count),
        @"lights": @(PFN2AArray(level, @"lights").count),
        @"annotations": @(PFN2AArray(level, @"notes").count),
        @"media": @(PFN2AArray(level, @"media").count),
        @"ambientSounds": @(PFN2AArray(level, @"ambientSounds").count),
        @"randomSounds": @(PFN2AArray(level, @"randomSounds").count),
        @"itemPlacements": @(PFN2AArray(level, @"itemPlacement").count),
        @"platforms": @(PFN2AArray(level, @"platforms").count),
        @"terminals": @(PFN2AArray(level, @"terimals").count),
        @"layers": @(PFN2AArray(level, @"layersInLevel").count),
        @"tags": @(PFN2AArray(level, @"tags").count),
        @"noteGroups": @(PFN2AArray(level, @"noteTypes").count),
    };
}

static NSDictionary *PFN2ALevelEnvelope(LELevelData *level)
{
    NSInteger env = level.environmentCode;
    id envJSON = (env >= 0 && env <= 4) ? @{@"classicCode": @(env)} : [NSNull null];

    return @{
        @"$schema": @"urn:pfhorge:schema:level:1",
        @"id": PFN2AId(level),
        @"name": level.levelName ?: @"Untitled Level",
        @"metadata": @{
            @"environment": envJSON,
            @"physicsModel": @(level.physicsModel),
            @"songIndex": @(level.songIndex),
            @"missionFlags": @((NSUInteger)level.missionFlags),
            @"environmentFlags": @((NSUInteger)level.environmentFlags),
            @"entryPointFlags": @((NSInteger)level.entryPointFlags),
        },
        @"geometry": @{@"points": @[], @"lines": @[], @"polygons": @[]},
        @"surfaces": @{@"sides": @[]},
        @"world": @{
            @"lights": @[], @"media": @[], @"platforms": @[], @"objects": @[],
            @"itemPlacements": @[], @"ambientSounds": @[], @"randomSounds": @[],
            @"tags": @[], @"annotations": @[],
        },
        @"terminals": @{@"items": @[]},
        @"editor": @{
            @"layers": @[], @"noteGroups": @[], @"names": @{}, @"lineOverrides": @[],
            @"currentLayer": [NSNull null], @"levelOptions": @{},
        },
        @"extensions": @{
            PFN2AExtensionId: @{
                @"canonicalAuthority": @NO,
                @"authoritativeResource": PFN2ABridgePath,
                @"objectCounts": PFN2ACounts(level),
                @"nextMilestone": @"FORMAT-2B",
            }
        },
        @"provenance": @{@"sources": @[], @"bindings": @[], @"opaqueFragments": @[]},
    };
}

static BOOL PfhorgeNative2ADataLooksLikePackage(NSData *data)
{
    if (data.length < 4) return NO;
    const uint8_t *b = data.bytes;
    return b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4;
}

static NSData *PfhorgeNative2ACreatePackage(id document, LELevelData *level, NSError **outError)
{
    NSError *error = nil;
    NSData *bridge = [NSKeyedArchiver archivedDataWithRootObject:level
                                          requiringSecureCoding:YES
                                                          error:&error];
    if (!bridge) {
        if (outError) *outError = error ?: PFN2AError(2220, @"Could not archive FORMAT-2A bridge payload.");
        return nil;
    }

    NSDictionary *levelJSON = PFN2ALevelEnvelope(level);
    NSData *levelData = PFN2AJSON(levelJSON, &error);
    if (!levelData) { if (outError) *outError = error; return nil; }

    NSString *docId = PFN2AId(document);
    NSString *levelId = PFN2AId(level);
    NSString *levelPath = [NSString stringWithFormat:@"levels/%@.json", levelId];

    NSDictionary *documentJSON = @{
        @"$schema": @"urn:pfhorge:schema:document:1",
        @"id": docId,
        @"kind": @"level",
        @"title": level.levelName ?: @"Untitled Level",
        @"levels": @[@{@"id":levelId, @"path":levelPath, @"name":level.levelName ?: @"Untitled Level"}],
        @"extensions": @[PFN2AExtensionId],
        @"metadata": @{@"writer": @"Pfhorge FORMAT-2A"},
    };
    NSData *documentData = PFN2AJSON(documentJSON, &error);
    if (!documentData) { if (outError) *outError = error; return nil; }

    NSDictionary *ext = @{
        @"id": PFN2AExtensionId,
        @"version": @"1.0",
        @"requiredForRead": @YES,
        @"requiredForWrite": @YES,
        @"resources": @[PFN2ABridgePath],
    };

    NSDictionary *manifestJSON = @{
        @"$schema": @"urn:pfhorge:schema:manifest:1",
        @"format": @"org.pfhorge.native",
        @"formatVersion": PFN2AFormatVersion,
        @"kind": @"level",
        @"document": @"document.json",
        @"extensions": @[ext],
        @"resources": @[
            @{@"path":@"document.json", @"mediaType":@"application/json",
              @"role":@"document", @"sha256":PFN2ASHA256(documentData)},
            @{@"path":levelPath, @"mediaType":@"application/json",
              @"role":@"level-envelope", @"sha256":PFN2ASHA256(levelData)},
            @{@"path":PFN2ABridgePath, @"mediaType":@"application/x-pfhorge-keyed-archive",
              @"role":@"format2a-authoritative-bridge", @"sha256":PFN2ASHA256(bridge)},
        ],
    };
    NSData *manifestData = PFN2AJSON(manifestJSON, &error);
    if (!manifestData) { if (outError) *outError = error; return nil; }

    NSData *mime = [PFN2AMimeType dataUsingEncoding:NSASCIIStringEncoding];
    return PFN2AWriteZip(@[
        @{@"name":@"mimetype", @"data":mime},
        @{@"name":@"manifest.json", @"data":manifestData},
        @{@"name":@"document.json", @"data":documentData},
        @{@"name":levelPath, @"data":levelData},
        @{@"name":PFN2ABridgePath, @"data":bridge},
    ], outError);
}

static LELevelData *PfhorgeNative2ALoadLevel(NSData *data, id document, NSError **outError)
{
    NSError *error = nil;
    NSDictionary<NSString *,NSData *> *files = PFN2AReadZip(data, &error);
    if (!files) { if (outError) *outError = error; return nil; }

    NSString *mime = [[NSString alloc] initWithData:files[@"mimetype"] encoding:NSASCIIStringEncoding];
    if (![mime isEqual:PFN2AMimeType]) {
        if (outError) *outError = PFN2AError(2230, @"ZIP is not a Pfhorge Native document.");
        return nil;
    }

    NSDictionary *manifest = [NSJSONSerialization JSONObjectWithData:files[@"manifest.json"] ?: [NSData data]
                                                              options:0 error:&error];
    if (![manifest isKindOfClass:[NSDictionary class]] ||
        ![manifest[@"format"] isEqual:@"org.pfhorge.native"] ||
        ![manifest[@"kind"] isEqual:@"level"]) {
        if (outError) *outError = error ?: PFN2AError(2231, @"Invalid Pfhorge Native manifest.");
        return nil;
    }

    BOOL hasBridgeDeclaration = NO;
    for (NSDictionary *ext in manifest[@"extensions"] ?: @[]) {
        if ([ext[@"id"] isEqual:PFN2AExtensionId] &&
            [ext[@"requiredForRead"] boolValue] &&
            [ext[@"resources"] containsObject:PFN2ABridgePath]) {
            hasBridgeDeclaration = YES;
            break;
        }
    }
    if (!hasBridgeDeclaration) {
        if (outError) *outError = PFN2AError(
            2232, @"This package is not a FORMAT-2A snapshot-authority document. Direct canonical read arrives in FORMAT-2B.");
        return nil;
    }

    NSData *bridge = files[PFN2ABridgePath];
    if (!bridge) {
        if (outError) *outError = PFN2AError(2233, @"Required FORMAT-2A bridge resource is missing.");
        return nil;
    }

    // Verify every manifest resource hash before decoding the bridge.
    for (NSDictionary *resource in manifest[@"resources"] ?: @[]) {
        NSString *path = resource[@"path"];
        NSString *expected = resource[@"sha256"];
        NSData *resourceData = files[path];
        if (!resourceData || ![PFN2ASHA256(resourceData) isEqual:expected]) {
            if (outError) *outError = PFN2AError(2234, [NSString stringWithFormat:@"Resource hash mismatch: %@", path]);
            return nil;
        }
    }

    LELevelData *level = [NSKeyedUnarchiver unarchivedObjectOfClass:[LELevelData class]
                                                           fromData:bridge
                                                              error:&error];
    if (!level) {
        if (outError) *outError = error ?: PFN2AError(2235, @"Could not decode FORMAT-2A bridge resource.");
        return nil;
    }

    NSString *documentPath = manifest[@"document"];
    NSDictionary *documentJSON = [NSJSONSerialization JSONObjectWithData:files[documentPath] ?: [NSData data]
                                                                  options:0 error:NULL];
    PFN2ASetId(document, documentJSON[@"id"]);
    NSArray *levelRefs = documentJSON[@"levels"];
    if ([levelRefs isKindOfClass:[NSArray class]] && levelRefs.count)
        PFN2ASetId(level, levelRefs[0][@"id"]);

    return level;
}
