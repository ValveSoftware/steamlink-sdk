/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef UnicodeRangeSet_h
#define UnicodeRangeSet_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"
#include "wtf/text/WTFString.h"

namespace blink {

struct PLATFORM_EXPORT UnicodeRange final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  UnicodeRange(UChar32 from, UChar32 to) : m_from(from), m_to(to) {}

  UChar32 from() const { return m_from; }
  UChar32 to() const { return m_to; }
  bool contains(UChar32 c) const { return m_from <= c && c <= m_to; }
  bool operator<(const UnicodeRange& other) const {
    return m_from < other.m_from;
  }
  bool operator<(UChar32 c) const { return m_to < c; }
  bool operator==(const UnicodeRange& other) const {
    return other.m_from == m_from && other.m_to == m_to;
  };

 private:
  UChar32 m_from;
  UChar32 m_to;
};

class PLATFORM_EXPORT UnicodeRangeSet : public RefCounted<UnicodeRangeSet> {
 public:
  explicit UnicodeRangeSet(const Vector<UnicodeRange>&);
  UnicodeRangeSet(){};
  bool contains(UChar32) const;
  bool intersectsWith(const String&) const;
  bool isEntireRange() const { return m_ranges.isEmpty(); }
  size_t size() const { return m_ranges.size(); }
  const UnicodeRange& rangeAt(size_t i) const { return m_ranges[i]; }
  bool operator==(const UnicodeRangeSet& other) const;

 private:
  Vector<UnicodeRange> m_ranges;  // If empty, represents the whole code space.
};

}  // namespace blink

#endif
