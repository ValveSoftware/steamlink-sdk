/*
 * Copyright (C) 2004, 2006, 2008, 2010 Apple Inc. All rights reserved.
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

#include "wtf/text/TextCodecUTF16.h"

#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/WTFString.h"
#include <memory>

using namespace std;

namespace WTF {

void TextCodecUTF16::registerEncodingNames(EncodingNameRegistrar registrar) {
  registrar("UTF-16LE", "UTF-16LE");
  registrar("UTF-16BE", "UTF-16BE");

  registrar("ISO-10646-UCS-2", "UTF-16LE");
  registrar("UCS-2", "UTF-16LE");
  registrar("UTF-16", "UTF-16LE");
  registrar("Unicode", "UTF-16LE");
  registrar("csUnicode", "UTF-16LE");
  registrar("unicodeFEFF", "UTF-16LE");

  registrar("unicodeFFFE", "UTF-16BE");
}

static std::unique_ptr<TextCodec> newStreamingTextDecoderUTF16LE(
    const TextEncoding&,
    const void*) {
  return makeUnique<TextCodecUTF16>(true);
}

static std::unique_ptr<TextCodec> newStreamingTextDecoderUTF16BE(
    const TextEncoding&,
    const void*) {
  return makeUnique<TextCodecUTF16>(false);
}

void TextCodecUTF16::registerCodecs(TextCodecRegistrar registrar) {
  registrar("UTF-16LE", newStreamingTextDecoderUTF16LE, 0);
  registrar("UTF-16BE", newStreamingTextDecoderUTF16BE, 0);
}

String TextCodecUTF16::decode(const char* bytes,
                              size_t length,
                              FlushBehavior flush,
                              bool,
                              bool& sawError) {
  // For compatibility reasons, ignore flush from fetch EOF.
  const bool reallyFlush = flush != DoNotFlush && flush != FetchEOF;

  if (!length) {
    if (reallyFlush && (m_haveLeadByte || m_haveLeadSurrogate)) {
      m_haveLeadByte = m_haveLeadSurrogate = false;
      sawError = true;
      return String(&replacementCharacter, 1);
    }
    return String();
  }

  const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes);
  const size_t numBytes = length + m_haveLeadByte;
  const bool willHaveExtraByte = numBytes & 1;
  const size_t numCharsIn = numBytes / 2;
  const size_t maxCharsOut = numCharsIn + (m_haveLeadSurrogate ? 1 : 0) +
                             (reallyFlush && willHaveExtraByte ? 1 : 0);

  StringBuffer<UChar> buffer(maxCharsOut);
  UChar* q = buffer.characters();

  for (size_t i = 0; i < numCharsIn; ++i) {
    UChar c;
    if (m_haveLeadByte) {
      c = m_littleEndian ? (m_leadByte | (p[0] << 8))
                         : ((m_leadByte << 8) | p[0]);
      m_haveLeadByte = false;
      ++p;
    } else {
      c = m_littleEndian ? (p[0] | (p[1] << 8)) : ((p[0] << 8) | p[1]);
      p += 2;
    }

    // TODO(jsbell): If necessary for performance, m_haveLeadByte handling
    // can be pulled out and this loop split into distinct cases for
    // big/little endian. The logic from here to the end of the loop is
    // constant with respect to m_haveLeadByte and m_littleEndian.

    if (m_haveLeadSurrogate && U_IS_TRAIL(c)) {
      *q++ = m_leadSurrogate;
      m_haveLeadSurrogate = false;
      *q++ = c;
    } else {
      if (m_haveLeadSurrogate) {
        m_haveLeadSurrogate = false;
        sawError = true;
        *q++ = replacementCharacter;
      }

      if (U_IS_LEAD(c)) {
        m_haveLeadSurrogate = true;
        m_leadSurrogate = c;
      } else if (U_IS_TRAIL(c)) {
        sawError = true;
        *q++ = replacementCharacter;
      } else {
        *q++ = c;
      }
    }
  }

  DCHECK(!m_haveLeadByte);
  if (willHaveExtraByte) {
    m_haveLeadByte = true;
    m_leadByte = p[0];
  }

  if (reallyFlush && (m_haveLeadByte || m_haveLeadSurrogate)) {
    m_haveLeadByte = m_haveLeadSurrogate = false;
    sawError = true;
    *q++ = replacementCharacter;
  }

  buffer.shrink(q - buffer.characters());

  return String::adopt(buffer);
}

CString TextCodecUTF16::encode(const UChar* characters,
                               size_t length,
                               UnencodableHandling) {
  // We need to be sure we can double the length without overflowing.
  // Since the passed-in length is the length of an actual existing
  // character buffer, each character is two bytes, and we know
  // the buffer doesn't occupy the entire address space, we can
  // assert here that doubling the length does not overflow size_t
  // and there's no need for a runtime check.
  ASSERT(length <= numeric_limits<size_t>::max() / 2);

  char* bytes;
  CString result = CString::createUninitialized(length * 2, bytes);

  // FIXME: CString is not a reasonable data structure for encoded UTF-16, which
  // will have null characters inside it. Perhaps the result of encode should
  // not be a CString.
  if (m_littleEndian) {
    for (size_t i = 0; i < length; ++i) {
      UChar c = characters[i];
      bytes[i * 2] = static_cast<char>(c);
      bytes[i * 2 + 1] = c >> 8;
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      UChar c = characters[i];
      bytes[i * 2] = c >> 8;
      bytes[i * 2 + 1] = static_cast<char>(c);
    }
  }

  return result;
}

CString TextCodecUTF16::encode(const LChar* characters,
                               size_t length,
                               UnencodableHandling) {
  // In the LChar case, we do actually need to perform this check in release. :)
  RELEASE_ASSERT(length <= numeric_limits<size_t>::max() / 2);

  char* bytes;
  CString result = CString::createUninitialized(length * 2, bytes);

  if (m_littleEndian) {
    for (size_t i = 0; i < length; ++i) {
      bytes[i * 2] = characters[i];
      bytes[i * 2 + 1] = 0;
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      bytes[i * 2] = 0;
      bytes[i * 2 + 1] = characters[i];
    }
  }

  return result;
}

}  // namespace WTF
