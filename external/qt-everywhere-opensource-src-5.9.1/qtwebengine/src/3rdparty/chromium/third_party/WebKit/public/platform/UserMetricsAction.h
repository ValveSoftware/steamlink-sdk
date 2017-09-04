// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UserMetricsAction_h
#define UserMetricsAction_h

#include "WebNonCopyable.h"

namespace blink {

// WebKit equivalent to base::UserMetricsAction.  Included here so that it's
// self-contained within WebKit.
class UserMetricsAction : public WebNonCopyable {
 public:
  explicit UserMetricsAction(const char* action) : m_action(action) {}
  const char* action() const { return m_action; }

 private:
  const char* const m_action;
};

}  // namespace blink
#endif
