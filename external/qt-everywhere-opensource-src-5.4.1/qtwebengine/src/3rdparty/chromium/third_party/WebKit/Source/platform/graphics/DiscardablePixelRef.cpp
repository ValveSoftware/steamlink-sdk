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

#include "config.h"
#include "platform/graphics/DiscardablePixelRef.h"

#include "public/platform/Platform.h"
#include <string.h>

namespace WebCore {

namespace {

// URI label for a discardable SkPixelRef.
const char labelDiscardable[] = "discardable";

} // namespace


bool DiscardablePixelRefAllocator::allocPixelRef(SkBitmap* dst, SkColorTable* ctable)
{
    // It should not be possible to have a non-null color table in Blink.
    ASSERT(!ctable);

    int64_t size = dst->computeSize64();
    if (size < 0 || !sk_64_isS32(size))
        return false;

    const SkImageInfo& info = dst->info();
    if (kUnknown_SkColorType == info.colorType())
        return false;

    SkAutoTUnref<DiscardablePixelRef> pixelRef(new DiscardablePixelRef(info, dst->rowBytes(), adoptPtr(new SkMutex())));
    if (pixelRef->allocAndLockDiscardableMemory(sk_64_asS32(size))) {
        pixelRef->setURI(labelDiscardable);
        dst->setPixelRef(pixelRef.get());
        // This method is only called when a DiscardablePixelRef is created to back a SkBitmap.
        // It is necessary to lock this SkBitmap to have a valid pointer to pixels. Otherwise,
        // this SkBitmap could be assigned to another SkBitmap and locking/unlocking the other
        // SkBitmap will make this one losing its pixels.
        dst->lockPixels();
        return true;
    }

    // Fallback to heap allocator if discardable memory is not available.
    return dst->allocPixels();
}

DiscardablePixelRef::DiscardablePixelRef(const SkImageInfo& info, size_t rowBytes, PassOwnPtr<SkMutex> mutex)
    : SkPixelRef(info, mutex.get())
    , m_lockedMemory(0)
    , m_mutex(mutex)
    , m_rowBytes(rowBytes)
{
}

DiscardablePixelRef::~DiscardablePixelRef()
{
}

bool DiscardablePixelRef::allocAndLockDiscardableMemory(size_t bytes)
{
    m_discardable = adoptPtr(blink::Platform::current()->allocateAndLockDiscardableMemory(bytes));
    if (m_discardable) {
        m_lockedMemory = m_discardable->data();
        return true;
    }
    return false;
}

bool DiscardablePixelRef::onNewLockPixels(LockRec* rec)
{
    if (!m_lockedMemory && m_discardable->lock())
        m_lockedMemory = m_discardable->data();

    if (m_lockedMemory) {
        rec->fPixels = m_lockedMemory;
        rec->fColorTable = 0;
        rec->fRowBytes = m_rowBytes;
        return true;
    }
    return false;
}

void DiscardablePixelRef::onUnlockPixels()
{
    if (m_lockedMemory)
        m_discardable->unlock();
    m_lockedMemory = 0;
}

bool DiscardablePixelRef::isDiscardable(SkPixelRef* pixelRef)
{
    return pixelRef && pixelRef->getURI() && !strcmp(pixelRef->getURI(), labelDiscardable);
}

} // namespace WebCore
