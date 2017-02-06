// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/Vector.h"
#include <memory>

class SkBitmap;

namespace blink {
class ImageDecoder;
class SharedBuffer;

using DecoderCreator = std::unique_ptr<ImageDecoder>(*)();
PassRefPtr<SharedBuffer> readFile(const char* fileName);
PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName);
unsigned hashBitmap(const SkBitmap&);
void createDecodingBaseline(DecoderCreator, SharedBuffer*, Vector<unsigned>* baselineHashes);
void testByteByByteDecode(DecoderCreator createDecoder, const char* file, size_t expectedFrameCount, int expectedRepetitionCount);
void testMergeBuffer(DecoderCreator createDecoder, const char* file);
} // namespace blink
