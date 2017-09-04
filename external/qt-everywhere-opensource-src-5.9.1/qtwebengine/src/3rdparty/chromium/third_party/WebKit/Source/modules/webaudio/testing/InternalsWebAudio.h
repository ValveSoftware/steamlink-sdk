// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsWebAudio_h
#define InternalsWebAudio_h

#include "wtf/Allocator.h"

namespace blink {

class Internals;

class InternalsWebAudio {
  STATIC_ONLY(InternalsWebAudio);

 public:
  static unsigned audioHandlerCount(Internals&);
};

}  // namespace blink

#endif  // InternalsWebAudio_h
