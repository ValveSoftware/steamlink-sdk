// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_CLIENT_H_
#define CC_LAYERS_LAYER_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CC_EXPORT LayerClient {
 public:
  // Returns a pointer to a debug info object, if one has been computed.
  // If not, returns NULL. If the returned pointer is non-NULL, the caller takes
  // ownership of the pointer.
  virtual scoped_refptr<base::debug::ConvertableToTraceFormat>
      TakeDebugInfo() = 0;

 protected:
  virtual ~LayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_CLIENT_H_
