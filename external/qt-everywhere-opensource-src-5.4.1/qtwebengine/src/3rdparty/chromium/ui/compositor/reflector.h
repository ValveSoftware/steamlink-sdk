// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_REFLECTOR_H_
#define UI_COMPOSITOR_REFLECTOR_H_

#include "base/memory/ref_counted.h"

namespace ui {

class Reflector : public base::RefCountedThreadSafe<Reflector> {
 public:
  Reflector() {}

  virtual void OnMirroringCompositorResized() {}

 protected:
  friend class base::RefCountedThreadSafe<Reflector>;
  virtual ~Reflector() {}

  DISALLOW_COPY_AND_ASSIGN(Reflector);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_REFLECTOR_H_
