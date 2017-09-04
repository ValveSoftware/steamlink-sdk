// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleEngineContext_h
#define StyleEngineContext_h

#include "core/dom/Document.h"

namespace blink {

class CORE_EXPORT StyleEngineContext {
 public:
  StyleEngineContext();
  ~StyleEngineContext() {}
  bool addedPendingSheetBeforeBody() const {
    return m_addedPendingSheetBeforeBody;
  }
  void addingPendingSheet(const Document&);

 private:
  bool m_addedPendingSheetBeforeBody : 1;
};

}  // namespace blink

#endif
