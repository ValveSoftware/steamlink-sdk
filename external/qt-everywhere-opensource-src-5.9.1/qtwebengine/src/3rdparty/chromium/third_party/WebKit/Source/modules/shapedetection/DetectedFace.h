// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedFace_h
#define DetectedFace_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedFace final : public GarbageCollected<DetectedFace>,
                                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedFace* create();
  DOMRect* boundingBox() const;
  DECLARE_TRACE();

 private:
  Member<DOMRect> m_boundingBox;
};

}  // namespace blink

#endif  // DetectedFace_h
