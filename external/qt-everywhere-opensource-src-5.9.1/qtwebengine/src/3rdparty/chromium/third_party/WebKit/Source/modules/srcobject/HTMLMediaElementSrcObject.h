// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementSrcObject_h
#define HTMLMediaElementSrcObject_h

#include "modules/ModulesExport.h"
#include "wtf/Allocator.h"

namespace blink {

class MediaStream;
class HTMLMediaElement;

class MODULES_EXPORT HTMLMediaElementSrcObject {
  STATIC_ONLY(HTMLMediaElementSrcObject);

 public:
  static MediaStream* srcObject(HTMLMediaElement&);
  static void setSrcObject(HTMLMediaElement&, MediaStream*);
};

}  // namespace blink

#endif
