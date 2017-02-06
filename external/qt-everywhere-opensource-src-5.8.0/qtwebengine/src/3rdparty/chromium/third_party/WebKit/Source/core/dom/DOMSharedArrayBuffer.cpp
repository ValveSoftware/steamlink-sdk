// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMSharedArrayBuffer.h"

#include "bindings/core/v8/DOMDataStore.h"

namespace blink {

v8::Local<v8::Object> DOMSharedArrayBuffer::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    DCHECK(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Local<v8::Object> wrapper = v8::SharedArrayBuffer::New(isolate, data(), byteLength());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

} // namespace blink
