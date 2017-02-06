/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLToken_h
#define HTMLToken_h

#include "core/dom/Attribute.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/Forward.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class DoctypeData {
    USING_FAST_MALLOC(DoctypeData);
    WTF_MAKE_NONCOPYABLE(DoctypeData);
public:
    DoctypeData()
        : m_hasPublicIdentifier(false)
        , m_hasSystemIdentifier(false)
        , m_forceQuirks(false)
    {
    }

    bool m_hasPublicIdentifier;
    bool m_hasSystemIdentifier;
    WTF::Vector<UChar> m_publicIdentifier;
    WTF::Vector<UChar> m_systemIdentifier;
    bool m_forceQuirks;
};

static inline Attribute* findAttributeInVector(Vector<Attribute>& attributes, const QualifiedName& name)
{
    for (unsigned i = 0; i < attributes.size(); ++i) {
        if (attributes.at(i).name().matches(name))
            return &attributes.at(i);
    }
    return 0;
}

class HTMLToken {
    WTF_MAKE_NONCOPYABLE(HTMLToken);
    USING_FAST_MALLOC(HTMLToken);
public:
    enum TokenType {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    class Attribute {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        class Range {
            DISALLOW_NEW();
        public:
            int start;
            int end;
        };

        AtomicString name() const { return AtomicString(m_name); }
        String nameAttemptStaticStringCreation() const { return attemptStaticStringCreation(m_name, Likely8Bit); }
        const Vector<UChar, 32>& nameAsVector() const { return m_name; }

        void appendToName(UChar c) { m_name.append(c); }

        PassRefPtr<StringImpl> value8BitIfNecessary() const { return StringImpl::create8BitIfPossible(m_value); }
        String value() const { return String(m_value); }

        void appendToValue(UChar c) { m_value.append(c); }
        void appendToValue(const String& value) { append(m_value, value); }
        void clearValue() { m_value.clear(); }

        const Range& nameRange() const { return m_nameRange; }
        const Range& valueRange() const { return m_valueRange; }
        Range& mutableNameRange() { return m_nameRange; }
        Range& mutableValueRange() { return m_valueRange; }

    private:
        Vector<UChar, 32> m_name;
        Vector<UChar, 32> m_value;
        Range m_nameRange;
        Range m_valueRange;
    };

    typedef Vector<Attribute, 10> AttributeList;

    // By using an inline capacity of 256, we avoid spilling over into an malloced buffer
    // approximately 99% of the time based on a non-scientific browse around a number of
    // popular web sites on 23 May 2013.
    typedef Vector<UChar, 256> DataVector;

    HTMLToken() { clear(); }

    void clear()
    {
        m_type = Uninitialized;
        m_range.start = 0;
        m_range.end = 0;
        m_baseOffset = 0;
        // Don't call Vector::clear() as that would destroy the
        // alloced VectorBuffer. If the innerHTML'd content has
        // two 257 character text nodes in a row, we'll needlessly
        // thrash malloc. When we finally finish the parse the
        // HTMLToken will be destroyed and the VectorBuffer released.
        m_data.shrink(0);
        m_orAllData = 0;
    }

    bool isUninitialized() { return m_type == Uninitialized; }
    TokenType type() const { return m_type; }

    void makeEndOfFile()
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndOfFile;
    }

    /* Range and offset methods exposed for HTMLSourceTracker and HTMLViewSourceParser */
    int startIndex() const { return m_range.start; }
    int endIndex() const { return m_range.end; }

    void setBaseOffset(int offset)
    {
        m_baseOffset = offset;
    }

    void end(int endOffset)
    {
        m_range.end = endOffset - m_baseOffset;
    }

    const DataVector& data() const
    {
        ASSERT(m_type == Character || m_type == Comment || m_type == StartTag || m_type == EndTag);
        return m_data;
    }

    bool isAll8BitData() const
    {
        return (m_orAllData <= 0xff);
    }

    const DataVector& name() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        return m_data;
    }

    void appendToName(UChar character)
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        ASSERT(character);
        m_data.append(character);
        m_orAllData |= character;
    }

    /* DOCTYPE Tokens */

    bool forceQuirks() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

    void setForceQuirks()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_forceQuirks = true;
    }

    void beginDOCTYPE()
    {
        ASSERT(m_type == Uninitialized);
        m_type = DOCTYPE;
        m_doctypeData = wrapUnique(new DoctypeData);
    }

    void beginDOCTYPE(UChar character)
    {
        ASSERT(character);
        beginDOCTYPE();
        m_data.append(character);
        m_orAllData |= character;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    const WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    const WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void setPublicIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasPublicIdentifier = true;
        m_doctypeData->m_publicIdentifier.clear();
    }

    void setSystemIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasSystemIdentifier = true;
        m_doctypeData->m_systemIdentifier.clear();
    }

    void appendToPublicIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasPublicIdentifier);
        m_doctypeData->m_publicIdentifier.append(character);
    }

    void appendToSystemIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasSystemIdentifier);
        m_doctypeData->m_systemIdentifier.append(character);
    }

    std::unique_ptr<DoctypeData> releaseDoctypeData()
    {
        return std::move(m_doctypeData);
    }

    /* Start/End Tag Tokens */

    bool selfClosing() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_selfClosing;
    }

    void setSelfClosing()
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_selfClosing = true;
    }

    void beginStartTag(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Uninitialized);
        m_type = StartTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
        m_orAllData |= character;
    }

    void beginEndTag(LChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
    }

    void beginEndTag(const Vector<LChar, 32>& characters)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.appendVector(characters);
    }

    void addNewAttribute()
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_attributes.grow(m_attributes.size() + 1);
        m_currentAttribute = &m_attributes.last();
#if ENABLE(ASSERT)
        m_currentAttribute->mutableNameRange().start = 0;
        m_currentAttribute->mutableNameRange().end = 0;
        m_currentAttribute->mutableValueRange().start = 0;
        m_currentAttribute->mutableValueRange().end = 0;
#endif
    }

    void beginAttributeName(int offset)
    {
        m_currentAttribute->mutableNameRange().start = offset - m_baseOffset;
    }

    void endAttributeName(int offset)
    {
        int index = offset - m_baseOffset;
        m_currentAttribute->mutableNameRange().end = index;
        m_currentAttribute->mutableValueRange().start = index;
        m_currentAttribute->mutableValueRange().end = index;
    }

    void beginAttributeValue(int offset)
    {
        m_currentAttribute->mutableValueRange().start = offset - m_baseOffset;
#if ENABLE(ASSERT)
        m_currentAttribute->mutableValueRange().end = 0;
#endif
    }

    void endAttributeValue(int offset)
    {
        m_currentAttribute->mutableValueRange().end = offset - m_baseOffset;
    }

    void appendToAttributeName(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == StartTag || m_type == EndTag);
        ASSERT(m_currentAttribute->nameRange().start);
        m_currentAttribute->appendToName(character);
    }

    void appendToAttributeValue(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == StartTag || m_type == EndTag);
        ASSERT(m_currentAttribute->valueRange().start);
        m_currentAttribute->appendToValue(character);
    }

    void appendToAttributeValue(size_t i, const String& value)
    {
        ASSERT(!value.isEmpty());
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_attributes[i].appendToValue(value);
    }

    const AttributeList& attributes() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_attributes;
    }

    const Attribute* getAttributeItem(const QualifiedName& name) const
    {
        for (unsigned i = 0; i < m_attributes.size(); ++i) {
            if (m_attributes.at(i).name() == name.localName())
                return &m_attributes.at(i);
        }
        return 0;
    }

    // Used by the XSSAuditor to nuke XSS-laden attributes.
    void eraseValueOfAttribute(size_t i)
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_attributes[i].clearValue();
    }

    /* Character Tokens */

    // Starting a character token works slightly differently than starting
    // other types of tokens because we want to save a per-character branch.
    void ensureIsCharacterToken()
    {
        ASSERT(m_type == Uninitialized || m_type == Character);
        m_type = Character;
    }

    const DataVector& characters() const
    {
        ASSERT(m_type == Character);
        return m_data;
    }

    void appendToCharacter(char character)
    {
        ASSERT(m_type == Character);
        m_data.append(character);
    }

    void appendToCharacter(UChar character)
    {
        ASSERT(m_type == Character);
        m_data.append(character);
        m_orAllData |= character;
    }

    void appendToCharacter(const Vector<LChar, 32>& characters)
    {
        ASSERT(m_type == Character);
        m_data.appendVector(characters);
    }

    /* Comment Tokens */

    const DataVector& comment() const
    {
        ASSERT(m_type == Comment);
        return m_data;
    }

    void beginComment()
    {
        ASSERT(m_type == Uninitialized);
        m_type = Comment;
    }

    void appendToComment(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Comment);
        m_data.append(character);
        m_orAllData |= character;
    }

    // Only for XSSAuditor
    void eraseCharacters()
    {
        ASSERT(m_type == Character);
        m_data.clear();
        m_orAllData = 0;
    }

private:
    TokenType m_type;
    Attribute::Range m_range; // Always starts at zero.
    int m_baseOffset;
    DataVector m_data;
    UChar m_orAllData;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes;

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;

    // For DOCTYPE
    std::unique_ptr<DoctypeData> m_doctypeData;
};

} // namespace blink

#endif
