// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_EVENT_H_
#define UI_V2_PUBLIC_EVENT_H_

#include "base/basictypes.h"
#include "ui/v2/public/v2_export.h"

namespace v2 {

enum V2_EXPORT EventType {
  ET_NONE
};

class V2_EXPORT Event {
 public:
  Event();
  ~Event();

  EventType type() const { return type_; };

 private:
  EventType type_;

  DISALLOW_COPY_AND_ASSIGN(Event);
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_EVENT_H_
