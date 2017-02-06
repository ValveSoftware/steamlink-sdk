// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kojii): This file is compiled with $host_toolchain, which cannot find
// include files in platform/fonts. The build scripts should include this
// directory, and fix these #include's to "platform/fonts/...".
#include "CharacterPropertyDataGenerator.h"

#include "CharacterProperty.h"
#include <cassert>
#include <cstring>
#include <stdio.h>
#if !defined(USING_SYSTEM_ICU)
#define MUTEX_H // Prevent compile failure of utrie2.h on Windows
#include <utrie2.h>
#endif

#if defined(USING_SYSTEM_ICU)
static void generate(FILE*)
{
}
#else

using namespace blink;

const UChar32 kMaxCodepoint = 0x10FFFF;
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

static void setRanges(CharacterProperty* values,
    const UChar32* ranges, size_t length,
    CharacterProperty value)
{
    assert(length % 2 == 0);
    const UChar32* end = ranges + length;
    for (; ranges != end; ranges += 2) {
        assert(ranges[0] <= ranges[1]
            && ranges[1] <= kMaxCodepoint);
        for (UChar32 c = ranges[0]; c <= ranges[1]; c++)
            values[c] |= value;
    }
}

static void setValues(CharacterProperty* values,
    const UChar32* begin, size_t length,
    CharacterProperty value)
{
    const UChar32* end = begin + length;
    for (; begin != end; begin++) {
        assert(*begin <= kMaxCodepoint);
        values[*begin] |= value;
    }
}

static void generateUTrieSerialized(FILE* fp, int32_t size, uint8_t* array)
{
    fprintf(fp,
        "#include <cstdint>\n\n"
        "namespace blink {\n\n"
        "int32_t serializedCharacterDataSize = %d;\n"
        "uint8_t serializedCharacterData[] = {", size);
    for (int32_t i = 0; i < size; ) {
        fprintf(fp, "\n   ");
        for (int col = 0; col < 16 && i < size; col++, i++)
            fprintf(fp, " 0x%02X,", array[i]);
    }
    fprintf(fp,
        "\n};\n\n"
        "} // namespace blink\n");
}

static void generate(FILE* fp)
{
    // Create a value array of all possible code points.
    const UChar32 size = kMaxCodepoint + 1;
    CharacterProperty* values = new CharacterProperty[size];
    memset(values, 0, sizeof(CharacterProperty) * size);

#define SET(name) \
    setRanges(values, name##Ranges, ARRAY_LENGTH(name##Ranges), \
        CharacterProperty::name); \
    setValues(values, name##Array, ARRAY_LENGTH(name##Array), \
        CharacterProperty::name);

    SET(isCJKIdeographOrSymbol);
    SET(isUprightInMixedVertical);
    SET(isPotentialCustomElementNameChar);

    // Create a trie from the value array.
    UErrorCode error = U_ZERO_ERROR;
    UTrie2* trie = utrie2_open(0, 0, &error);
    assert(error == U_ZERO_ERROR);
    UChar32 start = 0;
    CharacterProperty value = values[0];
    for (UChar32 c = 1;; c++) {
        if (c < size && values[c] == value)
            continue;
        if (static_cast<uint32_t>(value)) {
            utrie2_setRange32(trie, start, c - 1,
                static_cast<uint32_t>(value), TRUE, &error);
            assert(error == U_ZERO_ERROR);
        }
        if (c >= size)
            break;
        start = c;
        value = values[start];
    }

    // Freeze and serialize the trie to a byte array.
    utrie2_freeze(trie, UTrie2ValueBits::UTRIE2_16_VALUE_BITS, &error);
    assert(error == U_ZERO_ERROR);
    int32_t serializedSize = utrie2_serialize(trie, nullptr, 0, &error);
    error = U_ZERO_ERROR;
    uint8_t* serialized = new uint8_t[serializedSize];
    serializedSize = utrie2_serialize(trie, serialized, serializedSize, &error);
    assert(error == U_ZERO_ERROR);

    generateUTrieSerialized(fp, serializedSize, serialized);

    utrie2_close(trie);
}
#endif

int main(int argc, char** argv)
{

    // Write the serialized array to the source file.
    if (argc <= 1) {
        generate(stdout);
    } else {
        FILE* fp = fopen(argv[1], "wb");
        generate(fp);
        fclose(fp);
    }

    return 0;
}
