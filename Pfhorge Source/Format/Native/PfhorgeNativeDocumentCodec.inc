// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// Pfhorge Native .pfhlev codec — FORMAT-3B canonical authority; 2A/3A migration retained.
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
static NSString * const PFN2AFormatVersion = @"1.1.0-draft.1";
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

#include "PfhorgeCanonicalLevelBridge.inc"

static NSDictionary *PFN3BParseJSONResource(NSDictionary<NSString *, NSData *> *files,
                                            NSString *path,
                                            NSError **outError)
{
    NSData *data = files[path];
    if (!data) {
        if (outError) *outError = PFN2AError(3330, [NSString stringWithFormat:@"Missing native resource: %@", path]);
        return nil;
    }
    NSError *error = nil;
    id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (![object isKindOfClass:NSDictionary.class]) {
        if (outError) *outError = error ?: PFN2AError(3331, [NSString stringWithFormat:@"Invalid JSON resource: %@", path]);
        return nil;
    }
    return object;
}

static BOOL PFN3BVerifyManifestResources(NSDictionary *manifest,
                                         NSDictionary<NSString *, NSData *> *files,
                                         NSError **outError)
{
    for (id raw in manifest[@"resources"] ?: @[]) {
        if (![raw isKindOfClass:NSDictionary.class]) {
            if (outError) *outError = PFN2AError(3332, @"Manifest contains an invalid resource record.");
            return NO;
        }
        NSString *path = raw[@"path"];
        NSString *expected = raw[@"sha256"];
        NSData *resource = [path isKindOfClass:NSString.class] ? files[path] : nil;
        if (!resource || ![expected isKindOfClass:NSString.class] ||
            ![PFN2ASHA256(resource) isEqualToString:expected]) {
            if (outError) *outError = PFN2AError(3333, [NSString stringWithFormat:@"Resource hash mismatch: %@", path ?: @"<unknown>"]);
            return NO;
        }
    }
    return YES;
}

static BOOL PfhorgeNative2ADataLooksLikePackage(NSData *data)
{
    if (data.length < 4) return NO;
    const uint8_t *b = data.bytes;
    return b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4;
}

static BOOL PFN3BValidateWrittenPackage(NSData *package, NSError **outError)
{
    NSError *error = nil;
    NSDictionary<NSString *,NSData *> *files = PFN2AReadZip(package, &error);
    if (!files) { if (outError) *outError = error; return NO; }
    NSString *mime = [[NSString alloc] initWithData:files[@"mimetype"] encoding:NSASCIIStringEncoding];
    if (![mime isEqualToString:PFN2AMimeType]) {
        if (outError) *outError = PFN2AError(3334, @"Native read-back found an invalid mimetype.");
        return NO;
    }
    NSDictionary *manifest = PFN3BParseJSONResource(files, @"manifest.json", &error);
    if (!manifest || !PFN3BVerifyManifestResources(manifest, files, &error)) {
        if (outError) *outError = error; return NO;
    }
    NSDictionary *documentJSON = PFN3BParseJSONResource(files, manifest[@"document"], &error);
    NSArray *levels = [documentJSON[@"levels"] isKindOfClass:NSArray.class] ? documentJSON[@"levels"] : nil;
    NSString *levelPath = levels.count && [levels[0] isKindOfClass:NSDictionary.class] ? levels[0][@"path"] : nil;
    NSDictionary *levelJSON = PFN3BParseJSONResource(files, levelPath, &error);
    if (!levelJSON || !PFN3BValidateCanonicalLevelJSON(levelJSON, &error)) {
        if (outError) *outError = error; return NO;
    }

    // Exercise the canonical reader against the exact bytes just written. This
    // is deliberately more than a JSON parse: it catches Cocoa materialization
    // regressions before a Save is reported as successful.
    LELevelData *roundTripLevel = PFN3BLevelFromCanonicalJSON(levelJSON, &error);
    if (!roundTripLevel) {
        if (outError) *outError = error ?: PFN2AError(3335, @"FORMAT-3B read-back could not materialize LELevelData.");
        return NO;
    }
    NSDictionary *authority = levelJSON[@"extensions"][PFN3BCanonicalAuthorityExtensionId];
    NSDictionary *expectedCounts = [authority[@"objectCounts"] isKindOfClass:NSDictionary.class]
        ? authority[@"objectCounts"] : @{};
    if (![PFN3BCountsFromLevel(roundTripLevel) isEqualToDictionary:expectedCounts]) {
        if (outError) *outError = PFN2AError(3336, @"FORMAT-3B read-back object counts changed during materialization.");
        return NO;
    }
    if (files[PFN2ABridgePath]) {
        if (outError) *outError = PFN2AError(3337, @"FORMAT-3B writer unexpectedly emitted the legacy bridge resource.");
        return NO;
    }
    return YES;
}

static NSData *PfhorgeNative2ACreatePackage(id document, LELevelData *level, NSError **outError)
{
    NSError *error = nil;
    NSDictionary *levelJSON = PFN3BCanonicalLevelJSON(level, &error);
    if (!levelJSON || !PFN3BValidateCanonicalLevelJSON(levelJSON, &error)) {
        if (outError) *outError = error ?: PFN2AError(3340, @"Canonical graph validation failed before save.");
        return nil;
    }
    NSData *levelData = PFN2AJSON(levelJSON, &error);
    if (!levelData) { if (outError) *outError = error; return nil; }

    NSString *docId = PFN2AId(document ?: level);
    NSString *levelId = PFN2AId(level);
    NSString *levelPath = [NSString stringWithFormat:@"levels/%@.json", levelId];
    NSDictionary *documentJSON = @{
        @"$schema": @"urn:pfhorge:schema:document:1",
        @"id": docId,
        @"kind": @"level",
        @"title": level.levelName ?: @"Untitled Level",
        @"levels": @[@{@"id":levelId, @"path":levelPath, @"name":level.levelName ?: @"Untitled Level"}],
        @"extensions": @[PFN3BCanonicalAuthorityExtensionId],
        @"metadata": @{@"writer":@"Pfhorge FORMAT-3B", @"canonicalModelRevision":@(PFN3BCanonicalModelRevision)},
    };
    NSData *documentData = PFN2AJSON(documentJSON, &error);
    if (!documentData) { if (outError) *outError = error; return nil; }

    NSDictionary *manifestJSON = @{
        @"$schema": @"urn:pfhorge:schema:manifest:1",
        @"format": @"org.pfhorge.native",
        @"formatVersion": PFN2AFormatVersion,
        @"kind": @"level",
        @"document": @"document.json",
        @"extensions": @[@{
            @"id": PFN3BCanonicalAuthorityExtensionId,
            @"version": @"1.0",
            @"requiredForRead": @NO,
            @"requiredForWrite": @NO,
            @"resources": @[]
        }],
        @"resources": @[
            @{@"path":@"document.json", @"mediaType":@"application/json",
              @"role":@"document", @"sha256":PFN2ASHA256(documentData)},
            @{@"path":levelPath, @"mediaType":@"application/json",
              @"role":@"canonical-level", @"sha256":PFN2ASHA256(levelData)},
        ],
    };
    NSData *manifestData = PFN2AJSON(manifestJSON, &error);
    if (!manifestData) { if (outError) *outError = error; return nil; }

    NSData *mime = [PFN2AMimeType dataUsingEncoding:NSASCIIStringEncoding];
    NSData *package = PFN2AWriteZip(@[
        @{@"name":@"mimetype", @"data":mime},
        @{@"name":@"manifest.json", @"data":manifestData},
        @{@"name":@"document.json", @"data":documentData},
        @{@"name":levelPath, @"data":levelData},
    ], &error);
    if (!package) { if (outError) *outError = error; return nil; }

    if (!PFN3BValidateWrittenPackage(package, &error)) {
        if (outError) *outError = error ?: PFN2AError(3341, @"Native package failed FORMAT-3B read-back validation.");
        return nil;
    }
    return package;
}

static LELevelData *PfhorgeNative2ALoadLevel(NSData *data, id document, NSError **outError)
{
    NSError *error = nil;
    NSDictionary<NSString *,NSData *> *files = PFN2AReadZip(data, &error);
    if (!files) { if (outError) *outError = error; return nil; }

    NSString *mime = [[NSString alloc] initWithData:files[@"mimetype"] encoding:NSASCIIStringEncoding];
    if (![mime isEqualToString:PFN2AMimeType]) {
        if (outError) *outError = PFN2AError(3350, @"ZIP is not a Pfhorge Native document.");
        return nil;
    }
    NSDictionary *manifest = PFN3BParseJSONResource(files, @"manifest.json", &error);
    if (![manifest[@"format"] isEqual:@"org.pfhorge.native"] || ![manifest[@"kind"] isEqual:@"level"]) {
        if (outError) *outError = error ?: PFN2AError(3351, @"Invalid Pfhorge Native manifest.");
        return nil;
    }
    if (!PFN3BVerifyManifestResources(manifest, files, &error)) {
        if (outError) *outError = error; return nil;
    }

    BOOL bridgeDeclared = NO;
    for (NSDictionary *ext in manifest[@"extensions"] ?: @[]) {
        if (![ext isKindOfClass:NSDictionary.class]) continue;
        NSString *extId = ext[@"id"];
        if ([extId isEqual:PFN2AExtensionId]) {
            bridgeDeclared = [ext[@"resources"] containsObject:PFN2ABridgePath];
            continue;
        }
        if ([ext[@"requiredForRead"] boolValue]) {
            if (outError) *outError = PFN2AError(3352, [NSString stringWithFormat:@"Unsupported required native extension: %@", extId ?: @"<unknown>"]);
            return nil;
        }
    }

    NSDictionary *documentJSON = PFN3BParseJSONResource(files, manifest[@"document"], &error);
    if (!documentJSON) { if (outError) *outError = error; return nil; }
    PFN2ASetId(document, documentJSON[@"id"]);
    NSArray *levelRefs = [documentJSON[@"levels"] isKindOfClass:NSArray.class] ? documentJSON[@"levels"] : nil;
    NSDictionary *levelRef = levelRefs.count && [levelRefs[0] isKindOfClass:NSDictionary.class] ? levelRefs[0] : nil;

    // Backward-compatible migration path for FORMAT-2A / FORMAT-3A packages.
    if (bridgeDeclared || files[PFN2ABridgePath]) {
        NSData *bridge = files[PFN2ABridgePath];
        if (!bridge) {
            if (outError) *outError = PFN2AError(3353, @"Required historical bridge resource is missing.");
            return nil;
        }
        LELevelData *level = [NSKeyedUnarchiver unarchivedObjectOfClass:[LELevelData class]
                                                               fromData:bridge
                                                                  error:&error];
        if (!level) {
            if (outError) *outError = error ?: PFN2AError(3354, @"Could not decode historical Pfhorge Native bridge resource.");
            return nil;
        }
        PFN2ASetId(level, levelRef[@"id"]);
        return level;
    }

    NSString *levelPath = levelRef[@"path"];
    NSDictionary *levelJSON = PFN3BParseJSONResource(files, levelPath, &error);
    if (!levelJSON || !PFN3BValidateCanonicalLevelJSON(levelJSON, &error)) {
        if (outError) *outError = error; return nil;
    }
    LELevelData *level = PFN3BLevelFromCanonicalJSON(levelJSON, &error);
    if (!level) { if (outError) *outError = error; return nil; }
    PFN2ASetId(level, levelRef[@"id"] ?: levelJSON[@"id"]);
    return level;
}
