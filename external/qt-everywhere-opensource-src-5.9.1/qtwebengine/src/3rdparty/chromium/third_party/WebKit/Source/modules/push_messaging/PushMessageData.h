// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushMessageData_h
#define PushMessageData_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ArrayBufferOrArrayBufferViewOrUSVString;
class Blob;
class DOMArrayBuffer;
class ExceptionState;
class ScriptState;

class MODULES_EXPORT PushMessageData final
    : public GarbageCollectedFinalized<PushMessageData>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushMessageData* create(const String& data);
  static PushMessageData* create(
      const ArrayBufferOrArrayBufferViewOrUSVString& data);

  virtual ~PushMessageData();

  DOMArrayBuffer* arrayBuffer() const;
  Blob* blob() const;
  ScriptValue json(ScriptState*, ExceptionState&) const;
  String text() const;

  DECLARE_TRACE();

 private:
  PushMessageData(const char* data, unsigned bytesSize);

  Vector<char> m_data;
};

}  // namespace blink

#endif  // PushMessageData_h
