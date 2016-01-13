/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorStyleTextEditor.h"

#include "core/css/CSSPropertySourceData.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorStyleSheet.h"

namespace WebCore {

InspectorStyleTextEditor::InspectorStyleTextEditor(WillBeHeapVector<InspectorStyleProperty>* allProperties, const String& styleText, const NewLineAndWhitespace& format)
    : m_allProperties(allProperties)
    , m_styleText(styleText)
    , m_format(format)
{
}

void InspectorStyleTextEditor::insertProperty(unsigned index, const String& propertyText, unsigned styleBodyLength)
{
    long propertyStart = 0;

    bool insertLast = true;
    if (index < m_allProperties->size()) {
        const InspectorStyleProperty& property = m_allProperties->at(index);
        if (property.hasSource) {
            propertyStart = property.sourceData.range.start;
            // If inserting before a disabled property, it should be shifted, too.
            insertLast = false;
        }
    }

    bool insertFirstInSource = !m_allProperties->size() || !m_allProperties->at(0).hasSource;
    bool insertLastInSource = true;
    for (unsigned i = index, size = m_allProperties->size(); i < size; ++i) {
        const InspectorStyleProperty& property = m_allProperties->at(i);
        if (property.hasSource) {
            insertLastInSource = false;
            break;
        }
    }

    String textToSet = propertyText;

    int formattingPrependOffset = 0;
    if (insertLast && !insertFirstInSource) {
        propertyStart = styleBodyLength;
        if (propertyStart && textToSet.length()) {
            long curPos = propertyStart - 1; // The last position of style declaration, since propertyStart points past one.
            while (curPos && isHTMLSpace<UChar>(m_styleText[curPos]))
                --curPos;
            if (curPos) {
                bool terminated = m_styleText[curPos] == ';' || (m_styleText[curPos] == '/' && m_styleText[curPos - 1] == '*');
                if (!terminated) {
                    // Prepend a ";" to the property text if appending to a style declaration where
                    // the last property has no trailing ";".
                    textToSet.insert(";", 0);
                    formattingPrependOffset = 1;
                }
            }
        }
    }

    const String& formatLineFeed = m_format.first;
    const String& formatPropertyPrefix = m_format.second;
    if (insertLastInSource) {
        long formatPropertyPrefixLength = formatPropertyPrefix.length();
        if (!formattingPrependOffset && (propertyStart < formatPropertyPrefixLength || m_styleText.substring(propertyStart - formatPropertyPrefixLength, formatPropertyPrefixLength) != formatPropertyPrefix)) {
            textToSet.insert(formatPropertyPrefix, formattingPrependOffset);
            if (!propertyStart || !isHTMLLineBreak(m_styleText[propertyStart - 1]))
                textToSet.insert(formatLineFeed, formattingPrependOffset);
        }
        if (!isHTMLLineBreak(m_styleText[propertyStart]))
            textToSet = textToSet + formatLineFeed;
    } else {
        String fullPrefix = formatLineFeed + formatPropertyPrefix;
        long fullPrefixLength = fullPrefix.length();
        textToSet = textToSet + fullPrefix;
        if (insertFirstInSource && (propertyStart < fullPrefixLength || m_styleText.substring(propertyStart - fullPrefixLength, fullPrefixLength) != fullPrefix))
            textToSet.insert(fullPrefix, formattingPrependOffset);
    }
    m_styleText.insert(textToSet, propertyStart);
}

void InspectorStyleTextEditor::replaceProperty(unsigned index, const String& newText)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_allProperties->size());
    internalReplaceProperty(m_allProperties->at(index), newText);
}

void InspectorStyleTextEditor::internalReplaceProperty(const InspectorStyleProperty& property, const String& newText)
{
    const SourceRange& range = property.sourceData.range;
    long replaceRangeStart = range.start;
    long replaceRangeEnd = range.end;
    long newTextLength = newText.length();
    String finalNewText = newText;

    // Removing a property - remove preceding prefix.
    String fullPrefix = m_format.first + m_format.second;
    long fullPrefixLength = fullPrefix.length();
    if (!newTextLength && fullPrefixLength) {
        if (replaceRangeStart >= fullPrefixLength && m_styleText.substring(replaceRangeStart - fullPrefixLength, fullPrefixLength) == fullPrefix)
            replaceRangeStart -= fullPrefixLength;
    } else if (newTextLength) {
        if (isHTMLLineBreak(newText[newTextLength - 1])) {
            // Coalesce newlines of the original and new property values (to avoid a lot of blank lines while incrementally applying property values).
            bool foundNewline = false;
            bool isLastNewline = false;
            int i;
            int textLength = m_styleText.length();
            for (i = replaceRangeEnd; i < textLength && isSpaceOrNewline(m_styleText[i]); ++i) {
                isLastNewline = isHTMLLineBreak(m_styleText[i]);
                if (isLastNewline)
                    foundNewline = true;
                else if (foundNewline && !isLastNewline) {
                    replaceRangeEnd = i;
                    break;
                }
            }
            if (foundNewline && isLastNewline)
                replaceRangeEnd = i;
        }

        if (fullPrefixLength > replaceRangeStart || m_styleText.substring(replaceRangeStart - fullPrefixLength, fullPrefixLength) != fullPrefix)
            finalNewText.insert(fullPrefix, 0);
    }

    int replacedLength = replaceRangeEnd - replaceRangeStart;
    m_styleText.replace(replaceRangeStart, replacedLength, finalNewText);
}

} // namespace WebCore

