// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoHelper_h
#define MojoHelper_h

#include "base/message_loop/message_loop.h"

namespace blink {

// Used to get whether message loop is ready for current thread, to help
// blink::initialize() determining whether can initialize mojo stuff or not.
// TODO(leonhsl): http://crbug.com/660274 Remove this API by ensuring
// a message loop before calling blink::initialize().
inline bool canInitializeMojo() {
  return base::MessageLoop::current();
}

}  // namespace blink

#endif  // MojoHelper_h
