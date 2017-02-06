// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SerializedScriptValueFactory_h
#define SerializedScriptValueFactory_h

#include "bindings/core/v8/ScriptValueSerializer.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT SerializedScriptValueFactory {
    WTF_MAKE_NONCOPYABLE(SerializedScriptValueFactory);
    USING_FAST_MALLOC(SerializedScriptValueFactory);
public:
    // SerializedScriptValueFactory::initialize() should be invoked when Blink is initialized,
    // i.e. initialize() in WebKit.cpp.
    static void initialize(SerializedScriptValueFactory* newFactory)
    {
        DCHECK(!m_instance);
        m_instance = newFactory;
    }

protected:
    friend class SerializedScriptValue;

    // Following 2 methods are expected to be called by SerializedScriptValue.

    // If a serialization error occurs (e.g., cyclic input value) this
    // function returns an empty representation, schedules a V8 exception to
    // be thrown using v8::ThrowException(), and sets |didThrow|. In this case
    // the caller must not invoke any V8 operations until control returns to
    // V8. When serialization is successful, |didThrow| is false.
    virtual PassRefPtr<SerializedScriptValue> create(v8::Isolate*, v8::Local<v8::Value>, Transferables*, WebBlobInfoArray*, ExceptionState&);

    v8::Local<v8::Value> deserialize(SerializedScriptValue*, v8::Isolate*, MessagePortArray*, const WebBlobInfoArray*);

    // Following methods are expected to be called in SerializedScriptValueFactory{ForModules}.
    SerializedScriptValueFactory() { }
    virtual v8::Local<v8::Value> deserialize(String& data, BlobDataHandleMap& blobDataHandles, ArrayBufferContentsArray*, ImageBitmapContentsArray*, v8::Isolate*, MessagePortArray* messagePorts, const WebBlobInfoArray*);

private:
    static SerializedScriptValueFactory& instance()
    {
        if (!m_instance) {
            NOTREACHED();
            m_instance = new SerializedScriptValueFactory;
        }
        return *m_instance;
    }

    static SerializedScriptValueFactory* m_instance;
};

} // namespace blink

#endif // SerializedScriptValueFactory_h
