// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JSONValuesForV8_h
#define JSONValuesForV8_h

#include "core/CoreExport.h"
#include "platform/JSONValues.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class ExceptionState;
class ScriptState;

CORE_EXPORT PassRefPtr<JSONValue> toJSONValue(v8::Local<v8::Context>, v8::Local<v8::Value>, int maxDepth = JSONValue::maxDepth);

CORE_EXPORT v8::Local<v8::Value> fromJSONString(ScriptState*, const String& stringifiedJSON, ExceptionState&);

} // namespace blink

#endif // JSONValuesForV8_h
