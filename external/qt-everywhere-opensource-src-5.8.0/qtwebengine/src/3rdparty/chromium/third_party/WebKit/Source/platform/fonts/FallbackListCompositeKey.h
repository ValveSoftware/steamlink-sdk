// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FallbackListCompositeKey_h
#define FallbackListCompositeKey_h

#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/HashTableDeletedValueType.h"

namespace blink {

class FontDescription;

// Cache key representing a font description and font fallback list combination
// as passed into shaping. Used to look up an applicable ShapeCache instance
// from the global FontCache.
// TODO(eae,drott): Ideally this should be replaced by a combination of
// FontDescription and CSSFontSelector.
struct FallbackListCompositeKey {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    FallbackListCompositeKey(const FontDescription& fontDescription)
        : m_hash(fontDescription.styleHashWithoutFamilyList() << 1)
        , m_computedSize(fontDescription.computedSize())
        , m_letterSpacing(fontDescription.letterSpacing())
        , m_wordSpacing(fontDescription.wordSpacing())
        , m_bitmapFields(fontDescription.bitmapFields())
        , m_auxiliaryBitmapFields(fontDescription.auxiliaryBitmapFields()) { }
    FallbackListCompositeKey()
        : m_hash(0)
        , m_computedSize(0)
        , m_letterSpacing(0)
        , m_wordSpacing(0)
        , m_bitmapFields(0)
        , m_auxiliaryBitmapFields(0) { }
    FallbackListCompositeKey(WTF::HashTableDeletedValueType)
        : m_hash(s_deletedValueHash)
        , m_computedSize(0)
        , m_letterSpacing(0)
        , m_wordSpacing(0)
        , m_bitmapFields(0)
        , m_auxiliaryBitmapFields(0) { }

    void add(FontCacheKey key)
    {
        m_fontCacheKeys.append(key);
        // Djb2 with the first bit reserved for deleted.
        m_hash = (((m_hash << 5) + m_hash) + key.hash()) << 1;
    }

    unsigned hash() const { return m_hash; }

    bool operator==(const FallbackListCompositeKey& other) const
    {
        return m_hash == other.m_hash
            && m_computedSize == other.m_computedSize
            && m_letterSpacing == other.m_letterSpacing
            && m_wordSpacing == other.m_wordSpacing
            && m_bitmapFields == other.m_bitmapFields
            && m_auxiliaryBitmapFields == other.m_auxiliaryBitmapFields
            && m_fontCacheKeys == other.m_fontCacheKeys;
    }

    bool isHashTableDeletedValue() const
    {
        return m_hash == s_deletedValueHash;
    }

private:
    static const unsigned s_deletedValueHash = 1;
    FontDescription m_fontDescription;
    Vector<FontCacheKey> m_fontCacheKeys;
    unsigned m_hash;

    float m_computedSize;
    float m_letterSpacing;
    float m_wordSpacing;
    unsigned m_bitmapFields;
    unsigned m_auxiliaryBitmapFields;
};

struct FallbackListCompositeKeyHash {
    STATIC_ONLY(FallbackListCompositeKeyHash);
    static unsigned hash(const FallbackListCompositeKey& key)
    {
        return key.hash();
    }

    static bool equal(const FallbackListCompositeKey& a, const FallbackListCompositeKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct FallbackListCompositeKeyTraits : WTF::SimpleClassHashTraits<FallbackListCompositeKey> {
    STATIC_ONLY(FallbackListCompositeKeyTraits);
};

} // namespace blink

#endif // FallbackListCompositeKey_h
