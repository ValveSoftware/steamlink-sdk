/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/LazyDecodingPixelRef.h"

#include "SkData.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ImageFrameGenerator.h"

namespace WebCore {

LazyDecodingPixelRef::LazyDecodingPixelRef(const SkImageInfo& info, PassRefPtr<ImageFrameGenerator> frameGenerator, size_t index)
    : LazyPixelRef(info)
    , m_frameGenerator(frameGenerator)
    , m_frameIndex(index)
    , m_lockedImageResource(0)
    , m_objectTracker(this)
{
}

LazyDecodingPixelRef::~LazyDecodingPixelRef()
{
}

SkData* LazyDecodingPixelRef::onRefEncodedData()
{
    // If the image has been clipped or scaled, do not return the original encoded data, since
    // on playback it will not be known how the clipping/scaling was done.
    RefPtr<SharedBuffer> buffer = nullptr;
    bool allDataReceived = false;
    m_frameGenerator->copyData(&buffer, &allDataReceived);
    if (buffer && allDataReceived) {
        SkData* skdata = SkData::NewWithCopy((void*)buffer->data(), buffer->size());
        return skdata;
    }
    return 0;
}

bool LazyDecodingPixelRef::onNewLockPixels(LockRec* rec)
{
    TRACE_EVENT_ASYNC_BEGIN0("webkit", "LazyDecodingPixelRef::onNewLockPixels", this);

    ASSERT(!m_lockedImageResource);

    SkISize size = m_frameGenerator->getFullSize();
    if (!ImageDecodingStore::instance()->lockCache(m_frameGenerator.get(), size, m_frameIndex, &m_lockedImageResource))
        m_lockedImageResource = 0;

    // Use ImageFrameGenerator to generate the image. It will lock the cache
    // entry for us.
    if (!m_lockedImageResource) {
        PlatformInstrumentation::willDecodeLazyPixelRef(getGenerationID());
        m_lockedImageResource = m_frameGenerator->decodeAndScale(size, m_frameIndex);
        PlatformInstrumentation::didDecodeLazyPixelRef();
    }
    if (!m_lockedImageResource)
        return false;

    ASSERT(!m_lockedImageResource->bitmap().isNull());
    ASSERT(m_lockedImageResource->scaledSize() == size);
    rec->fPixels = m_lockedImageResource->bitmap().getAddr(0, 0);
    rec->fColorTable = 0;
    rec->fRowBytes = m_lockedImageResource->bitmap().rowBytes();
    return true;
}

void LazyDecodingPixelRef::onUnlockPixels()
{
    if (m_lockedImageResource) {
        ImageDecodingStore::instance()->unlockCache(m_frameGenerator.get(), m_lockedImageResource);
        m_lockedImageResource = 0;
    }

    TRACE_EVENT_ASYNC_END0("webkit", "LazyDecodingPixelRef::lockPixels", this);
}

bool LazyDecodingPixelRef::onLockPixelsAreWritable() const
{
    return false;
}

bool LazyDecodingPixelRef::MaybeDecoded()
{
    return ImageDecodingStore::instance()->isCached(m_frameGenerator.get(), m_frameGenerator->getFullSize(), m_frameIndex);
}

bool LazyDecodingPixelRef::PrepareToDecode(const LazyPixelRef::PrepareParams& params)
{
    ASSERT(false);
    return false;
}

void LazyDecodingPixelRef::Decode()
{
    lockPixels();
    unlockPixels();
}


} // namespace blink
