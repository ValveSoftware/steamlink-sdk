// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_

#include "components/previews/core/previews_experiments.h"

namespace net {
class URLRequest;
}

namespace previews {

class PreviewsDecider {
 public:
  // Whether |request| is allowed to show a preview of |type|.
  virtual bool ShouldAllowPreview(const net::URLRequest& request,
                                  PreviewsType type) const = 0;

 protected:
  PreviewsDecider() {}
  virtual ~PreviewsDecider() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
