// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/AtomicStringTable.h"

#include "wtf/text/StringHash.h"
#include "wtf/text/UTF8.h"

namespace WTF {

using namespace Unicode;

AtomicStringTable::AtomicStringTable()
{
    for (StringImpl* string : StringImpl::allStaticStrings().values())
        add(string);
}

AtomicStringTable::~AtomicStringTable()
{
    for (StringImpl* string : m_table) {
        if (!string->isStatic()) {
            DCHECK(string->isAtomic());
            string->setIsAtomic(false);
        }
    }
}

void AtomicStringTable::reserveCapacity(unsigned size)
{
    m_table.reserveCapacityForSize(size);
}

template<typename T, typename HashTranslator>
PassRefPtr<StringImpl> AtomicStringTable::addToStringTable(const T& value)
{
    HashSet<StringImpl*>::AddResult addResult = m_table.addWithTranslator<HashTranslator>(value);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.isNewEntry ? adoptRef(*addResult.storedValue) : *addResult.storedValue;
}

template<typename CharacterType>
struct HashTranslatorCharBuffer {
    const CharacterType* s;
    unsigned length;
};

typedef HashTranslatorCharBuffer<UChar> UCharBuffer;
struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        return WTF::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = StringImpl::create8BitIfPossible(buf.s, buf.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

template<typename CharacterType>
struct HashAndCharacters {
    unsigned hash;
    const CharacterType* characters;
    unsigned length;
};

template<typename CharacterType>
struct HashAndCharactersTranslator {
    static unsigned hash(const HashAndCharacters<CharacterType>& buffer)
    {
        DCHECK(buffer.hash == StringHasher::computeHashAndMaskTop8Bits(buffer.characters, buffer.length));
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndCharacters<CharacterType>& buffer)
    {
        return WTF::equal(string, buffer.characters, buffer.length);
    }

    static void translate(StringImpl*& location, const HashAndCharacters<CharacterType>& buffer, unsigned hash)
    {
        location = StringImpl::create(buffer.characters, buffer.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

struct HashAndUTF8Characters {
    unsigned hash;
    const char* characters;
    unsigned length;
    unsigned utf16Length;
};

struct HashAndUTF8CharactersTranslator {
    static unsigned hash(const HashAndUTF8Characters& buffer)
    {
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndUTF8Characters& buffer)
    {
        if (buffer.utf16Length != string->length())
            return false;

        // If buffer contains only ASCII characters UTF-8 and UTF16 length are the same.
        if (buffer.utf16Length != buffer.length) {
            if (string->is8Bit()) {
                const LChar* characters8 = string->characters8();
                return equalLatin1WithUTF8(characters8, characters8 + string->length(), buffer.characters, buffer.characters + buffer.length);
            }
            const UChar* characters16 = string->characters16();
            return equalUTF16WithUTF8(characters16, characters16 + string->length(), buffer.characters, buffer.characters + buffer.length);
        }

        if (string->is8Bit()) {
            const LChar* stringCharacters = string->characters8();

            for (unsigned i = 0; i < buffer.length; ++i) {
                DCHECK(isASCII(buffer.characters[i]));
                if (stringCharacters[i] != buffer.characters[i])
                    return false;
            }

            return true;
        }

        const UChar* stringCharacters = string->characters16();

        for (unsigned i = 0; i < buffer.length; ++i) {
            DCHECK(isASCII(buffer.characters[i]));
            if (stringCharacters[i] != buffer.characters[i])
                return false;
        }

        return true;
    }

    static void translate(StringImpl*& location, const HashAndUTF8Characters& buffer, unsigned hash)
    {
        UChar* target;
        RefPtr<StringImpl> newString = StringImpl::createUninitialized(buffer.utf16Length, target);

        bool isAllASCII;
        const char* source = buffer.characters;
        if (convertUTF8ToUTF16(&source, source + buffer.length, &target, target + buffer.utf16Length, &isAllASCII) != conversionOK)
            NOTREACHED();

        if (isAllASCII)
            newString = StringImpl::create(buffer.characters, buffer.length);

        location = newString.release().leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicStringTable::add(const UChar* s, unsigned length)
{
    if (!s)
        return nullptr;

    if (!length)
        return StringImpl::empty();

    UCharBuffer buffer = { s, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

PassRefPtr<StringImpl> AtomicStringTable::add(const UChar* s, unsigned length, unsigned existingHash)
{
    DCHECK(s);
    DCHECK(existingHash);

    if (!length)
        return StringImpl::empty();

    HashAndCharacters<UChar> buffer = { existingHash, s, length };
    return addToStringTable<HashAndCharacters<UChar>, HashAndCharactersTranslator<UChar>>(buffer);
}

PassRefPtr<StringImpl> AtomicStringTable::add(const UChar* s)
{
    if (!s)
        return nullptr;

    unsigned length = 0;
    while (s[length] != UChar(0))
        ++length;

    if (!length)
        return StringImpl::empty();

    UCharBuffer buffer = { s, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

struct SubstringLocation {
    StringImpl* baseString;
    unsigned start;
    unsigned length;
};

struct SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        if (buffer.baseString->is8Bit())
            return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters8() + buffer.start, buffer.length);
        return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters16() + buffer.start, buffer.length);
    }

    static bool equal(StringImpl* const& string, const SubstringLocation& buffer)
    {
        if (buffer.baseString->is8Bit())
            return WTF::equal(string, buffer.baseString->characters8() + buffer.start, buffer.length);
        return WTF::equal(string, buffer.baseString->characters16() + buffer.start, buffer.length);
    }

    static void translate(StringImpl*& location, const SubstringLocation& buffer, unsigned hash)
    {
        location = buffer.baseString->substring(buffer.start, buffer.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicStringTable::add(StringImpl* baseString, unsigned start, unsigned length)
{
    if (!baseString)
        return nullptr;

    if (!length || start >= baseString->length())
        return StringImpl::empty();

    unsigned maxLength = baseString->length() - start;
    if (length >= maxLength) {
        if (!start)
            return add(baseString);
        length = maxLength;
    }

    SubstringLocation buffer = { baseString, start, length };
    return addToStringTable<SubstringLocation, SubstringTranslator>(buffer);
}

typedef HashTranslatorCharBuffer<LChar> LCharBuffer;
struct LCharBufferTranslator {
    static unsigned hash(const LCharBuffer& buf)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const LCharBuffer& buf)
    {
        return WTF::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const LCharBuffer& buf, unsigned hash)
    {
        location = StringImpl::create(buf.s, buf.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicStringTable::add(const LChar* s, unsigned length)
{
    if (!s)
        return nullptr;

    if (!length)
        return StringImpl::empty();

    LCharBuffer buffer = { s, length };
    return addToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
}

StringImpl* AtomicStringTable::add(StringImpl* string)
{
    if (!string->length())
        return StringImpl::empty();

    StringImpl* result = *m_table.add(string).storedValue;

    if (!result->isAtomic())
        result->setIsAtomic(true);

    DCHECK(!string->isStatic() || result->isStatic());
    return result;
}

PassRefPtr<StringImpl> AtomicStringTable::addUTF8(const char* charactersStart, const char* charactersEnd)
{
    HashAndUTF8Characters buffer;
    buffer.characters = charactersStart;
    buffer.hash = calculateStringHashAndLengthFromUTF8MaskingTop8Bits(charactersStart, charactersEnd, buffer.length, buffer.utf16Length);

    if (!buffer.hash)
        return nullptr;

    return addToStringTable<HashAndUTF8Characters, HashAndUTF8CharactersTranslator>(buffer);
}

template<typename CharacterType>
HashSet<StringImpl*>::iterator AtomicStringTable::find(const StringImpl* string)
{
    HashAndCharacters<CharacterType> buffer = { string->existingHash(), string->getCharacters<CharacterType>(), string->length() };
    return m_table.find<HashAndCharactersTranslator<CharacterType>>(buffer);
}

void AtomicStringTable::remove(StringImpl* string)
{
    HashSet<StringImpl*>::iterator iterator;
    if (string->is8Bit())
        iterator = find<LChar>(string);
    else
        iterator = find<UChar>(string);
    RELEASE_ASSERT(iterator != m_table.end());
    m_table.remove(iterator);
}

} // namespace WTF
