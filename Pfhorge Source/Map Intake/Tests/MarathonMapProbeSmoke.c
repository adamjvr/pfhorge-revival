// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#include "../Core/MarathonMapProbe.h"
#include "../Core/MarathonMapProbe.inc"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8U);
    p[1] = (uint8_t)value;
}

static void write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24U);
    p[1] = (uint8_t)(value >> 16U);
    p[2] = (uint8_t)(value >> 8U);
    p[3] = (uint8_t)value;
}

static size_t append_tag(
    uint8_t *entry,
    size_t cursor,
    const char tag[4],
    size_t payload_length,
    bool last)
{
    memcpy(entry + cursor, tag, 4);
    const size_t next = cursor + 16 + payload_length;
    write_be32(entry + cursor + 4, last ? 0 : (uint32_t)next);
    write_be32(entry + cursor + 8, (uint32_t)payload_length);
    write_be32(entry + cursor + 12, 0);
    memset(entry + cursor + 16, 0, payload_length);
    return next;
}

static size_t make_v4_map(uint8_t *buffer, size_t capacity)
{
    assert(capacity >= 512);
    memset(buffer, 0, capacity);

    const size_t entry_offset = 128;
    size_t cursor = 0;
    cursor = append_tag(buffer + entry_offset, cursor, "PNTS", 8, false);
    cursor = append_tag(buffer + entry_offset, cursor, "LINS", 32, false);
    cursor = append_tag(buffer + entry_offset, cursor, "POLY", 128, true);

    const size_t directory_offset = entry_offset + cursor;
    const size_t record_size = 10 + 74;
    const size_t total_size = directory_offset + record_size;

    write_be16(buffer + 0, 4);
    write_be16(buffer + 2, 1);
    memcpy(buffer + 4, "Smoke Infinity Map", 18);
    write_be32(buffer + 68, 0);
    write_be32(buffer + 72, (uint32_t)directory_offset);
    write_be16(buffer + 76, 1);
    write_be16(buffer + 78, 74);
    write_be16(buffer + 80, 16);
    write_be16(buffer + 82, 10);
    write_be32(buffer + 84, 0);

    write_be32(buffer + directory_offset + 0, (uint32_t)entry_offset);
    write_be32(buffer + directory_offset + 4, (uint32_t)cursor);
    write_be16(buffer + directory_offset + 8, 7);
    write_be16(buffer + directory_offset + 10, 0);
    write_be16(buffer + directory_offset + 12, 0);
    write_be32(buffer + directory_offset + 14, 1);
    memcpy(buffer + directory_offset + 18, "Smoke Level", 11);

    return total_size;
}

static size_t make_v0_map(uint8_t *buffer, size_t capacity)
{
    assert(capacity >= 512);
    memset(buffer, 0, capacity);

    const size_t entry_offset = 128;
    size_t cursor = 0;

    memcpy(buffer + entry_offset + cursor, "PNTS", 4);
    write_be32(buffer + entry_offset + cursor + 4, 20);
    write_be32(buffer + entry_offset + cursor + 8, 8);
    cursor = 20;

    memcpy(buffer + entry_offset + cursor, "LINS", 4);
    write_be32(buffer + entry_offset + cursor + 4, 64);
    write_be32(buffer + entry_offset + cursor + 8, 32);
    cursor = 64;

    memcpy(buffer + entry_offset + cursor, "POLY", 4);
    write_be32(buffer + entry_offset + cursor + 4, 0);
    write_be32(buffer + entry_offset + cursor + 8, 128);
    cursor += 12 + 128;

    const size_t directory_offset = entry_offset + cursor;
    const size_t total_size = directory_offset + 8;

    write_be16(buffer + 0, 0);
    write_be16(buffer + 2, 0);
    memcpy(buffer + 4, "Smoke M1 Map", 12);
    write_be32(buffer + 72, (uint32_t)directory_offset);
    write_be16(buffer + 76, 1);

    write_be32(buffer + directory_offset + 0, (uint32_t)entry_offset);
    write_be32(buffer + directory_offset + 4, (uint32_t)cursor);
    return total_size;
}

static size_t wrap_applesingle(
    uint8_t *destination,
    size_t capacity,
    const uint8_t *data,
    size_t data_length)
{
    const size_t header_size = 26 + 12;
    assert(capacity >= header_size + data_length);
    memset(destination, 0, capacity);
    write_be32(destination + 0, UINT32_C(0x00051600));
    write_be32(destination + 4, UINT32_C(0x00020000));
    write_be16(destination + 24, 1);
    write_be32(destination + 26, 1);
    write_be32(destination + 30, (uint32_t)header_size);
    write_be32(destination + 34, (uint32_t)data_length);
    memcpy(destination + header_size, data, data_length);
    return header_size + data_length;
}

static size_t wrap_macbinary(
    uint8_t *destination,
    size_t capacity,
    const uint8_t *data,
    size_t data_length)
{
    const size_t padded_data = (data_length + 127U) & ~127U;
    assert(capacity >= 128 + padded_data);
    memset(destination, 0, capacity);
    destination[1] = 5;
    memcpy(destination + 2, "Smoke", 5);
    memcpy(destination + 65, "sce2Pfhg", 8);
    write_be32(destination + 83, (uint32_t)data_length);
    write_be32(destination + 87, 0);
    destination[122] = 129;
    destination[123] = 129;
    memcpy(destination + 128, data, data_length);
    return 128 + padded_data;
}

static void test_v4_map(void)
{
    uint8_t bytes[1024];
    const size_t length = make_v4_map(bytes, sizeof(bytes));
    PfhMapProbeResult result;
    const bool success = PfhMapProbeDataFork((PfhMapByteView){bytes, length}, &result);

    assert(success);
    assert(result.contentKind == PfhMapContentMap);
    assert(result.dialect == PfhMapDialectInfinityCompatible);
    assert(result.directoryEntryCount == 1);
    assert(result.directoryEntries[0].logicalIndex == 7);
    assert(result.directoryEntries[0].hasLevelName);
    assert(result.tagCount == 3);
}

static void test_v0_map(void)
{
    uint8_t bytes[1024];
    const size_t length = make_v0_map(bytes, sizeof(bytes));
    PfhMapProbeResult result;
    const bool success = PfhMapProbeDataFork((PfhMapByteView){bytes, length}, &result);

    assert(success);
    assert(result.contentKind == PfhMapContentMap);
    assert(result.dialect == PfhMapDialectMarathon1);
    assert(result.directoryEntries[0].logicalIndex == 0);
}

static void test_applesingle(void)
{
    uint8_t map[1024];
    uint8_t wrapped[2048];
    const size_t map_length = make_v4_map(map, sizeof(map));
    const size_t wrapped_length = wrap_applesingle(
        wrapped,
        sizeof(wrapped),
        map,
        map_length);

    PfhMapResolvedForks forks;
    assert(PfhMapResolveContainedForks(
        (PfhMapByteView){wrapped, wrapped_length},
        &forks));
    assert(forks.envelopeKind == PfhMapSourceEnvelopeAppleSingle);
    assert(forks.dataFork.length == map_length);

    PfhMapProbeResult result;
    assert(PfhMapProbeDataFork(forks.dataFork, &result));
}

static void test_macbinary(void)
{
    uint8_t map[1024];
    uint8_t wrapped[4096];
    const size_t map_length = make_v4_map(map, sizeof(map));
    const size_t wrapped_length = wrap_macbinary(
        wrapped,
        sizeof(wrapped),
        map,
        map_length);

    PfhMapResolvedForks forks;
    assert(PfhMapResolveContainedForks(
        (PfhMapByteView){wrapped, wrapped_length},
        &forks));
    assert(forks.envelopeKind == PfhMapSourceEnvelopeMacBinary);
    assert(forks.dataFork.length == map_length);
}

static void test_malformed_directory(void)
{
    uint8_t bytes[1024];
    const size_t length = make_v4_map(bytes, sizeof(bytes));
    write_be32(bytes + 72, UINT32_C(0x7FFFFFFF));

    PfhMapProbeResult result;
    assert(!PfhMapProbeDataFork((PfhMapByteView){bytes, length}, &result));
    assert(!result.structurallyUsable);
    assert(result.findingCount > 0);
}

int main(void)
{
    test_v4_map();
    test_v0_map();
    test_applesingle();
    test_macbinary();
    test_malformed_directory();
    puts("MAP-1A Marathon map probe smoke test passed");
    return EXIT_SUCCESS;
}
