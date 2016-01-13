/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Dictionary_h
#define Dictionary_h

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/Nullable.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8BindingMacros.h"
#include "core/events/EventListener.h"
#include "core/dom/MessagePort.h"
#include <v8.h>
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ArrayValue;
class DOMError;
class LocalDOMWindow;
class Gamepad;
class MediaStream;
class HeaderMap;
class IDBKeyRange;
class MIDIPort;
class MediaKeyError;
class Notification;
class SpeechRecognitionResult;
class SpeechRecognitionResultList;
class Storage;
class TrackBase;
class VoidCallback;

class Dictionary {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    Dictionary();
    Dictionary(const v8::Handle<v8::Value>& options, v8::Isolate*);
    ~Dictionary();

    Dictionary& operator=(const Dictionary&);

    bool isObject() const;
    bool isUndefinedOrNull() const;

    bool get(const String&, bool&) const;
    bool get(const String&, int32_t&) const;
    bool get(const String&, double&, bool& hasValue) const;
    bool get(const String&, double&) const;
    bool get(const String&, String&) const;
    bool get(const String&, AtomicString&) const;
    bool get(const String&, ScriptValue&) const;
    bool get(const String&, short&) const;
    bool get(const String&, unsigned short&) const;
    bool get(const String&, unsigned&) const;
    bool get(const String&, unsigned long&) const;
    bool get(const String&, unsigned long long&) const;
    bool get(const String&, RefPtrWillBeMember<LocalDOMWindow>&) const;
    bool get(const String&, RefPtrWillBeMember<Storage>&) const;
    bool get(const String&, MessagePortArray&) const;
    bool get(const String&, RefPtr<Uint8Array>&) const;
    bool get(const String&, RefPtr<ArrayBufferView>&) const;
    bool get(const String&, RefPtrWillBeMember<MIDIPort>&) const;
    bool get(const String&, RefPtr<MediaKeyError>&) const;
    bool get(const String&, RefPtrWillBeMember<TrackBase>&) const;
    bool get(const String&, Member<SpeechRecognitionResult>&) const;
    bool get(const String&, Member<SpeechRecognitionResultList>&) const;
    bool get(const String&, Member<Gamepad>&) const;
    bool get(const String&, RefPtr<MediaStream>&) const;
    bool get(const String&, RefPtrWillBeMember<EventTarget>&) const;
    bool get(const String&, HashSet<AtomicString>&) const;
    bool get(const String&, Dictionary&) const;
    bool get(const String&, Vector<String>&) const;
    bool get(const String&, ArrayValue&) const;
    bool get(const String&, RefPtrWillBeMember<DOMError>&) const;
    bool get(const String&, v8::Local<v8::Value>&) const;
    bool get(const String&, RefPtr<HeaderMap>&) const;

    class ConversionContext {
    public:
        ConversionContext(const String& interfaceName, const String& methodName, ExceptionState& exceptionState)
            : m_interfaceName(interfaceName)
            , m_methodName(methodName)
            , m_exceptionState(exceptionState)
            , m_dirty(true)
        {
            resetPerPropertyContext();
        }

        const String& interfaceName() const { return m_interfaceName; }
        const String& methodName() const { return m_methodName; }
        bool forConstructor() const { return m_methodName.isEmpty(); }
        ExceptionState& exceptionState() const { return m_exceptionState; }

        bool isNullable() const { return m_isNullable; }
        String typeName() const { return m_propertyTypeName; }

        ConversionContext& setConversionType(const String&, bool);

        void throwTypeError(const String& detail);

        void resetPerPropertyContext();

    private:
        const String m_interfaceName;
        const String m_methodName;
        ExceptionState& m_exceptionState;
        bool m_dirty;

        bool m_isNullable;
        String m_propertyTypeName;
    };

    class ConversionContextScope {
    public:
        ConversionContextScope(ConversionContext& context)
            : m_context(context) { }
        ~ConversionContextScope()
        {
            m_context.resetPerPropertyContext();
        }
    private:
        ConversionContext& m_context;
    };

    bool convert(ConversionContext&, const String&, bool&) const;
    bool convert(ConversionContext&, const String&, double&) const;
    bool convert(ConversionContext&, const String&, String&) const;
    bool convert(ConversionContext&, const String&, ScriptValue&) const;

    template<typename IntegralType>
    bool convert(ConversionContext&, const String&, IntegralType&) const;
    template<typename IntegralType>
    bool convert(ConversionContext&, const String&, Nullable<IntegralType>&) const;

    bool convert(ConversionContext&, const String&, MessagePortArray&) const;
    bool convert(ConversionContext&, const String&, HashSet<AtomicString>&) const;
    bool convert(ConversionContext&, const String&, Dictionary&) const;
    bool convert(ConversionContext&, const String&, Vector<String>&) const;
    bool convert(ConversionContext&, const String&, ArrayValue&) const;
    template<template <typename> class PointerType, typename T>
    bool convert(ConversionContext&, const String&, PointerType<T>&) const;

    template<typename StringType>
    bool getStringType(const String&, StringType&) const;

    bool getOwnPropertiesAsStringHashMap(HashMap<String, String>&) const;
    bool getOwnPropertyNames(Vector<String>&) const;

    bool getWithUndefinedOrNullCheck(const String&, String&) const;

    bool hasProperty(const String&) const;

private:
    bool getKey(const String& key, v8::Local<v8::Value>&) const;

    v8::Handle<v8::Value> m_options;
    v8::Isolate* m_isolate;
};

template<>
struct NativeValueTraits<Dictionary> {
    static inline Dictionary nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
    {
        return Dictionary(value, isolate);
    }
};

template <typename T>
struct IntegralTypeTraits {
};

template <>
struct IntegralTypeTraits<uint8_t> {
    static inline uint8_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt8(value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt8"; }
};

template <>
struct IntegralTypeTraits<int8_t> {
    static inline int8_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt8(value, configuration, exceptionState);
    }
    static const String typeName() { return "Int8"; }
};

template <>
struct IntegralTypeTraits<unsigned short> {
    static inline uint16_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt16(value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt16"; }
};

template <>
struct IntegralTypeTraits<short> {
    static inline int16_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt16(value, configuration, exceptionState);
    }
    static const String typeName() { return "Int16"; }
};

template <>
struct IntegralTypeTraits<unsigned> {
    static inline uint32_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt32(value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt32"; }
};

template <>
struct IntegralTypeTraits<unsigned long> {
    static inline uint32_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt32(value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt32"; }
};

template <>
struct IntegralTypeTraits<int> {
    static inline int32_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt32(value, configuration, exceptionState);
    }
    static const String typeName() { return "Int32"; }
};

template <>
struct IntegralTypeTraits<long> {
    static inline int32_t toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt32(value, configuration, exceptionState);
    }
    static const String typeName() { return "Int32"; }
};

template <>
struct IntegralTypeTraits<unsigned long long> {
    static inline unsigned long long toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt64(value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt64"; }
};

template <>
struct IntegralTypeTraits<long long> {
    static inline long long toIntegral(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt64(value, configuration, exceptionState);
    }
    static const String typeName() { return "Int64"; }
};

template<typename T> bool Dictionary::convert(ConversionContext& context, const String& key, T& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    value = IntegralTypeTraits<T>::toIntegral(v8Value, NormalConversion, context.exceptionState());
    if (context.exceptionState().throwIfNeeded())
        return false;

    return true;
}

template<typename T> bool Dictionary::convert(ConversionContext& context, const String& key, Nullable<T>& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value)) {
        value = Nullable<T>();
        return true;
    }

    T converted = IntegralTypeTraits<T>::toIntegral(v8Value, NormalConversion, context.exceptionState());

    if (context.exceptionState().throwIfNeeded())
        return false;

    value = Nullable<T>(converted);
    return true;
}

template<template <typename> class PointerType, typename T> bool Dictionary::convert(ConversionContext& context, const String& key, PointerType<T>& value) const
{
    ConversionContextScope scope(context);

    if (!get(key, value))
        return true;

    if (value)
        return true;

    v8::Local<v8::Value> v8Value;
    getKey(key, v8Value);
    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value))
        return true;

    context.throwTypeError(ExceptionMessages::incorrectPropertyType(key, "does not have a " + context.typeName() + " type."));
    return false;
}

}

#endif // Dictionary_h
