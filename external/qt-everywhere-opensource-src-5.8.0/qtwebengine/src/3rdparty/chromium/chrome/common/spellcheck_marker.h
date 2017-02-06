// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_MARKER_H_
#define CHROME_COMMON_SPELLCHECK_MARKER_H_

#include <stddef.h>
#include <stdint.h>

class SpellCheckMarker {
 public:
  // A predicate to test spellcheck marker validity.
  class IsValidPredicate {
   public:
    typedef SpellCheckMarker argument_type;
    explicit IsValidPredicate(size_t text_length) : text_length_(text_length) {}
    bool operator()(const SpellCheckMarker& marker) const {
      return marker.offset < text_length_;
    }
   private:
    size_t text_length_;
  };

  // IPC requires a default constructor.
  SpellCheckMarker() : hash(0xFFFFFFFF), offset(UINT32_MAX) {}

  SpellCheckMarker(uint32_t hash, uint32_t offset)
      : hash(hash), offset(offset) {}

  uint32_t hash;
  // Note: we use uint32_t instead of size_t because this struct is sent over
  // IPC which could span 32 & 64 bit processes. This is fine since the offset
  // shouldn't exceed UINT32_MAX even on 64 bit builds.
  uint32_t offset;
};

#endif  // CHROME_COMMON_SPELLCHECK_MARKER_H_
