// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#ifndef PFHORGE_MARATHON_MAP_PROBE_IMPLEMENTATION
#define PFHORGE_MARATHON_MAP_PROBE_IMPLEMENTATION

#include "MarathonMapProbe.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PFH_APPLESINGLE_MAGIC UINT32_C(0x00051600)
#define PFH_APPLEDOUBLE_MAGIC UINT32_C(0x00051607)
#define PFH_APPLEFILE_VERSION_2 UINT32_C(0x00020000)
#define PFH_APPLE_ENTRY_DATA_FORK UINT32_C(1)
#define PFH_APPLE_ENTRY_RESOURCE_FORK UINT32_C(2)
#define PFH_APPLE_ENTRY_FINDER_INFO UINT32_C(9)

static bool PfhMapCheckedAddSize(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

static bool PfhMapCheckedMultiplySize(size_t a, size_t b, size_t *result)
{
    if (a != 0 && b > SIZE_MAX / a) {
        return false;
    }
    *result = a * b;
    return true;
}

static bool PfhMapViewContains(PfhMapByteView view, size_t offset, size_t length)
{
    size_t end = 0;
    return PfhMapCheckedAddSize(offset, length, &end) && end <= view.length;
}

static uint16_t PfhMapReadBE16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
}

static int16_t PfhMapReadBES16(const uint8_t *bytes)
{
    return (int16_t)PfhMapReadBE16(bytes);
}

static uint32_t PfhMapReadBE32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static int32_t PfhMapReadBES32(const uint8_t *bytes)
{
    return (int32_t)PfhMapReadBE32(bytes);
}

static void PfhMapClearResult(PfhMapProbeResult *result)
{
    memset(result, 0, sizeof(*result));
    result->checksumStatus = PfhMapChecksumNotCalculated;
}

static void PfhMapAddFinding(
    PfhMapProbeResult *result,
    PfhMapFindingSeverity severity,
    const char *format,
    ...)
{
    if (result->findingCount >= PFH_MAP_MAX_FINDINGS) {
        result->findingsTruncated = true;
        return;
    }

    PfhMapFinding *finding = &result->findings[result->findingCount++];
    finding->severity = severity;

    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(finding->message, sizeof(finding->message), format, arguments);
    va_end(arguments);
}

static bool PfhMapHasFatalFinding(const PfhMapProbeResult *result)
{
    for (size_t index = 0; index < result->findingCount; ++index) {
        if (result->findings[index].severity == PfhMapFindingFatal) {
            return true;
        }
    }
    return false;
}

static bool PfhMapMakeSubview(
    PfhMapByteView source,
    uint32_t offset,
    uint32_t length,
    PfhMapByteView *view)
{
    if (!PfhMapViewContains(source, (size_t)offset, (size_t)length)) {
        return false;
    }

    view->bytes = source.bytes + offset;
    view->length = length;
    return true;
}

static bool PfhMapResolveAppleFile(
    PfhMapByteView source,
    PfhMapResolvedForks *resolved,
    uint32_t magic)
{
    if (!PfhMapViewContains(source, 0, 26)) {
        return false;
    }

    const uint32_t version = PfhMapReadBE32(source.bytes + 4);
    const uint16_t entryCount = PfhMapReadBE16(source.bytes + 24);

    if (version != PFH_APPLEFILE_VERSION_2 && version != UINT32_C(0x00010000)) {
        return false;
    }

    size_t descriptorBytes = 0;
    size_t descriptorEnd = 0;
    if (!PfhMapCheckedMultiplySize(entryCount, 12, &descriptorBytes) ||
        !PfhMapCheckedAddSize(26, descriptorBytes, &descriptorEnd) ||
        descriptorEnd > source.length) {
        return false;
    }

    resolved->envelopeKind =
        magic == PFH_APPLESINGLE_MAGIC
            ? PfhMapSourceEnvelopeAppleSingle
            : PfhMapSourceEnvelopeAppleDouble;
    resolved->recognizedEnvelope = true;

    for (uint16_t index = 0; index < entryCount; ++index) {
        const size_t descriptorOffset = 26 + ((size_t)index * 12);
        const uint32_t entryID = PfhMapReadBE32(source.bytes + descriptorOffset);
        const uint32_t entryOffset = PfhMapReadBE32(source.bytes + descriptorOffset + 4);
        const uint32_t entryLength = PfhMapReadBE32(source.bytes + descriptorOffset + 8);
        PfhMapByteView entryView = {0};

        if (!PfhMapMakeSubview(source, entryOffset, entryLength, &entryView)) {
            return false;
        }

        switch (entryID) {
            case PFH_APPLE_ENTRY_DATA_FORK:
                resolved->dataFork = entryView;
                break;

            case PFH_APPLE_ENTRY_RESOURCE_FORK:
                resolved->resourceFork = entryView;
                break;

            case PFH_APPLE_ENTRY_FINDER_INFO:
                resolved->finderInfo = entryView;
                break;

            default:
                break;
        }
    }

    return true;
}

static size_t PfhMapRoundUp128(size_t value)
{
    const size_t remainder = value % 128;
    if (remainder == 0) {
        return value;
    }
    return value + (128 - remainder);
}

static bool PfhMapResolveMacBinary(
    PfhMapByteView source,
    PfhMapResolvedForks *resolved)
{
    if (!PfhMapViewContains(source, 0, 128)) {
        return false;
    }

    const uint8_t filenameLength = source.bytes[1];
    if (source.bytes[0] != 0 || filenameLength < 1 || filenameLength > 63 ||
        source.bytes[74] != 0 || source.bytes[82] != 0) {
        return false;
    }

    const uint32_t dataLength = PfhMapReadBE32(source.bytes + 83);
    const uint32_t resourceLength = PfhMapReadBE32(source.bytes + 87);
    const uint16_t secondaryHeaderLength = PfhMapReadBE16(source.bytes + 120);

    size_t dataOffset = 128;
    size_t secondaryPadded = PfhMapRoundUp128(secondaryHeaderLength);
    if (!PfhMapCheckedAddSize(dataOffset, secondaryPadded, &dataOffset)) {
        return false;
    }

    size_t resourceOffset = 0;
    if (!PfhMapCheckedAddSize(dataOffset, PfhMapRoundUp128(dataLength), &resourceOffset)) {
        return false;
    }

    if (!PfhMapViewContains(source, dataOffset, dataLength) ||
        !PfhMapViewContains(source, resourceOffset, resourceLength)) {
        return false;
    }

    resolved->envelopeKind = PfhMapSourceEnvelopeMacBinary;
    resolved->recognizedEnvelope = true;
    resolved->dataFork.bytes = source.bytes + dataOffset;
    resolved->dataFork.length = dataLength;
    resolved->resourceFork.bytes = source.bytes + resourceOffset;
    resolved->resourceFork.length = resourceLength;
    resolved->finderInfo.bytes = source.bytes + 65;
    resolved->finderInfo.length = 16;
    return true;
}

bool PfhMapResolveContainedForks(
    PfhMapByteView source,
    PfhMapResolvedForks *resolved)
{
    if (resolved == NULL || source.bytes == NULL) {
        return false;
    }

    memset(resolved, 0, sizeof(*resolved));
    resolved->envelopeKind = PfhMapSourceEnvelopeRaw;
    resolved->dataFork = source;

    if (source.length >= 4) {
        const uint32_t magic = PfhMapReadBE32(source.bytes);
        if (magic == PFH_APPLESINGLE_MAGIC || magic == PFH_APPLEDOUBLE_MAGIC) {
            memset(resolved, 0, sizeof(*resolved));
            return PfhMapResolveAppleFile(source, resolved, magic);
        }
    }

    PfhMapResolvedForks macBinary = {0};
    if (PfhMapResolveMacBinary(source, &macBinary)) {
        *resolved = macBinary;
        return true;
    }

    resolved->recognizedEnvelope = false;
    return true;
}

static uint32_t PfhMapCRC32WithZeroedChecksum(PfhMapByteView data)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);

    for (size_t index = 0; index < data.length; ++index) {
        uint8_t byte = data.bytes[index];
        if (index >= 68 && index < 72) {
            byte = 0;
        }

        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }

    return crc ^ UINT32_C(0xFFFFFFFF);
}

static bool PfhMapTagEquals(const char tag[5], const char expected[5])
{
    return memcmp(tag, expected, 4) == 0;
}

static void PfhMapCopyTag(char destination[5], const uint8_t *source)
{
    for (size_t index = 0; index < 4; ++index) {
        const uint8_t byte = source[index];
        destination[index] =
            (byte >= 0x20 && byte <= 0x7E) ? (char)byte : '.';
    }
    destination[4] = '\0';
}

static bool PfhMapHasTag(const PfhMapProbeResult *result, const char expected[5])
{
    for (size_t index = 0; index < result->tagCount; ++index) {
        if (PfhMapTagEquals(result->tags[index].tag, expected)) {
            return true;
        }
    }
    return false;
}

static void PfhMapClassify(PfhMapProbeResult *result)
{
    switch (result->header.dataVersion) {
        case 0:
            result->dialect = PfhMapDialectMarathon1;
            break;

        case 1:
            result->dialect =
                result->header.containerVersion >= 4
                    ? PfhMapDialectInfinityCompatible
                    : PfhMapDialectMarathon2;
            break;

        case 2:
            result->dialect = PfhMapDialectAlephOneExtended;
            break;

        default:
            result->dialect = PfhMapDialectUnknown;
            break;
    }

    const bool hasPoints =
        PfhMapHasTag(result, "PNTS") || PfhMapHasTag(result, "EPNT");
    const bool hasLines = PfhMapHasTag(result, "LINS");
    const bool hasPolygons = PfhMapHasTag(result, "POLY");

    if (hasPoints && hasLines && hasPolygons) {
        result->contentKind = PfhMapContentMap;
        if (result->header.parentChecksum != 0) {
            result->mapKind = PfhMapKindOverlay;
        } else if (result->header.entryCount > 1) {
            result->mapKind = PfhMapKindMergedScenario;
        } else {
            result->mapKind = PfhMapKindSingleLevel;
        }
    } else if (result->tagCount > 0) {
        result->contentKind = PfhMapContentNonMapContainer;
        PfhMapAddFinding(
            result,
            PfhMapFindingWarning,
            "Marathon container found, but the map geometry tag set is incomplete.");
    } else if (result->structurallyUsable) {
        result->contentKind = PfhMapContentUnknown;
    } else {
        result->contentKind = PfhMapContentDamaged;
    }
}

static bool PfhMapReadHeader(
    PfhMapByteView dataFork,
    PfhMapProbeResult *result)
{
    if (!PfhMapViewContains(dataFork, 0, 128)) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "The data fork is shorter than the 128-byte Marathon container header.");
        return false;
    }

    const uint8_t *bytes = dataFork.bytes;
    PfhMapContainerHeader *header = &result->header;
    header->containerVersion = PfhMapReadBES16(bytes + 0);
    header->dataVersion = PfhMapReadBES16(bytes + 2);
    memcpy(header->internalName, bytes + 4, sizeof(header->internalName));
    header->declaredChecksum = PfhMapReadBE32(bytes + 68);
    header->directoryOffset = PfhMapReadBES32(bytes + 72);
    header->entryCount = PfhMapReadBES16(bytes + 76);
    header->applicationDirectoryDataSize = PfhMapReadBES16(bytes + 78);
    header->entryHeaderSize = PfhMapReadBES16(bytes + 80);
    header->directoryEntryBaseSize = PfhMapReadBES16(bytes + 82);
    header->parentChecksum = PfhMapReadBE32(bytes + 84);

    if (header->containerVersion < 0 || header->containerVersion > 4 ||
        header->containerVersion == 3) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "Unsupported or implausible Marathon container version %d.",
            header->containerVersion);
        return false;
    }

    if (header->dataVersion < 0 || header->dataVersion > 2) {
        PfhMapAddFinding(
            result,
            PfhMapFindingWarning,
            "Unknown Marathon data version %d; structural inspection will continue.",
            header->dataVersion);
    }

    if (header->entryCount < 1) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "The Marathon container declares no entries.");
        return false;
    }

    if (header->directoryOffset < 128) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "Directory offset %d is before the end of the header.",
            header->directoryOffset);
        return false;
    }

    if (header->applicationDirectoryDataSize < 0) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "Application-specific directory data size is negative.");
        return false;
    }

    if (header->containerVersion <= 1) {
        if (header->applicationDirectoryDataSize != 0) {
            PfhMapAddFinding(
                result,
                PfhMapFindingCompatibility,
                "Old container version uses nonzero application directory data.");
        }
    } else {
        if (header->entryHeaderSize < 12 || header->entryHeaderSize > 64) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Entry header size %d is outside the supported safe range.",
                header->entryHeaderSize);
            return false;
        }
        if (header->directoryEntryBaseSize < 8 || header->directoryEntryBaseSize > 64) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Directory base size %d is outside the supported safe range.",
                header->directoryEntryBaseSize);
            return false;
        }
    }

    result->recognizedMarathonContainer = true;
    return true;
}

static bool PfhMapParseEntryTags(
    PfhMapByteView dataFork,
    const PfhMapDirectoryEntrySummary *entry,
    int16_t containerVersion,
    int16_t storedEntryHeaderSize,
    PfhMapProbeResult *result)
{
    const size_t entryStart = (size_t)entry->dataOffset;
    const size_t entryLength = (size_t)entry->dataLength;
    const size_t headerSize =
        containerVersion <= 1 ? 12U : (size_t)storedEntryHeaderSize;

    if (entryLength == 0) {
        PfhMapAddFinding(
            result,
            PfhMapFindingWarning,
            "Directory entry %zu has zero length.",
            entry->directoryOrdinal);
        return true;
    }

    size_t relativeOffset = 0;
    size_t guard = 0;

    while (true) {
        if (++guard > 4096) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Directory entry %zu has an excessive or cyclic tag chain.",
                entry->directoryOrdinal);
            return false;
        }

        size_t absoluteOffset = 0;
        if (!PfhMapCheckedAddSize(entryStart, relativeOffset, &absoluteOffset) ||
            !PfhMapViewContains(dataFork, absoluteOffset, headerSize) ||
            relativeOffset + headerSize > entryLength) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Tag header in directory entry %zu extends outside the entry.",
                entry->directoryOrdinal);
            return false;
        }

        const uint8_t *header = dataFork.bytes + absoluteOffset;
        const int32_t nextOffset = PfhMapReadBES32(header + 4);
        const int32_t payloadLength = PfhMapReadBES32(header + 8);
        const int32_t patchOffset =
            headerSize >= 16 ? PfhMapReadBES32(header + 12) : 0;

        if (payloadLength < 0) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Tag in directory entry %zu has a negative payload length.",
                entry->directoryOrdinal);
            return false;
        }

        size_t payloadEnd = 0;
        if (!PfhMapCheckedAddSize(relativeOffset, headerSize, &payloadEnd) ||
            !PfhMapCheckedAddSize(payloadEnd, (size_t)payloadLength, &payloadEnd) ||
            payloadEnd > entryLength) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Tag payload in directory entry %zu extends outside the entry.",
                entry->directoryOrdinal);
            return false;
        }

        if (result->tagCount < PFH_MAP_MAX_TAGS) {
            PfhMapTagSummary *summary = &result->tags[result->tagCount++];
            summary->directoryOrdinal = entry->directoryOrdinal;
            summary->logicalIndex = entry->logicalIndex;
            PfhMapCopyTag(summary->tag, header);
            summary->payloadLength = payloadLength;
            summary->patchOffset = patchOffset;
        } else {
            result->tagsTruncated = true;
        }

        if (nextOffset == 0) {
            break;
        }

        if (nextOffset < 0 || (size_t)nextOffset < payloadEnd ||
            (size_t)nextOffset >= entryLength) {
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Tag chain in directory entry %zu contains an invalid next offset %d.",
                entry->directoryOrdinal,
                nextOffset);
            return false;
        }

        relativeOffset = (size_t)nextOffset;
    }

    return true;
}

static bool PfhMapParseDirectory(
    PfhMapByteView dataFork,
    PfhMapProbeResult *result)
{
    const PfhMapContainerHeader *header = &result->header;
    const size_t baseSize =
        header->containerVersion <= 1
            ? 8U
            : (size_t)header->directoryEntryBaseSize;
    const size_t applicationSize =
        (size_t)header->applicationDirectoryDataSize;
    size_t recordSize = 0;
    size_t directorySize = 0;

    if (!PfhMapCheckedAddSize(baseSize, applicationSize, &recordSize) ||
        !PfhMapCheckedMultiplySize((size_t)header->entryCount, recordSize, &directorySize) ||
        !PfhMapViewContains(dataFork, (size_t)header->directoryOffset, directorySize)) {
        PfhMapAddFinding(
            result,
            PfhMapFindingFatal,
            "The Marathon directory extends outside the data fork.");
        return false;
    }

    int16_t seenLogicalIndexes[PFH_MAP_MAX_DIRECTORY_ENTRIES] = {0};
    int32_t seenOffsets[PFH_MAP_MAX_DIRECTORY_ENTRIES] = {0};
    int32_t seenLengths[PFH_MAP_MAX_DIRECTORY_ENTRIES] = {0};
    size_t seenCount = 0;

    for (size_t ordinal = 0; ordinal < (size_t)header->entryCount; ++ordinal) {
        const size_t recordOffset =
            (size_t)header->directoryOffset + (ordinal * recordSize);
        const uint8_t *record = dataFork.bytes + recordOffset;
        const int32_t entryOffset = PfhMapReadBES32(record);
        const int32_t entryLength = PfhMapReadBES32(record + 4);
        const int16_t logicalIndex =
            header->containerVersion >= 2 && baseSize >= 10
                ? PfhMapReadBES16(record + 8)
                : (int16_t)ordinal;

        PfhMapDirectoryEntrySummary temporary = {0};
        temporary.directoryOrdinal = ordinal;
        temporary.logicalIndex = logicalIndex;
        temporary.dataOffset = entryOffset;
        temporary.dataLength = entryLength;

        if (applicationSize >= 74) {
            const uint8_t *application = record + baseSize;
            temporary.entryPointFlags = PfhMapReadBE32(application + 4);
            memcpy(temporary.levelName, application + 8, PFH_MAP_MAX_NAME_BYTES);
            temporary.hasLevelName = temporary.levelName[0] != 0;
        }

        if (entryOffset < 128 || entryLength < 0 ||
            !PfhMapViewContains(dataFork, (size_t)entryOffset, (size_t)entryLength)) {
            temporary.structurallyValid = false;
            PfhMapAddFinding(
                result,
                PfhMapFindingFatal,
                "Directory entry %zu points outside the data fork.",
                ordinal);
        } else {
            temporary.structurallyValid = true;

            const size_t entryStart = (size_t)entryOffset;
            const size_t entryEnd = entryStart + (size_t)entryLength;
            const size_t directoryStart = (size_t)header->directoryOffset;
            const size_t directoryEnd = directoryStart + directorySize;

            if (entryStart < directoryEnd && entryEnd > directoryStart) {
                temporary.structurallyValid = false;
                PfhMapAddFinding(
                    result,
                    PfhMapFindingFatal,
                    "Directory entry %zu overlaps the directory table.",
                    ordinal);
            }

            for (size_t previous = 0; previous < seenCount; ++previous) {
                const size_t previousStart = (size_t)seenOffsets[previous];
                const size_t previousEnd =
                    previousStart + (size_t)seenLengths[previous];

                if (entryStart < previousEnd && entryEnd > previousStart) {
                    temporary.structurallyValid = false;
                    PfhMapAddFinding(
                        result,
                        PfhMapFindingFatal,
                        "Directory entries %zu and %zu overlap.",
                        previous,
                        ordinal);
                }

                if (seenLogicalIndexes[previous] == logicalIndex) {
                    PfhMapAddFinding(
                        result,
                        PfhMapFindingWarning,
                        "Directory entries %zu and %zu share logical index %d.",
                        previous,
                        ordinal,
                        logicalIndex);
                }
            }
        }

        if (temporary.structurallyValid &&
            seenCount < PFH_MAP_MAX_DIRECTORY_ENTRIES) {
            seenLogicalIndexes[seenCount] = logicalIndex;
            seenOffsets[seenCount] = entryOffset;
            seenLengths[seenCount] = entryLength;
            ++seenCount;
        }

        if (result->directoryEntryCount < PFH_MAP_MAX_DIRECTORY_ENTRIES) {
            result->directoryEntries[result->directoryEntryCount++] = temporary;
        } else {
            result->directoryEntriesTruncated = true;
        }

        if (temporary.structurallyValid &&
            !PfhMapParseEntryTags(
                dataFork,
                &temporary,
                header->containerVersion,
                header->entryHeaderSize,
                result)) {
            return false;
        }
    }

    return !PfhMapHasFatalFinding(result);
}

bool PfhMapProbeDataFork(
    PfhMapByteView dataFork,
    PfhMapProbeResult *result)
{
    if (result == NULL || dataFork.bytes == NULL) {
        return false;
    }

    PfhMapClearResult(result);

    if (!PfhMapReadHeader(dataFork, result)) {
        result->contentKind = PfhMapContentDamaged;
        return false;
    }

    result->computedChecksum = PfhMapCRC32WithZeroedChecksum(dataFork);
    if (result->header.declaredChecksum == 0) {
        result->checksumStatus = PfhMapChecksumNotPresent;
    } else if (result->header.declaredChecksum == result->computedChecksum) {
        result->checksumStatus = PfhMapChecksumMatches;
    } else {
        result->checksumStatus = PfhMapChecksumMismatch;
        PfhMapAddFinding(
            result,
            PfhMapFindingWarning,
            "Stored checksum 0x%08X does not match computed checksum 0x%08X.",
            result->header.declaredChecksum,
            result->computedChecksum);
    }

    const bool directoryOK = PfhMapParseDirectory(dataFork, result);
    result->structurallyUsable = directoryOK && !PfhMapHasFatalFinding(result);
    PfhMapClassify(result);

    if (result->header.parentChecksum != 0) {
        PfhMapAddFinding(
            result,
            PfhMapFindingInfo,
            "Container declares parent checksum 0x%08X.",
            result->header.parentChecksum);
    }

    return result->recognizedMarathonContainer && result->structurallyUsable;
}

const char *PfhMapEnvelopeKindName(PfhMapSourceEnvelopeKind kind)
{
    switch (kind) {
        case PfhMapSourceEnvelopeRaw:
            return "Raw data fork";
        case PfhMapSourceEnvelopeAppleSingle:
            return "AppleSingle";
        case PfhMapSourceEnvelopeAppleDouble:
            return "AppleDouble";
        case PfhMapSourceEnvelopeMacBinary:
            return "MacBinary";
    }
    return "Unknown";
}

const char *PfhMapDialectName(PfhMapDialect dialect)
{
    switch (dialect) {
        case PfhMapDialectMarathon1:
            return "Marathon 1";
        case PfhMapDialectMarathon2:
            return "Marathon 2";
        case PfhMapDialectInfinityCompatible:
            return "Marathon Infinity / Aleph One compatible";
        case PfhMapDialectAlephOneExtended:
            return "Aleph One extended";
        case PfhMapDialectUnknown:
            break;
    }
    return "Unknown";
}

const char *PfhMapContentKindName(PfhMapContentKind kind)
{
    switch (kind) {
        case PfhMapContentMap:
            return "Marathon map or scenario";
        case PfhMapContentNonMapContainer:
            return "Non-map Marathon container";
        case PfhMapContentDamaged:
            return "Damaged Marathon container";
        case PfhMapContentUnknown:
            break;
    }
    return "Unknown content";
}

const char *PfhMapMapKindName(PfhMapMapKind kind)
{
    switch (kind) {
        case PfhMapKindSingleLevel:
            return "Single-level Marathon map";
        case PfhMapKindMergedScenario:
            return "Multi-level merged Marathon scenario";
        case PfhMapKindOverlay:
            return "Marathon overlay or patch container";
        case PfhMapKindUnknown:
            break;
    }
    return "Unknown Marathon map kind";
}

const char *PfhMapChecksumStatusName(PfhMapChecksumStatus status)
{
    switch (status) {
        case PfhMapChecksumNotPresent:
            return "Not present";
        case PfhMapChecksumMatches:
            return "Matches";
        case PfhMapChecksumMismatch:
            return "Mismatch";
        case PfhMapChecksumNotCalculated:
            break;
    }
    return "Not calculated";
}

const char *PfhMapFindingSeverityName(PfhMapFindingSeverity severity)
{
    switch (severity) {
        case PfhMapFindingInfo:
            return "Information";
        case PfhMapFindingWarning:
            return "Warning";
        case PfhMapFindingCompatibility:
            return "Compatibility";
        case PfhMapFindingFatal:
            return "Fatal";
    }
    return "Unknown";
}

#endif  // PFHORGE_MARATHON_MAP_PROBE_IMPLEMENTATION
