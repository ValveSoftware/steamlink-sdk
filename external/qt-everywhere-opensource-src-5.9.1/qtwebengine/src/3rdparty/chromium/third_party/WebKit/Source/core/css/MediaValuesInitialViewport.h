// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValuesInitialViewport_h
#define MediaValuesInitialViewport_h

#include "core/css/MediaValuesDynamic.h"

namespace blink {

class CORE_EXPORT MediaValuesInitialViewport final : public MediaValuesDynamic {
 public:
  static MediaValuesInitialViewport* create(LocalFrame&);

  double viewportWidth() const override;
  double viewportHeight() const override;

 private:
  MediaValuesInitialViewport(LocalFrame&);
};

}  // namespace blink

#endif  // MediaValuesInitialViewport_h
