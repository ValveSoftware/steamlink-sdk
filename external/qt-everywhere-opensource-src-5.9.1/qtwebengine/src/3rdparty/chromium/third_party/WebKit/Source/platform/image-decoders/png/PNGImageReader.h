/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef PNGImageReader_h
#define PNGImageReader_h

#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "png.h"

#if !defined(PNG_LIBPNG_VER_MAJOR) || !defined(PNG_LIBPNG_VER_MINOR)
#error version error: compile against a versioned libpng.
#endif

#if PNG_LIBPNG_VER_MAJOR > 1 || \
    (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4)
#define JMPBUF(png_ptr) png_jmpbuf(png_ptr)
#else
#define JMPBUF(png_ptr) png_ptr->jmpbuf
#endif

namespace blink {

class SegmentReader;

class PLATFORM_EXPORT PNGImageReader final {
  USING_FAST_MALLOC(PNGImageReader);
  WTF_MAKE_NONCOPYABLE(PNGImageReader);

 public:
  PNGImageReader(PNGImageDecoder*, size_t offset);
  ~PNGImageReader();

  bool decode(const SegmentReader&, bool sizeOnly);
  png_structp pngPtr() const { return m_png; }
  png_infop infoPtr() const { return m_info; }

  size_t getReadOffset() const { return m_readOffset; }
  void setReadOffset(size_t offset) { m_readOffset = offset; }
  size_t currentBufferSize() const { return m_currentBufferSize; }
  bool decodingSizeOnly() const { return m_decodingSizeOnly; }
  void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
  bool hasAlpha() const { return m_hasAlpha; }

  png_bytep interlaceBuffer() const { return m_interlaceBuffer.get(); }
  void createInterlaceBuffer(int size) {
    m_interlaceBuffer = wrapArrayUnique(new png_byte[size]);
  }

 private:
  png_structp m_png;
  png_infop m_info;
  PNGImageDecoder* m_decoder;
  size_t m_readOffset;
  size_t m_currentBufferSize;
  bool m_decodingSizeOnly;
  bool m_hasAlpha;
  std::unique_ptr<png_byte[]> m_interlaceBuffer;
};

}  // namespace blink

#endif
