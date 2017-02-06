// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextCodecReplacement.h"

#include "wtf/PtrUtil.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace WTF {

TextCodecReplacement::TextCodecReplacement()
    : m_replacementErrorReturned(false)
{
}

void TextCodecReplacement::registerEncodingNames(EncodingNameRegistrar registrar)
{
    // The 'replacement' label itself should not be referenceable by
    // resources or script - it's a specification convenience - but much of
    // the wtf/text API asserts that an encoding name is a label for itself.
    // This is handled in TextEncoding by marking it as not valid.
    registrar("replacement", "replacement");

    registrar("csiso2022kr", "replacement");
    registrar("hz-gb-2312", "replacement");
    registrar("iso-2022-cn", "replacement");
    registrar("iso-2022-cn-ext", "replacement");
    registrar("iso-2022-kr", "replacement");
}

static std::unique_ptr<TextCodec> newStreamingTextDecoderReplacement(const TextEncoding&, const void*)
{
    return wrapUnique(new TextCodecReplacement);
}

void TextCodecReplacement::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("replacement", newStreamingTextDecoderReplacement, 0);
}

String TextCodecReplacement::decode(const char*, size_t length, FlushBehavior, bool, bool& sawError)
{
    // https://encoding.spec.whatwg.org/#replacement-decoder

    // 1. If byte is end-of-stream, return finished.
    if (!length)
        return String();

    // 2. If replacement error returned flag is unset, set the replacement
    // error returned flag and return error.
    if (!m_replacementErrorReturned) {
        m_replacementErrorReturned = true;
        sawError = true;
        return String(&replacementCharacter, 1);
    }

    // 3. Return finished.
    return String();
}

} // namespace WTF
