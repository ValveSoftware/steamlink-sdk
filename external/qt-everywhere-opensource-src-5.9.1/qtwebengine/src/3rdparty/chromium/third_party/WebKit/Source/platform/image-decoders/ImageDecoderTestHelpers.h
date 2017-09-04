// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/Vector.h"
#include <memory>

class SkBitmap;

namespace blink {
class ImageDecoder;
class SharedBuffer;

const char decodersTestingDir[] = "Source/platform/image-decoders/testing";

using DecoderCreator = std::unique_ptr<ImageDecoder> (*)();
PassRefPtr<SharedBuffer> readFile(const char* fileName);
PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName);
unsigned hashBitmap(const SkBitmap&);
void createDecodingBaseline(DecoderCreator,
                            SharedBuffer*,
                            Vector<unsigned>* baselineHashes);

void testByteByByteDecode(DecoderCreator createDecoder,
                          const char* file,
                          size_t expectedFrameCount,
                          int expectedRepetitionCount);
void testByteByByteDecode(DecoderCreator createDecoder,
                          const char* dir,
                          const char* file,
                          size_t expectedFrameCount,
                          int expectedRepetitionCount);

void testMergeBuffer(DecoderCreator createDecoder, const char* file);
void testMergeBuffer(DecoderCreator createDecoder,
                     const char* dir,
                     const char* file);

// |skippingStep| is used to randomize the decoding order. For images with
// a small number of frames (e.g. < 10), this value should be smaller, on the
// order of (number of frames) / 2.
void testRandomFrameDecode(DecoderCreator,
                           const char* file,
                           size_t skippingStep = 5);
void testRandomFrameDecode(DecoderCreator,
                           const char* dir,
                           const char* file,
                           size_t skippingStep = 5);

// |skippingStep| is used to randomize the decoding order. For images with
// a small number of frames (e.g. < 10), this value should be smaller, on the
// order of (number of frames) / 2.
void testRandomDecodeAfterClearFrameBufferCache(DecoderCreator,
                                                const char* file,
                                                size_t skippingStep = 5);
void testRandomDecodeAfterClearFrameBufferCache(DecoderCreator,
                                                const char* dir,
                                                const char* file,
                                                size_t skippingStep = 5);

void testDecodeAfterReallocatingData(DecoderCreator, const char* file);
void testDecodeAfterReallocatingData(DecoderCreator,
                                     const char* dir,
                                     const char* file);
void testByteByByteSizeAvailable(DecoderCreator,
                                 const char* file,
                                 size_t frameOffset,
                                 bool hasColorSpace,
                                 int expectedRepetitionCount);
void testByteByByteSizeAvailable(DecoderCreator,
                                 const char* dir,
                                 const char* file,
                                 size_t frameOffset,
                                 bool hasColorSpace,
                                 int expectedRepetitionCount);

// Data is provided in chunks of length |increment| to the decoder. This value
// can be increased to reduce processing time.
void testProgressiveDecoding(DecoderCreator,
                             const char* file,
                             size_t increment = 1);
void testProgressiveDecoding(DecoderCreator,
                             const char* dir,
                             const char* file,
                             size_t increment = 1);
}  // namespace blink
