// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMArrayBuffer.h"

#include "bindings/core/v8/DOMDataStore.h"

namespace blink {

v8::Local<v8::Object> DOMArrayBuffer::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    DCHECK(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Local<v8::Object> wrapper = v8::ArrayBuffer::New(isolate, data(), byteLength());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

} // namespace blink
