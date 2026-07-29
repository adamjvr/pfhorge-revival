// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#ifndef PFHORGE_MARATHON_MAP_PROBE_H
#define PFHORGE_MARATHON_MAP_PROBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PFH_MAP_MAX_DIRECTORY_ENTRIES 64
#define PFH_MAP_MAX_TAGS 256
#define PFH_MAP_MAX_FINDINGS 48
#define PFH_MAP_MAX_NAME_BYTES 66
#define PFH_MAP_MAX_MESSAGE_BYTES 192

typedef enum PfhMapSourceEnvelopeKind {
    PfhMapSourceEnvelopeRaw = 0,
    PfhMapSourceEnvelopeAppleSingle,
    PfhMapSourceEnvelopeAppleDouble,
    PfhMapSourceEnvelopeMacBinary
} PfhMapSourceEnvelopeKind;

typedef enum PfhMapDialect {
    PfhMapDialectUnknown = 0,
    PfhMapDialectMarathon1,
    PfhMapDialectMarathon2,
    PfhMapDialectInfinityCompatible,
    PfhMapDialectAlephOneExtended
} PfhMapDialect;

typedef enum PfhMapContentKind {
    PfhMapContentUnknown = 0,
    PfhMapContentMap,
    PfhMapContentNonMapContainer,
    PfhMapContentDamaged
} PfhMapContentKind;

typedef enum PfhMapFindingSeverity {
    PfhMapFindingInfo = 0,
    PfhMapFindingWarning,
    PfhMapFindingCompatibility,
    PfhMapFindingFatal
} PfhMapFindingSeverity;

typedef enum PfhMapChecksumStatus {
    PfhMapChecksumNotPresent = 0,
    PfhMapChecksumMatches,
    PfhMapChecksumMismatch,
    PfhMapChecksumNotCalculated
} PfhMapChecksumStatus;

typedef struct PfhMapByteView {
    const uint8_t *bytes;
    size_t length;
} PfhMapByteView;

typedef struct PfhMapResolvedForks {
    PfhMapSourceEnvelopeKind envelopeKind;
    PfhMapByteView dataFork;
    PfhMapByteView resourceFork;
    PfhMapByteView finderInfo;
    bool recognizedEnvelope;
} PfhMapResolvedForks;

typedef struct PfhMapContainerHeader {
    int16_t containerVersion;
    int16_t dataVersion;
    uint8_t internalName[64];
    uint32_t declaredChecksum;
    int32_t directoryOffset;
    int16_t entryCount;
    int16_t applicationDirectoryDataSize;
    int16_t entryHeaderSize;
    int16_t directoryEntryBaseSize;
    uint32_t parentChecksum;
} PfhMapContainerHeader;

typedef struct PfhMapDirectoryEntrySummary {
    size_t directoryOrdinal;
    int16_t logicalIndex;
    int32_t dataOffset;
    int32_t dataLength;
    uint32_t entryPointFlags;
    uint8_t levelName[PFH_MAP_MAX_NAME_BYTES];
    bool hasLevelName;
    bool structurallyValid;
} PfhMapDirectoryEntrySummary;

typedef struct PfhMapTagSummary {
    size_t directoryOrdinal;
    int16_t logicalIndex;
    char tag[5];
    int32_t payloadLength;
    int32_t patchOffset;
} PfhMapTagSummary;

typedef struct PfhMapFinding {
    PfhMapFindingSeverity severity;
    char message[PFH_MAP_MAX_MESSAGE_BYTES];
} PfhMapFinding;

typedef struct PfhMapProbeResult {
    bool recognizedMarathonContainer;
    bool structurallyUsable;
    PfhMapContentKind contentKind;
    PfhMapDialect dialect;
    PfhMapContainerHeader header;

    PfhMapChecksumStatus checksumStatus;
    uint32_t computedChecksum;

    size_t directoryEntryCount;
    PfhMapDirectoryEntrySummary directoryEntries[PFH_MAP_MAX_DIRECTORY_ENTRIES];
    bool directoryEntriesTruncated;

    size_t tagCount;
    PfhMapTagSummary tags[PFH_MAP_MAX_TAGS];
    bool tagsTruncated;

    size_t findingCount;
    PfhMapFinding findings[PFH_MAP_MAX_FINDINGS];
    bool findingsTruncated;
} PfhMapProbeResult;

/**
 * Resolves raw, AppleSingle, AppleDouble, and MacBinary byte streams.
 *
 * AppleDouble normally has no data fork inside the sidecar; callers should
 * pair its resource/finder views with the sibling raw data file.
 */
bool PfhMapResolveContainedForks(
    PfhMapByteView source,
    PfhMapResolvedForks *resolved);

/** Probe one normalized Marathon data fork. */
bool PfhMapProbeDataFork(
    PfhMapByteView dataFork,
    PfhMapProbeResult *result);

const char *PfhMapEnvelopeKindName(PfhMapSourceEnvelopeKind kind);
const char *PfhMapDialectName(PfhMapDialect dialect);
const char *PfhMapContentKindName(PfhMapContentKind kind);
const char *PfhMapChecksumStatusName(PfhMapChecksumStatus status);
const char *PfhMapFindingSeverityName(PfhMapFindingSeverity severity);

#ifdef __cplusplus
}
#endif

#endif
