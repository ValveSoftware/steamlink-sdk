// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SerializedScriptValueForModulesFactory_h
#define SerializedScriptValueForModulesFactory_h

#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "wtf/Noncopyable.h"

namespace blink {

class SerializedScriptValueForModulesFactory final
    : public SerializedScriptValueFactory {
  USING_FAST_MALLOC(SerializedScriptValueForModulesFactory);
  WTF_MAKE_NONCOPYABLE(SerializedScriptValueForModulesFactory);

 public:
  SerializedScriptValueForModulesFactory() : SerializedScriptValueFactory() {}

  PassRefPtr<SerializedScriptValue> create(v8::Isolate*,
                                           v8::Local<v8::Value>,
                                           Transferables*,
                                           WebBlobInfoArray*,
                                           ExceptionState&) override;

 protected:
  v8::Local<v8::Value> deserialize(SerializedScriptValue*,
                                   v8::Isolate*,
                                   MessagePortArray*,
                                   const WebBlobInfoArray*) override;
};

}  // namespace blink

#endif  // SerializedScriptValueForModulesFactory_h
