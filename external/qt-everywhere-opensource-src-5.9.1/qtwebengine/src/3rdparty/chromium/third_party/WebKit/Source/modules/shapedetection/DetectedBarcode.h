// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedBarcode_h
#define DetectedBarcode_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedBarcode final
    : public GarbageCollectedFinalized<DetectedBarcode>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedBarcode* create();
  static DetectedBarcode* create(String, DOMRect*);

  const String& rawValue() const;
  DOMRect* boundingBox() const;
  DECLARE_TRACE();

 private:
  DetectedBarcode(String rawValue, DOMRect* boundingBox);

  const String m_rawValue;
  const Member<DOMRect> m_boundingBox;
};

}  // namespace blink

#endif  // DetectedBarcode_h
