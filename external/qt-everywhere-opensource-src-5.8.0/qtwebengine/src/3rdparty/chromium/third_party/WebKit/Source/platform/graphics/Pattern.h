/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2013 Google, Inc. All rights reserved.
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

#ifndef Pattern_h
#define Pattern_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Image.h"
#include "third_party/skia/include/core/SkRefCnt.h"

#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

class SkMatrix;
class SkPaint;
class SkPicture;

namespace blink {

class PLATFORM_EXPORT Pattern : public RefCounted<Pattern> {
    WTF_MAKE_NONCOPYABLE(Pattern);
public:
    enum RepeatMode {
        RepeatModeX    = 1 << 0,
        RepeatModeY    = 1 << 1,

        RepeatModeNone = 0,
        RepeatModeXY   = RepeatModeX | RepeatModeY
    };

    static PassRefPtr<Pattern> createImagePattern(PassRefPtr<Image>, RepeatMode = RepeatModeXY);
    static PassRefPtr<Pattern> createPicturePattern(PassRefPtr<SkPicture>,
        RepeatMode = RepeatModeXY);
    virtual ~Pattern();

    void applyToPaint(SkPaint&, const SkMatrix&) const;

    bool isRepeatX() const { return m_repeatMode & RepeatModeX; }
    bool isRepeatY() const { return m_repeatMode & RepeatModeY; }
    bool isRepeatXY() const { return m_repeatMode == RepeatModeXY; }

    virtual bool isTextureBacked() const { return false; }

protected:
    virtual sk_sp<SkShader> createShader(const SkMatrix&) const = 0;

    void adjustExternalMemoryAllocated(int64_t delta);

    RepeatMode m_repeatMode;

    Pattern(RepeatMode, int64_t externalMemoryAllocated = 0);

private:
    mutable sk_sp<SkShader> m_cachedShader;
    int64_t m_externalMemoryAllocated;
};

} // namespace blink

#endif
