// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenizerInputStream_h
#define CSSTokenizerInputStream_h

#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSTokenizerInputStream {
    WTF_MAKE_NONCOPYABLE(CSSTokenizerInputStream);
    USING_FAST_MALLOC(CSSTokenizerInputStream);
public:
    explicit CSSTokenizerInputStream(String input);

    UChar peek(unsigned) const;
    UChar nextInputChar() const { return peek(0); }

    // For fast-path code, don't replace nulls with replacement characters
    UChar peekWithoutReplacement(unsigned lookaheadOffset) const
    {
        DCHECK((m_offset + lookaheadOffset) <= m_stringLength);
        if ((m_offset + lookaheadOffset) == m_stringLength)
            return '\0';
        return (*m_string)[m_offset + lookaheadOffset];
    }

    void advance(unsigned offset = 1) { m_offset += offset; }
    void pushBack(UChar cc)
    {
        --m_offset;
        DCHECK(nextInputChar() == cc);
    }

    double getDouble(unsigned start, unsigned end) const;

    template<bool characterPredicate(UChar)>
    unsigned skipWhilePredicate(unsigned offset)
    {
        while ((m_offset + offset) < m_stringLength && characterPredicate((*m_string)[m_offset + offset]))
            ++offset;
        return offset;
    }

    void advanceUntilNonWhitespace();

    unsigned length() const { return m_stringLength; }
    unsigned offset() const { return std::min(m_offset, m_stringLength); }
    StringView rangeAt(unsigned start, unsigned length) const;

private:
    size_t m_offset;
    const size_t m_stringLength;
    const RefPtr<StringImpl> m_string;
};

} // namespace blink

#endif // CSSTokenizerInputStream_h

