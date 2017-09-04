/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebRange_h
#define WebRange_h

#include "public/platform/WebCommon.h"
#if BLINK_IMPLEMENTATION
#include "core/editing/EphemeralRange.h"
#endif

namespace blink {

class LocalFrame;
class PlainTextRange;
class Range;
class WebString;

class WebRange final {
 public:
  BLINK_EXPORT WebRange(int start, int length);
  BLINK_EXPORT WebRange() {}

  int startOffset() const { return m_start; }
  int endOffset() const { return m_end; }
  int length() const { return m_end - m_start; }

  bool isNull() const { return m_start == -1 && m_end == -1; }
  bool isEmpty() const { return m_start == m_end; }

#if BLINK_IMPLEMENTATION
  WebRange(const EphemeralRange&);
  WebRange(const PlainTextRange&);

  EphemeralRange createEphemeralRange(LocalFrame*) const;
#endif

 private:
  // Note that this also matches the values for gfx::Range::InvalidRange
  // for easy conversion.
  int m_start = -1;
  int m_end = -1;
};

}  // namespace blink

#endif
