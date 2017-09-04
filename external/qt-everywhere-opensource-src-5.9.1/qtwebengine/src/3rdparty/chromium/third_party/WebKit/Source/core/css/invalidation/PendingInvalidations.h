// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PendingInvalidations_h
#define PendingInvalidations_h

#include "core/css/invalidation/InvalidationSet.h"

namespace blink {

class CORE_EXPORT PendingInvalidations final {
  WTF_MAKE_NONCOPYABLE(PendingInvalidations);

 public:
  PendingInvalidations() {}

  InvalidationSetVector& descendants() { return m_descendants; }
  const InvalidationSetVector& descendants() const { return m_descendants; }
  InvalidationSetVector& siblings() { return m_siblings; }
  const InvalidationSetVector& siblings() const { return m_siblings; }

 private:
  InvalidationSetVector m_descendants;
  InvalidationSetVector m_siblings;
};

}  // namespace blink

#endif  // PendingInvalidations_h
