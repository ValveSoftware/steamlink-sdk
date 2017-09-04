/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#include "platform/text/TextEncodingDetector.h"

#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

bool detectTextEncoding(const char* data,
                        size_t length,
                        const char* hintEncodingName,
                        WTF::TextEncoding* detectedEncoding) {
  *detectedEncoding = WTF::TextEncoding();
  int consumedBytes;
  bool isReliable;
  Encoding encoding = CompactEncDet::DetectEncoding(
      data, length, nullptr, nullptr, nullptr,
      EncodingNameAliasToEncoding(hintEncodingName), UNKNOWN_LANGUAGE,
      CompactEncDet::WEB_CORPUS,
      false,  // Include 7-bit encodings to detect ISO-2022-JP
      &consumedBytes, &isReliable);
  if (encoding == UNKNOWN_ENCODING)
    return false;

  // 7-bit encodings (except ISO-2022-JP) are not supported in WHATWG encoding
  // standard. Mark them as ASCII to keep the raw bytes intact.
  switch (encoding) {
    case HZ_GB_2312:
    case ISO_2022_KR:
    case ISO_2022_CN:
    case UTF7:
      encoding = ASCII_7BIT;
      break;
    default:
      break;
  }
  *detectedEncoding = WTF::TextEncoding(MimeEncodingName(encoding));
  return true;
}

}  // namespace blink
