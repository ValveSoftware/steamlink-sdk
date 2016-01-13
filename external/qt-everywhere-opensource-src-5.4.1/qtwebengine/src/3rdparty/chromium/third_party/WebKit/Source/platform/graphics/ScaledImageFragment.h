/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ScaledImageFragment_h
#define ScaledImageFragment_h

#include "SkBitmap.h"
#include "SkRect.h"
#include "SkSize.h"

#include "platform/PlatformExport.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

// ScaledImageFragment is a scaled version of an image.
class PLATFORM_EXPORT ScaledImageFragment {
public:
    enum ImageGeneration {
        CompleteImage = 0,
        FirstPartialImage = 1,
    };

    static PassOwnPtr<ScaledImageFragment> createComplete(const SkISize& scaledSize, size_t index, const SkBitmap& bitmap)
    {
        return adoptPtr(new ScaledImageFragment(scaledSize, index, CompleteImage, bitmap));
    }

    static PassOwnPtr<ScaledImageFragment> createPartial(const SkISize& scaledSize, size_t index, size_t generation, const SkBitmap& bitmap)
    {
        return adoptPtr(new ScaledImageFragment(scaledSize, index, generation, bitmap));
    }

    ScaledImageFragment(const SkISize&, size_t index, size_t generation, const SkBitmap&);
    ~ScaledImageFragment();

    const SkISize& scaledSize() const { return m_scaledSize; }
    size_t index() const { return m_index; }
    size_t generation() const { return m_generation; }
    bool isComplete() const { return m_generation == CompleteImage; }
    const SkBitmap& bitmap() const { return m_bitmap; }
    SkBitmap& bitmap() { return m_bitmap; }

private:
    SkISize m_scaledSize;
    size_t m_index;
    size_t m_generation;
    SkBitmap m_bitmap;
};

} // namespace WebCore

#endif
