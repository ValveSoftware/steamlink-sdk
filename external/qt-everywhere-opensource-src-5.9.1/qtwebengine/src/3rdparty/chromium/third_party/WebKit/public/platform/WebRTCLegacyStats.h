// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCLegacyStats_h
#define WebRTCLegacyStats_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebRTCLegacyStatsMemberIterator;

enum WebRTCLegacyStatsMemberType {
  WebRTCLegacyStatsMemberTypeInt,
  WebRTCLegacyStatsMemberTypeInt64,
  WebRTCLegacyStatsMemberTypeFloat,
  WebRTCLegacyStatsMemberTypeString,
  WebRTCLegacyStatsMemberTypeBool,
  WebRTCLegacyStatsMemberTypeId,
};

class WebRTCLegacyStats {
 public:
  virtual ~WebRTCLegacyStats() {}

  virtual WebString id() const = 0;
  virtual WebString type() const = 0;
  virtual double timestamp() const = 0;

  // The caller owns the iterator. The iterator must not be used after
  // the |WebRTCLegacyStats| that created it is destroyed.
  virtual WebRTCLegacyStatsMemberIterator* iterator() const = 0;
};

class WebRTCLegacyStatsMemberIterator {
 public:
  virtual ~WebRTCLegacyStatsMemberIterator() {}
  virtual bool isEnd() const = 0;
  virtual void next() = 0;

  virtual WebString name() const = 0;
  virtual WebRTCLegacyStatsMemberType type() const = 0;
  // Value getters. No conversion is performed; the function must match the
  // member's |type|.
  virtual int valueInt() const = 0;        // WebRTCLegacyStatsMemberTypeInt
  virtual int64_t valueInt64() const = 0;  // WebRTCLegacyStatsMemberTypeInt64
  virtual float valueFloat() const = 0;    // WebRTCLegacyStatsMemberTypeFloat
  virtual WebString valueString()
      const = 0;                       // WebRTCLegacyStatsMemberTypeString
  virtual bool valueBool() const = 0;  // WebRTCLegacyStatsMemberTypeBool

  // Converts the value to string (regardless of |type|).
  virtual WebString valueToString() const = 0;
};

}  // namespace blink

#endif  // WebRTCLegacyStats_h
