/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/TextResourceDecoderBuilder.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "platform/weborigin/SecurityOrigin.h"
#include <memory>

namespace blink {

static inline bool canReferToParentFrameEncoding(const LocalFrame* frame, const LocalFrame* parentFrame)
{
    return parentFrame && parentFrame->document()->getSecurityOrigin()->canAccess(frame->document()->getSecurityOrigin());
}

namespace {

struct LegacyEncoding {
    const char* domain;
    const char* encoding;
};

static const LegacyEncoding encodings[] = {
    { "au", "windows-1252" },
    { "az", "ISO-8859-9" },
    { "bd", "windows-1252" },
    { "bg", "windows-1251" },
    { "br", "windows-1252" },
    { "ca", "windows-1252" },
    { "ch", "windows-1252" },
    { "cn", "GBK" },
    { "cz", "windows-1250" },
    { "de", "windows-1252" },
    { "dk", "windows-1252" },
    { "ee", "windows-1256" },
    { "eg", "windows-1257" },
    { "et", "windows-1252" },
    { "fi", "windows-1252" },
    { "fr", "windows-1252" },
    { "gb", "windows-1252" },
    { "gr", "ISO-8859-7" },
    { "hk", "Big5" },
    { "hr", "windows-1250" },
    { "hu", "ISO-8859-2" },
    { "il", "windows-1255" },
    { "ir", "windows-1257" },
    { "is", "windows-1252" },
    { "it", "windows-1252" },
    { "jp", "Shift_JIS" },
    { "kr", "windows-949" },
    { "lt", "windows-1256" },
    { "lv", "windows-1256" },
    { "mk", "windows-1251" },
    { "nl", "windows-1252" },
    { "no", "windows-1252" },
    { "pl", "ISO-8859-2" },
    { "pt", "windows-1252" },
    { "ro", "ISO-8859-2" },
    { "rs", "windows-1251" },
    { "ru", "windows-1251" },
    { "se", "windows-1252" },
    { "si", "ISO-8859-2" },
    { "sk", "windows-1250" },
    { "th", "windows-874" },
    { "tr", "ISO-8859-9" },
    { "tw", "Big5" },
    { "tz", "windows-1252" },
    { "ua", "windows-1251" },
    { "us", "windows-1252" },
    { "vn", "windows-1258" },
    { "xa", "windows-1252" },
    { "xb", "windows-1257" }
};

static const WTF::TextEncoding getEncodingFromDomain(const KURL& url)
{
    Vector<String> tokens;
    url.host().split(".", tokens);
    if (!tokens.isEmpty()) {
        auto tld = tokens.last();
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(encodings); i++) {
            if (tld == encodings[i].domain)
                return WTF::TextEncoding(encodings[i].encoding);
        }
    }
    return WTF::TextEncoding();
}

} // namespace

TextResourceDecoderBuilder::TextResourceDecoderBuilder(const AtomicString& mimeType, const AtomicString& encoding)
    : m_mimeType(mimeType)
    , m_encoding(encoding)
{
}

TextResourceDecoderBuilder::~TextResourceDecoderBuilder()
{
}


inline std::unique_ptr<TextResourceDecoder> TextResourceDecoderBuilder::createDecoderInstance(Document* document)
{
    const WTF::TextEncoding encodingFromDomain = getEncodingFromDomain(document->url());
    if (LocalFrame* frame = document->frame()) {
        if (Settings* settings = frame->settings())
            return TextResourceDecoder::create(m_mimeType, encodingFromDomain.isValid() ? encodingFromDomain : settings->defaultTextEncodingName(), settings->usesEncodingDetector());
    }

    return TextResourceDecoder::create(m_mimeType, encodingFromDomain);
}

inline void TextResourceDecoderBuilder::setupEncoding(TextResourceDecoder* decoder, Document* document)
{
    LocalFrame* frame = document->frame();
    LocalFrame* parentFrame = 0;
    if (frame && frame->tree().parent() && frame->tree().parent()->isLocalFrame())
        parentFrame = toLocalFrame(frame->tree().parent());

    if (!m_encoding.isEmpty())
        decoder->setEncoding(m_encoding.getString(), TextResourceDecoder::EncodingFromHTTPHeader);

    // Set the hint encoding to the parent frame encoding only if
    // the parent and the current frames share the security origin.
    // We impose this condition because somebody can make a child frameg63
    // containing a carefully crafted html/javascript in one encoding
    // that can be mistaken for hintEncoding (or related encoding) by
    // an auto detector. When interpreted in the latter, it could be
    // an attack vector.
    // FIXME: This might be too cautious for non-7bit-encodings and
    // we may consider relaxing this later after testing.
    if (frame && canReferToParentFrameEncoding(frame, parentFrame)) {
        if (parentFrame->document()->encodingWasDetectedHeuristically())
            decoder->setHintEncoding(parentFrame->document()->encoding());

        if (m_encoding.isEmpty())
            decoder->setEncoding(parentFrame->document()->encoding(), TextResourceDecoder::EncodingFromParentFrame);
    }
}

std::unique_ptr<TextResourceDecoder> TextResourceDecoderBuilder::buildFor(Document* document)
{
    std::unique_ptr<TextResourceDecoder> decoder = createDecoderInstance(document);
    setupEncoding(decoder.get(), document);
    return decoder;
}

void TextResourceDecoderBuilder::clear()
{
    m_encoding = nullAtom;
}

} // namespace blink
