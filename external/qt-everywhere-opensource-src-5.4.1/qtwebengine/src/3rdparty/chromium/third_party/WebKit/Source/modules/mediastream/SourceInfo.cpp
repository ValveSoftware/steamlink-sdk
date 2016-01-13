/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/mediastream/SourceInfo.h"

#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<SourceInfo> SourceInfo::create(const blink::WebSourceInfo& webSourceInfo)
{
    ASSERT(!webSourceInfo.isNull());
    return adoptRefWillBeNoop(new SourceInfo(webSourceInfo));
}

SourceInfo::SourceInfo(const blink::WebSourceInfo& webSourceInfo)
    : m_webSourceInfo(webSourceInfo)
{
    ScriptWrappable::init(this);
}

String SourceInfo::id() const
{
    return m_webSourceInfo.id();
}

String SourceInfo::kind() const
{
    switch (m_webSourceInfo.kind()) {
    case blink::WebSourceInfo::SourceKindAudio:
        return "audio";
    case blink::WebSourceInfo::SourceKindVideo:
        return "video";
    case blink::WebSourceInfo::SourceKindNone:
        return "none";
    }

    ASSERT_NOT_REACHED();
    return String();
}

String SourceInfo::label() const
{
    return m_webSourceInfo.label();
}

String SourceInfo::facing() const
{
    switch (m_webSourceInfo.facing()) {
    case blink::WebSourceInfo::VideoFacingModeNone:
        return String();
    case blink::WebSourceInfo::VideoFacingModeUser:
        return "user";
    case blink::WebSourceInfo::VideoFacingModeEnvironment:
        return "environment";
    }

    ASSERT_NOT_REACHED();
    return String();
}

} // namespace WebCore
