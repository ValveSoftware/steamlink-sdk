// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialsTest_h
#define OriginTrialsTest_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ScriptState;

// OriginTrialsTest is a very simple interface used for testing
// origin-trial-enabled features which are attached directly to interfaces at
// run-time.
class OriginTrialsTest : public GarbageCollectedFinalized<OriginTrialsTest>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OriginTrialsTest* create() { return new OriginTrialsTest(); }
  virtual ~OriginTrialsTest() = default;

  bool normalAttribute();
  static bool staticAttribute();

  bool throwingAttribute(ScriptState*, ExceptionState&);
  bool unconditionalAttribute();

  static const unsigned short kConstant = 1;

  DECLARE_TRACE();

 private:
  OriginTrialsTest() = default;
};

}  // namespace blink

#endif  // OriginTrialsTest_h
