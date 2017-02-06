/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8ValueCache_h
#define V8ValueCache_h

#include "bindings/core/v8/V8GlobalValueMap.h"
#include "core/CoreExport.h"
#include "platform/text/CompressibleString.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class StringCacheMapTraits : public V8GlobalValueMapTraits<StringImpl*, v8::String, v8::kWeakWithParameter> {
    STATIC_ONLY(StringCacheMapTraits);
public:
    // Weak traits:
    typedef StringImpl WeakCallbackDataType;
    typedef v8::GlobalValueMap<StringImpl*, v8::String, StringCacheMapTraits> MapType;

    static WeakCallbackDataType* WeakCallbackParameter(
        MapType* map, StringImpl* key, v8::Local<v8::String>& value) { return key; }
    static void DisposeCallbackData(WeakCallbackDataType* callbackData) { }

    static MapType* MapFromWeakCallbackInfo(
        const v8::WeakCallbackInfo<WeakCallbackDataType>&);

    static StringImpl* KeyFromWeakCallbackInfo(
        const v8::WeakCallbackInfo<WeakCallbackDataType>& data)
    {
        return data.GetParameter();
    }

    static void OnWeakCallback(const v8::WeakCallbackInfo<WeakCallbackDataType>&);

    static void Dispose(v8::Isolate*, v8::Global<v8::String> value, StringImpl* key);
    static void DisposeWeak(const v8::WeakCallbackInfo<WeakCallbackDataType>&);
};

class CompressibleStringCacheMapTraits : public V8GlobalValueMapTraits<CompressibleStringImpl*, v8::String, v8::kWeakWithParameter> {
    STATIC_ONLY(CompressibleStringCacheMapTraits);
public:
    // Weak traits:
    typedef CompressibleStringImpl WeakCallbackDataType;
    typedef v8::GlobalValueMap<CompressibleStringImpl*, v8::String, CompressibleStringCacheMapTraits> MapType;

    static WeakCallbackDataType* WeakCallbackParameter(
        MapType* map, CompressibleStringImpl* key, v8::Local<v8::String>& value) { return key; }
    static void DisposeCallbackData(WeakCallbackDataType* callbackData) { }

    static MapType* MapFromWeakCallbackInfo(
        const v8::WeakCallbackInfo<WeakCallbackDataType>&);

    static CompressibleStringImpl* KeyFromWeakCallbackInfo(
        const v8::WeakCallbackInfo<WeakCallbackDataType>& data)
    {
        return data.GetParameter();
    }

    static void OnWeakCallback(const v8::WeakCallbackInfo<WeakCallbackDataType>&);

    static void Dispose(v8::Isolate*, v8::Global<v8::String> value, CompressibleStringImpl* key);
    static void DisposeWeak(const v8::WeakCallbackInfo<WeakCallbackDataType>&);
};

class CORE_EXPORT StringCache {
    USING_FAST_MALLOC(StringCache);
    WTF_MAKE_NONCOPYABLE(StringCache);
public:
    explicit StringCache(v8::Isolate* isolate)
        : m_stringCache(isolate)
        , m_compressibleStringCache(isolate)
    {
    }

    v8::Local<v8::String> v8ExternalString(v8::Isolate* isolate, StringImpl* stringImpl)
    {
        ASSERT(stringImpl);
        if (m_lastStringImpl.get() == stringImpl)
            return m_lastV8String.NewLocal(isolate);
        return v8ExternalStringSlow(isolate, stringImpl);
    }

    v8::Local<v8::String> v8ExternalString(v8::Isolate* isolate, const CompressibleString& string)
    {
        // Note that the last CompressibleString is not cached.
        ASSERT(!string.isNull());
        return v8ExternalStringSlow(isolate, string);
    }

    void setReturnValueFromString(v8::ReturnValue<v8::Value> returnValue, StringImpl* stringImpl)
    {
        ASSERT(stringImpl);
        if (m_lastStringImpl.get() == stringImpl)
            m_lastV8String.SetReturnValue(returnValue);
        else
            setReturnValueFromStringSlow(returnValue, stringImpl);
    }

    void dispose();

    friend class StringCacheMapTraits;
    friend class CompressibleStringCacheMapTraits;

private:
    v8::Local<v8::String> v8ExternalStringSlow(v8::Isolate*, StringImpl*);
    v8::Local<v8::String> v8ExternalStringSlow(v8::Isolate*, const CompressibleString&);
    void setReturnValueFromStringSlow(v8::ReturnValue<v8::Value>, StringImpl*);
    v8::Local<v8::String> createStringAndInsertIntoCache(v8::Isolate*, StringImpl*);
    v8::Local<v8::String> createStringAndInsertIntoCache(v8::Isolate*, const CompressibleString&);
    void InvalidateLastString();

    StringCacheMapTraits::MapType m_stringCache;
    StringCacheMapTraits::MapType::PersistentValueReference m_lastV8String;

    CompressibleStringCacheMapTraits::MapType m_compressibleStringCache;

    // Note: RefPtr is a must as we cache by StringImpl* equality, not identity
    // hence lastStringImpl might be not a key of the cache (in sense of identity)
    // and hence it's not refed on addition.
    RefPtr<StringImpl> m_lastStringImpl;
};

} // namespace blink

#endif // V8ValueCache_h
