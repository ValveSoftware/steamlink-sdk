/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/IdentifiersFactory.h"

#include "core/dom/WeakIdentifierMap.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/loader/DocumentLoader.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

volatile int s_lastUsedIdentifier = 0;

} // namespace


// static
String IdentifiersFactory::createIdentifier()
{
    int identifier = atomicIncrement(&s_lastUsedIdentifier);
    return addProcessIdPrefixTo(identifier);
}

// static
String IdentifiersFactory::requestId(unsigned long identifier)
{
    return identifier ? addProcessIdPrefixTo(identifier) : String();
}

// static
String IdentifiersFactory::frameId(LocalFrame* frame)
{
    return addProcessIdPrefixTo(WeakIdentifierMap<LocalFrame>::identifier(frame));
}

// static
LocalFrame* IdentifiersFactory::frameById(InspectedFrames* inspectedFrames, const String& frameId)
{
    bool ok;
    int id = removeProcessIdPrefixFrom(frameId, &ok);
    if (!ok)
        return nullptr;
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(id);
    return frame && inspectedFrames->contains(frame) ? frame : nullptr;
}

// static
String IdentifiersFactory::loaderId(DocumentLoader* loader)
{
    return addProcessIdPrefixTo(WeakIdentifierMap<DocumentLoader>::identifier(loader));
}

// static
DocumentLoader* IdentifiersFactory::loaderById(InspectedFrames* inspectedFrames, const String& loaderId)
{
    bool ok;
    int id = removeProcessIdPrefixFrom(loaderId, &ok);
    if (!ok)
        return nullptr;
    DocumentLoader* loader = WeakIdentifierMap<DocumentLoader>::lookup(id);
    LocalFrame* frame = loader->frame();
    return frame && inspectedFrames->contains(frame) ? loader : nullptr;
}

// static
String IdentifiersFactory::addProcessIdPrefixTo(int id)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(uint32_t, s_processId, new uint32_t(Platform::current()->getUniqueIdForProcess()));

    StringBuilder builder;

    builder.appendNumber(s_processId);
    builder.append('.');
    builder.appendNumber(id);

    return builder.toString();
}

// static
int IdentifiersFactory::removeProcessIdPrefixFrom(const String& id, bool* ok)
{
    size_t dotIndex = id.find('.');

    if (dotIndex == kNotFound) {
        *ok = false;
        return 0;
    }
    return id.substring(dotIndex + 1).toInt(ok);
}

} // namespace blink

