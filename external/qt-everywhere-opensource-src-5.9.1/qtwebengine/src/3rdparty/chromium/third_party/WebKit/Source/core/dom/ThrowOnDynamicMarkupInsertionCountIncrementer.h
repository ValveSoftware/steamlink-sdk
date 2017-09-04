// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThrowOnDynamicMarkupInsertionCountIncrementer_h
#define ThrowOnDynamicMarkupInsertionCountIncrementer_h

#include "core/dom/Document.h"
#include "wtf/Allocator.h"

namespace blink {

class ThrowOnDynamicMarkupInsertionCountIncrementer {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ThrowOnDynamicMarkupInsertionCountIncrementer);

 public:
  explicit ThrowOnDynamicMarkupInsertionCountIncrementer(Document* document)
      : m_count(document ? &document->m_throwOnDynamicMarkupInsertionCount
                         : 0) {
    if (!m_count)
      return;
    ++(*m_count);
  }

  ~ThrowOnDynamicMarkupInsertionCountIncrementer() {
    if (!m_count)
      return;
    --(*m_count);
  }

 private:
  unsigned* m_count;
};

}  // namespace blink

#endif
