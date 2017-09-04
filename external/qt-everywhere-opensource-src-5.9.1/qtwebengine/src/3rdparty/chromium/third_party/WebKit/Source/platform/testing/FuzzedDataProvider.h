// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FuzzedDataProvider_h
#define FuzzedDataProvider_h

#include "base/test/fuzzed_data_provider.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/CString.h"

namespace blink {

// This class simply wraps //base/test/fuzzed_data_provider and vends Blink
// friendly types.
class FuzzedDataProvider {
  WTF_MAKE_NONCOPYABLE(FuzzedDataProvider);

 public:
  FuzzedDataProvider(const uint8_t* bytes, size_t numBytes);

  // Returns a string with length between minBytes and maxBytes. If the
  // length is greater than the length of the remaining data this is
  // equivalent to ConsumeRemainingBytes().
  CString ConsumeBytesInRange(uint32_t minBytes, uint32_t maxBytes);

  // Returns a String containing all remaining bytes of the input data.
  CString ConsumeRemainingBytes();

  // Returns a bool, or false when no data remains.
  bool ConsumeBool();

 private:
  base::FuzzedDataProvider m_provider;
};

}  // namespace blink

#endif  // FuzzedDataProvider_h
