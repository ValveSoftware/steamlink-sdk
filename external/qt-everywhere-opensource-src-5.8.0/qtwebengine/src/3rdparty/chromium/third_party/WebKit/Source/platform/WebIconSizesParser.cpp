// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebIconSizesParser.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "wtf/text/StringToNumber.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

namespace {

static inline bool isIntegerStart(UChar c)
{
    return c > '0' && c <= '9';
}

static bool isWhitespace(UChar c)
{
    // Sizes space characters are U+0020 SPACE, U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF), U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR).
    return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}

static bool isNotWhitespace(UChar c)
{
    return !isWhitespace(c);
}

static bool isNonDigit(UChar c)
{
    return !isASCIIDigit(c);
}

static inline size_t findEndOfWord(const String& string, size_t start)
{
    return std::min(string.find(isWhitespace, start), static_cast<size_t>(string.length()));
}

static inline int partialStringToInt(const String& string, size_t start, size_t end)
{
    if (string.is8Bit())
        return charactersToInt(string.characters8() + start, end - start);
    return charactersToInt(string.characters16() + start, end - start);
}

} // namespace

WebVector<WebSize> WebIconSizesParser::parseIconSizes(const WebString& webSizesString)
{
    String sizesString = webSizesString;
    Vector<WebSize> iconSizes;
    if (sizesString.isEmpty())
        return iconSizes;

    unsigned length = sizesString.length();
    for (unsigned i = 0; i < length; ++i) {
        // Skip whitespaces.
        i = std::min(sizesString.find(isNotWhitespace, i), static_cast<size_t>(length));
        if (i >= length)
            break;

        // See if the current size is "any".
        if (sizesString.findIgnoringCase("any", i) == i
            && (i + 3 == length || isWhitespace(sizesString[i + 3]))) {
            iconSizes.append(WebSize(0, 0));
            i = i + 3;
            continue;
        }

        // Parse the width.
        if (!isIntegerStart(sizesString[i])) {
            i = findEndOfWord(sizesString, i);
            continue;
        }
        unsigned widthStart = i;
        i = std::min(sizesString.find(isNonDigit, i), static_cast<size_t>(length));
        if (i >= length || (sizesString[i] != 'x' && sizesString[i] != 'X')) {
            i = findEndOfWord(sizesString, i);
            continue;
        }
        unsigned widthEnd = i++;

        // Parse the height.
        if (i >= length || !isIntegerStart(sizesString[i])) {
            i = findEndOfWord(sizesString, i);
            continue;
        }
        unsigned heightStart = i;
        i = std::min(sizesString.find(isNonDigit, i), static_cast<size_t>(length));
        if (i < length && !isWhitespace(sizesString[i])) {
            i = findEndOfWord(sizesString, i);
            continue;
        }
        unsigned heightEnd = i;

        // Append the parsed size to iconSizes.
        iconSizes.append(WebSize(
            partialStringToInt(sizesString, widthStart, widthEnd),
            partialStringToInt(sizesString, heightStart, heightEnd)));
    }
    return iconSizes;
}

} // namespace blink
