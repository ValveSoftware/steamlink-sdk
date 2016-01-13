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

#ifndef LazyDecodingPixelRef_h
#define LazyDecodingPixelRef_h

#include "SkFlattenableBuffers.h"
#include "SkPixelRef.h"
#include "SkRect.h"
#include "SkSize.h"
#include "SkTypes.h"
#include "platform/PlatformInstrumentation.h"
#include "skia/ext/lazy_pixel_ref.h"

#include "wtf/RefPtr.h"

using skia::LazyPixelRef;

class SkData;

namespace WebCore {

class ImageFrameGenerator;
class ScaledImageFragment;

class LazyDecodingPixelRef FINAL : public LazyPixelRef {
public:
    LazyDecodingPixelRef(const SkImageInfo&, PassRefPtr<ImageFrameGenerator>, size_t index);
    virtual ~LazyDecodingPixelRef();

    SK_DECLARE_UNFLATTENABLE_OBJECT()

    PassRefPtr<ImageFrameGenerator> frameGenerator() const { return m_frameGenerator; }
    size_t frameIndex() const { return m_frameIndex; }

    // Returns true if the image might already be decoded in the cache.
    // Optimistic version of PrepareToDecode; requires less locking.
    virtual bool MaybeDecoded() OVERRIDE;
    virtual bool PrepareToDecode(const LazyPixelRef::PrepareParams&) OVERRIDE;
    virtual void Decode() OVERRIDE;

protected:
    // SkPixelRef implementation.
    virtual bool onNewLockPixels(LockRec*) OVERRIDE;
    virtual void onUnlockPixels() OVERRIDE;
    virtual bool onLockPixelsAreWritable() const OVERRIDE;

    virtual SkData* onRefEncodedData() OVERRIDE;

private:
    RefPtr<ImageFrameGenerator> m_frameGenerator;
    size_t m_frameIndex;

    const ScaledImageFragment* m_lockedImageResource;
    PlatformInstrumentation::LazyPixelRefTracker m_objectTracker;
};

} // namespace WebCore

#endif // LazyDecodingPixelRef_h_
