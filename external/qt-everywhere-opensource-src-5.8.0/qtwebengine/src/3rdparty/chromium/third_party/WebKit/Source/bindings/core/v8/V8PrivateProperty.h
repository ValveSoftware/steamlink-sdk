// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8PrivateProperty_h
#define V8PrivateProperty_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromiseProperties.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include <memory>
#include <v8.h>

namespace blink {

class ScriptState;
class ScriptWrappable;

// Apply |X| for each pair of (InterfaceName, PrivateKeyName).
#define V8_PRIVATE_PROPERTY_FOR_EACH(X) \
    X(CustomEvent, Detail) \
    X(DOMException, Error) \
    X(ErrorEvent, Error) \
    X(IDBObserver, Callback)              \
    X(IntersectionObserver, Callback)     \
    X(MessageEvent, CachedData) \
    X(MutationObserver, Callback)         \
    X(PerformanceObserver, Callback)      \
    X(PrivateScriptRunner, IsInitialized) \
    X(SameObject, NotificationActions) \
    X(SameObject, NotificationVibrate) \
    X(V8NodeFilterCondition, Filter)

// The getter's name for a private property.
#define V8_PRIVATE_PROPERTY_GETTER_NAME(InterfaceName, PrivateKeyName) \
    get##InterfaceName##PrivateKeyName

// The member variable's name for a private property.
#define V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, PrivateKeyName) \
    m_symbol##InterfaceName##PrivateKeyName

// The string used to create a private symbol.  Must be unique per V8 instance.
#define V8_PRIVATE_PROPERTY_SYMBOL_STRING(InterfaceName, PrivateKeyName) \
    #InterfaceName"#"#PrivateKeyName // NOLINT(whitespace/indent)

// Provides access to V8's private properties.
//
// Usage 1) Fast path to use a pre-registered symbol.
//   auto private = V8PrivateProperty::getMessageEventCachedData(isolate);
//   v8::Local<v8::Context> context = ...;
//   v8::Local<v8::Object> object = ...;
//   v8::Local<v8::Value> value = private.get(context, object);
//   value = ...;
//   private.set(context, object, value);
//
// Usage 2) Slow path to create a global private symbol.
//   const char symbolName[] = "Interface#PrivateKeyName";
//   auto private = V8PrivateProperty::createSymbol(isolate, symbolName, sizeof symbolName);
//   ...
class CORE_EXPORT V8PrivateProperty {
    USING_FAST_MALLOC(V8PrivateProperty);
    WTF_MAKE_NONCOPYABLE(V8PrivateProperty);
public:
    // Provides fast access to V8's private properties.
    //
    // Retrieving/creating a global private symbol from a string is very
    // expensive compared to get or set a private property.  This class
    // provides a way to cache a private symbol and re-use it.
    class CORE_EXPORT Symbol {
        STACK_ALLOCATED();
    public:
        bool hasValue(v8::Local<v8::Context> context, v8::Local<v8::Object> object) const
        {
            return v8CallBoolean(object->HasPrivate(context, m_privateSymbol));
        }

        // Returns the value of the private property if set, or undefined.
        v8::Local<v8::Value> getOrUndefined(v8::Local<v8::Context> context, v8::Local<v8::Object> object) const
        {
            return v8CallOrCrash(object->GetPrivate(context, m_privateSymbol));
        }

        // Returns the value of the private property if set, or an empty handle.
        v8::Local<v8::Value> get(v8::Local<v8::Context> context, v8::Local<v8::Object> object) const
        {
            if (!v8CallBoolean(object->HasPrivate(context, m_privateSymbol)))
                return v8::Local<v8::Value>();
            v8::Local<v8::Value> value;
            if (v8Call(object->GetPrivate(context, m_privateSymbol), value))
                return value;
            return v8::Local<v8::Value>();
        }

        bool set(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::Value> value) const
        {
            return v8CallBoolean(object->SetPrivate(context, m_privateSymbol, value));
        }

    private:
        friend class V8PrivateProperty;
        // The following classes are exceptionally allowed to call to
        // getFromMainWorld.
        friend class V8CustomEvent;
        friend class V8ServiceWorkerMessageEventInternal;

        explicit Symbol(v8::Local<v8::Private> privateSymbol)
            : m_privateSymbol(privateSymbol) { }

        // Only friend classes are allowed to use this API.
        v8::Local<v8::Value> getFromMainWorld(ScriptState*, ScriptWrappable*);

        v8::Local<v8::Private> m_privateSymbol;
    };

    static std::unique_ptr<V8PrivateProperty> create()
    {
        return wrapUnique(new V8PrivateProperty());
    }

#define V8_PRIVATE_PROPERTY_DEFINE_GETTER(InterfaceName, KeyName) \
    static Symbol V8_PRIVATE_PROPERTY_GETTER_NAME(InterfaceName, KeyName)(v8::Isolate* isolate) /* NOLINT(readability/naming/underscores) */ \
    { \
        V8PrivateProperty* privateProp = V8PerIsolateData::from(isolate)->privateProperty(); \
        if (UNLIKELY(privateProp->V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName).isEmpty())) { \
            privateProp->V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName).set( \
                isolate, \
                createV8Private( \
                    isolate, \
                    V8_PRIVATE_PROPERTY_SYMBOL_STRING(InterfaceName, KeyName), \
                    sizeof V8_PRIVATE_PROPERTY_SYMBOL_STRING(InterfaceName, KeyName))); /* NOLINT(readability/naming/underscores) */ \
        } \
        return Symbol(privateProp->V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName).newLocal(isolate)); \
    }
    V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DEFINE_GETTER)
#undef V8_PRIVATE_PROPERTY_DEFINE_GETTER

    static Symbol createSymbol(v8::Isolate* isolate, const char* symbol, size_t length)
    {
        return Symbol(createV8Private(isolate, symbol, length));
    }

private:
    V8PrivateProperty() { }

    static v8::Local<v8::Private> createV8Private(v8::Isolate*, const char* symbol, size_t length);

#define V8_PRIVATE_PROPERTY_DECLARE_MEMBER(InterfaceName, KeyName) \
    ScopedPersistent<v8::Private> V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName); // NOLINT(readability/naming/underscores)
    V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DECLARE_MEMBER)
#undef V8_PRIVATE_PROPERTY_DECLARE_MEMBER
};

} // namespace blink

#endif // V8PrivateProperty_h
