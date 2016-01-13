// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_WINDOW_H_
#define UI_V2_PUBLIC_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "ui/v2/public/v2_export.h"
#include "ui/v2/public/view.h"

namespace v2 {

// A Window is a View that has a nested View hierarchy.
// All Windows have Layers.
class V2_EXPORT Window : public View {
 public:
  Window();
  virtual ~Window();

  View* contents() { return contents_.get(); }

 private:
  // Nested view hierarchy.
  scoped_ptr<View> contents_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_WINDOW_H_
