/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef JPEGImageEncoder_h
#define JPEGImageEncoder_h

#include "platform/geometry/IntSize.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

struct ImageDataBuffer;

class PLATFORM_EXPORT JPEGImageEncoderState {
    USING_FAST_MALLOC(JPEGImageEncoderState);
    WTF_MAKE_NONCOPYABLE(JPEGImageEncoderState);
public:
    static std::unique_ptr<JPEGImageEncoderState> create(const IntSize& imageSize, const double& quality, Vector<unsigned char>* output);
    JPEGImageEncoderState() {}
    virtual ~JPEGImageEncoderState() {}
};

class PLATFORM_EXPORT JPEGImageEncoder {
    STATIC_ONLY(JPEGImageEncoder);
public:
    enum {
        ProgressiveEncodeFailed = -1
    };

    // Encode the image data with a compression quality in [0-100].
    // Warning: Calling this method off the main thread may result in data race
    // problems; instead, call JPEGImageEncoderState::create on main thread
    // first followed by encodeWithPreInitializedState off the main thread will
    // be safer.
    static bool encode(const ImageDataBuffer&, const double& quality, Vector<unsigned char>*);

    static bool encodeWithPreInitializedState(std::unique_ptr<JPEGImageEncoderState>, const unsigned char*, int numRowsCompleted = 0);
    static int progressiveEncodeRowsJpegHelper(JPEGImageEncoderState*, unsigned char*, int, const double, double);
    static int computeCompressionQuality(const double& quality);

private:
    // For callers: provide a reasonable compression quality default.
    enum Quality { DefaultCompressionQuality = 92 };
};

} // namespace blink

#endif
