// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistration_h
#define PropertyRegistration_h

#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PropertyDescriptor;

class PropertyRegistration {
  STATIC_ONLY(PropertyRegistration);

 public:
  static void registerProperty(ExecutionContext*,
                               const PropertyDescriptor&,
                               ExceptionState&);
};

}  // namespace blink

#endif  // PropertyRegistration_h
