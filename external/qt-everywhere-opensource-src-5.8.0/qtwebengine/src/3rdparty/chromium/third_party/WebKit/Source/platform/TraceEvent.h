/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

// This header file defines implementation details of how the trace macros in
// TraceEventCommon.h collect and store trace events. Anything not
// implementation-specific should go in TraceEventCommon.h instead of here.

#ifndef TraceEvent_h
#define TraceEvent_h

#include "platform/EventTracer.h"
#include "platform/TraceEventCommon.h"

#include "platform/TracedValue.h"
#include "wtf/Allocator.h"
#include "wtf/DynamicAnnotations.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/CString.h"
#include <memory>

////////////////////////////////////////////////////////////////////////////////
// Implementation specific tracing API definitions.

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) \
    blink::TraceEvent::TraceStringWithCopy(str)

// By default, uint64 ID argument values are not mangled with the Process ID in
// TRACE_EVENT_ASYNC macros. Use this macro to force Process ID mangling.
#define TRACE_ID_MANGLE(id) \
    blink::TraceEvent::TraceID::ForceMangle(id)

// By default, pointers are mangled with the Process ID in TRACE_EVENT_ASYNC
// macros. Use this macro to prevent Process ID mangling.
#define TRACE_ID_DONT_MANGLE(id) \
    blink::TraceEvent::TraceID::DontMangle(id)

// By default, trace IDs are eventually converted to a single 64-bit number. Use
// this macro to add a scope string.
#define TRACE_ID_WITH_SCOPE(scope, id) \
    blink::TraceEvent::TraceID::WithScope(scope, id)

// Creates a scope of a sampling state with the given category and name (both must
// be constant strings). These states are intended for a sampling profiler.
// Implementation note: we store category and name together because we don't
// want the inconsistency/expense of storing two pointers.
// |thread_bucket| is [0..2] and is used to statically isolate samples in one
// thread from others.
//
// {  // The sampling state is set within this scope.
//    TRACE_EVENT_SAMPLING_STATE_SCOPE_FOR_BUCKET(0, "category", "name");
//    ...;
// }
#define TRACE_EVENT_SCOPED_SAMPLING_STATE_FOR_BUCKET(bucket_number, category, name) \
    TraceEvent::SamplingStateScope<bucket_number> traceEventSamplingScope(category "\0" name);

// Returns a current sampling state of the given bucket.
// The format of the returned string is "category\0name".
#define TRACE_EVENT_GET_SAMPLING_STATE_FOR_BUCKET(bucket_number) \
    TraceEvent::SamplingStateScope<bucket_number>::current()

// Sets a current sampling state of the given bucket.
// |category| and |name| have to be constant strings.
#define TRACE_EVENT_SET_SAMPLING_STATE_FOR_BUCKET(bucket_number, category, name) \
    TraceEvent::SamplingStateScope<bucket_number>::set(category "\0" name)

// Sets a current sampling state of the given bucket.
// |categoryAndName| doesn't need to be a constant string.
// The format of the string is "category\0name".
#define TRACE_EVENT_SET_NONCONST_SAMPLING_STATE_FOR_BUCKET(bucket_number, categoryAndName) \
    TraceEvent::SamplingStateScope<bucket_number>::set(categoryAndName)

// Get a pointer to the enabled state of the given trace category. Only
// long-lived literal strings should be given as the category name. The returned
// pointer can be held permanently in a local static for example. If the
// unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// TRACE_EVENT_API_ADD_TRACE_EVENT can be called. It's OK if tracing is disabled
// between the load of the tracing state and the call to
// TRACE_EVENT_API_ADD_TRACE_EVENT, because this flag only provides an early out
// for best performance when tracing is disabled.
// const unsigned char*
//     TRACE_EVENT_API_GET_CATEGORY_ENABLED(const char* category_name)
#define TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED \
    blink::EventTracer::getTraceCategoryEnabledFlag

// Add a trace event to the platform tracing system.
// blink::TraceEvent::TraceEventHandle TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    const char* name,
//                    const char* scope,
//                    unsigned long long id,
//                    double timestamp,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    std::unique_ptr<TracedValue> tracedValues[],
//                    unsigned char flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT \
    blink::EventTracer::addTraceEvent

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
//     blink::TraceEvent::TraceEventHandle handle)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION \
    blink::EventTracer::updateTraceEventDuration

////////////////////////////////////////////////////////////////////////////////

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define INTERNAL_TRACE_EVENT_UID3(a, b) \
    trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a, b) \
    INTERNAL_TRACE_EVENT_UID3(a, b)
#define INTERNALTRACEEVENTUID(name_prefix) \
    INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

// Implementation detail: internal macro to create static category.
// - WTF_ANNOTATE_BENIGN_RACE, see Thread Safety above.

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category) \
    static const unsigned char* INTERNALTRACEEVENTUID(categoryGroupEnabled) = 0; \
    WTF_ANNOTATE_BENIGN_RACE(&INTERNALTRACEEVENTUID(categoryGroupEnabled), \
                             "trace_event category"); \
    if (!INTERNALTRACEEVENTUID(categoryGroupEnabled)) { \
        INTERNALTRACEEVENTUID(categoryGroupEnabled) = \
            TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category); \
    }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category, name, flags, ...) \
    do { \
        INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
        if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
            blink::TraceEvent::addTraceEvent( \
                phase, INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                blink::TraceEvent::kGlobalScope, blink::TraceEvent::noEventId, \
                flags, ##__VA_ARGS__); \
        } \
    } while (0)

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    blink::TraceEvent::ScopedTracer INTERNALTRACEEVENTUID(scopedTracer); \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
        blink::TraceEvent::TraceEventHandle h = \
            blink::TraceEvent::addTraceEvent( \
                TRACE_EVENT_PHASE_COMPLETE, \
                INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                blink::TraceEvent::kGlobalScope, blink::TraceEvent::noEventId, \
                TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__); \
        INTERNALTRACEEVENTUID(scopedTracer).initialize( \
            INTERNALTRACEEVENTUID(categoryGroupEnabled), name, h); \
    }

#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category, name, bindId, flowFlags, ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    blink::TraceEvent::ScopedTracer INTERNALTRACEEVENTUID(scopedTracer); \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
        unsigned traceEventFlags = flowFlags; \
        blink::TraceEvent::TraceID traceEventBindId(bindId, &traceEventFlags); \
        blink::TraceEvent::TraceEventHandle h = \
            blink::TraceEvent::addTraceEvent( \
                TRACE_EVENT_PHASE_COMPLETE, \
                INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                blink::TraceEvent::kGlobalScope, blink::TraceEvent::noEventId, \
                traceEventBindId.data(), \
                EventTracer::systemTraceTime(), traceEventFlags, ##__VA_ARGS__); \
        INTERNALTRACEEVENTUID(scopedTracer).initialize( \
            INTERNALTRACEEVENTUID(categoryGroupEnabled), name, h); \
    }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category, name, id, flags, ...) \
    do { \
        INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
        if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
            unsigned traceEventFlags = flags | TRACE_EVENT_FLAG_HAS_ID; \
            blink::TraceEvent::TraceID traceEventTraceID( \
                id, &traceEventFlags); \
            blink::TraceEvent::addTraceEvent( \
                phase, INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                traceEventTraceID.scope(), traceEventTraceID.data(), \
                traceEventFlags, ##__VA_ARGS__); \
        } \
    } while (0)


// Implementation detail: internal macro to create static category and add event
// if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_AND_TIMESTAMP(phase, category, name, id, timestamp, flags, ...) \
    do { \
        INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
        if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
            unsigned traceEventFlags = flags | TRACE_EVENT_FLAG_HAS_ID; \
            blink::TraceEvent::TraceID traceEventTraceID( \
                id, &traceEventFlags); \
            blink::TraceEvent::addTraceEvent( \
                phase, INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                traceEventTraceID.scope(), traceEventTraceID.data(), \
                blink::TraceEvent::noBindId, \
                timestamp, traceEventFlags, ##__VA_ARGS__); \
        } \
    } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(phase, category, name, timestamp, flags, ...) \
    do { \
        INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
        if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
            blink::TraceEvent::addTraceEvent( \
                phase, INTERNALTRACEEVENTUID(categoryGroupEnabled), name, \
                blink::TraceEvent::kGlobalScope, blink::TraceEvent::noEventId, \
                blink::TraceEvent::noBindId, \
                timestamp, flags, ##__VA_ARGS__); \
        } \
    } while (0)

// Implementation detail: internal macro to enter and leave a context based on
// the current scope.
#define INTERNAL_TRACE_EVENT_SCOPED_CONTEXT(categoryGroup, name, context) \
    struct INTERNAL_TRACE_EVENT_UID(ScopedContext) { \
    public: \
        INTERNAL_TRACE_EVENT_UID(ScopedContext)(uint64_t cid) : m_cid(cid) { \
            TRACE_EVENT_ENTER_CONTEXT(category_group, name, m_cid); \
        } \
        ~INTERNAL_TRACE_EVENT_UID(ScopedContext)() { \
            TRACE_EVENT_LEAVE_CONTEXT(category_group, name, m_cid); \
        } \
    private: \
        uint64_t m_cid; \
        INTERNAL_TRACE_EVENT_UID(ScopedContext) \
            (const INTERNAL_TRACE_EVENT_UID(ScopedContext)&) {}; \
        void operator=(const INTERNAL_TRACE_EVENT_UID(ScopedContext)&) {}; \
    }; \
    INTERNAL_TRACE_EVENT_UID(ScopedContext) \
    INTERNAL_TRACE_EVENT_UID(scoped_context)(context.data());

// These values must be in sync with base::debug::TraceLog::CategoryGroupEnabledFlags.
#define INTERNAL_ENABLED_FOR_RECORDING (1 << 0)
#define INTERNAL_ENABLED_FOR_EVENT_CALLBACK (1 << 2)

#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE() \
    (*INTERNALTRACEEVENTUID(categoryGroupEnabled) & (INTERNAL_ENABLED_FOR_RECORDING | INTERNAL_ENABLED_FOR_EVENT_CALLBACK))

#define INTERNAL_TRACE_MEMORY(category, name)

namespace blink {

namespace TraceEvent {

// Specify these values when the corresponding argument of addTraceEvent is not
// used.
const int zeroNumArgs = 0;
const std::nullptr_t kGlobalScope = nullptr;
const unsigned long long noEventId = 0;
const unsigned long long noBindId = 0;

// TraceID encapsulates an ID that can either be an integer or pointer;
// optionally, it can be paired with a scope string, too. Pointers are mangled
// with the Process ID so that they are unlikely to collide when the same
// pointer is used on different processes.
class TraceID final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(TraceID);
public:
    class WithScope final {
        STACK_ALLOCATED();
    public:
        template<typename T> WithScope(const char* scope, T id)
            : m_scope(scope), m_data(reinterpret_cast<unsigned long long>(id)) { }
        const char* scope() const { return m_scope; }
        unsigned long long data() const { return m_data; }
    private:
        const char* m_scope = kGlobalScope;
        unsigned long long m_data;
    };

    template<bool dummyMangle> class MangleBehavior final {
        STACK_ALLOCATED();
    public:
        template<typename T> explicit MangleBehavior(T id) : m_data(reinterpret_cast<unsigned long long>(id)) { }
        explicit MangleBehavior(WithScope scopedID) : m_scope(scopedID.scope()), m_data(scopedID.data()) { }
        const char* scope() const { return m_scope; }
        unsigned long long data() const { return m_data; }
    private:
        const char* m_scope = kGlobalScope;
        unsigned long long m_data;
    };
    typedef MangleBehavior<false> DontMangle;
    typedef MangleBehavior<true> ForceMangle;

    TraceID(const void* id, unsigned* flags)
        : m_data(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(id)))
    {
        *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
    }
    TraceID(ForceMangle id, unsigned* flags)
        : m_scope(id.scope()), m_data(id.data())
    {
        *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
    }
    TraceID(DontMangle id, unsigned*) : m_scope(id.scope()), m_data(id.data()) { }
    TraceID(unsigned long long id, unsigned*) : m_data(id) { }
    TraceID(unsigned long id, unsigned*) : m_data(id) { }
    TraceID(unsigned id, unsigned*) : m_data(id) { }
    TraceID(unsigned short id, unsigned*) : m_data(id) { }
    TraceID(unsigned char id, unsigned*) : m_data(id) { }
    TraceID(long long id, unsigned*) :
        m_data(static_cast<unsigned long long>(id)) { }
    TraceID(long id, unsigned*) :
        m_data(static_cast<unsigned long long>(id)) { }
    TraceID(int id, unsigned*) :
        m_data(static_cast<unsigned long long>(id)) { }
    TraceID(short id, unsigned*) :
        m_data(static_cast<unsigned long long>(id)) { }
    TraceID(signed char id, unsigned*) :
        m_data(static_cast<unsigned long long>(id)) { }
    TraceID(WithScope scopedID, unsigned*) :
        m_scope(scopedID.scope()), m_data(scopedID.data()) { }

    const char* scope() const { return m_scope; }
    unsigned long long data() const { return m_data; }

private:
    const char* m_scope = kGlobalScope;
    unsigned long long m_data;
};

// Simple union to store various types as unsigned long long.
union TraceValueUnion {
    bool m_bool;
    unsigned long long m_uint;
    long long m_int;
    double m_double;
    const void* m_pointer;
    const char* m_string;
};

// Simple container for const char* that should be copied instead of retained.
class TraceStringWithCopy {
    STACK_ALLOCATED();
public:
    explicit TraceStringWithCopy(const char* str) : m_str(str) { }
    const char* str() const { return m_str; }
private:
    const char* m_str;
};

// Define setTraceValue for each allowed type. It stores the type and
// value in the return arguments. This allows this API to avoid declaring any
// structures so that it is portable to third_party libraries.
#define INTERNAL_DECLARE_SET_TRACE_VALUE(actualType, argExpression, unionMember, valueTypeId) \
    static inline void setTraceValue(actualType arg, unsigned char* type, unsigned long long* value) { \
        TraceValueUnion typeValue; \
        typeValue.unionMember = argExpression; \
        *type = valueTypeId; \
        *value = typeValue.m_uint; \
    }
// Simpler form for int types that can be safely casted.
#define INTERNAL_DECLARE_SET_TRACE_VALUE_INT(actualType, valueTypeId) \
    static inline void setTraceValue(actualType arg, \
                                     unsigned char* type, \
                                     unsigned long long* value) { \
        *type = valueTypeId; \
        *value = static_cast<unsigned long long>(arg); \
    }

INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned short, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned char, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(short, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(signed char, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE(bool, arg, m_bool, TRACE_VALUE_TYPE_BOOL)
INTERNAL_DECLARE_SET_TRACE_VALUE(double, arg, m_double, TRACE_VALUE_TYPE_DOUBLE)
INTERNAL_DECLARE_SET_TRACE_VALUE(const void*, arg, m_pointer, TRACE_VALUE_TYPE_POINTER)
INTERNAL_DECLARE_SET_TRACE_VALUE(const char*, arg, m_string, TRACE_VALUE_TYPE_STRING)
INTERNAL_DECLARE_SET_TRACE_VALUE(const TraceStringWithCopy&, arg.str(), m_string, TRACE_VALUE_TYPE_COPY_STRING)

#undef INTERNAL_DECLARE_SET_TRACE_VALUE
#undef INTERNAL_DECLARE_SET_TRACE_VALUE_INT

// WTF::String version of setTraceValue so that trace arguments can be strings.
static inline void setTraceValue(const WTF::CString& arg, unsigned char* type, unsigned long long* value)
{
    TraceValueUnion typeValue;
    typeValue.m_string = arg.data();
    *type = TRACE_VALUE_TYPE_COPY_STRING;
    *value = typeValue.m_uint;
}

static inline void setTraceValue(TracedValue*, unsigned char* type, unsigned long long*)
{
    *type = TRACE_VALUE_TYPE_CONVERTABLE;
}

template<typename T> static inline void setTraceValue(const std::unique_ptr<T>& ptr, unsigned char* type, unsigned long long* value)
{
    setTraceValue(ptr.get(), type, value);
}

template<typename T> struct TracedValueTraits {
    static const bool isTracedValue = false;
    static std::unique_ptr<TracedValue> moveFromIfTracedValue(const T&)
    {
        return nullptr;
    }
};

template<typename T> struct TracedValueTraits<std::unique_ptr<T>> {
    static const bool isTracedValue = std::is_convertible<T*, TracedValue*>::value;
    static std::unique_ptr<TracedValue> moveFromIfTracedValue(std::unique_ptr<T>&& tracedValue)
    {
        return std::move(tracedValue);
    }
};

template<typename T> bool isTracedValue(const T&)
{
    return TracedValueTraits<T>::isTracedValue;
}

template<typename T> std::unique_ptr<TracedValue> moveFromIfTracedValue(T&& value)
{
    return TracedValueTraits<T>::moveFromIfTracedValue(std::forward<T>(value));
}

// These addTraceEvent template functions are defined here instead of in the
// macro, because the arg values could be temporary string objects. In order to
// store pointers to the internal c_str and pass through to the tracing API, the
// arg values must live throughout these procedures.

static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bindId,
    double timestamp,
    unsigned flags)
{
    return TRACE_EVENT_API_ADD_TRACE_EVENT(
        phase, categoryEnabled, name, scope, id, bindId, timestamp,
        zeroNumArgs, 0, 0, 0,
        flags);
}

template <typename ARG1_TYPE>
static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bindId,
    double timestamp,
    unsigned flags,
    const char* arg1Name,
    ARG1_TYPE&& arg1Val)
{
    const int numArgs = 1;
    unsigned char argTypes[1];
    unsigned long long argValues[1];
    setTraceValue(arg1Val, &argTypes[0], &argValues[0]);
    if (isTracedValue(arg1Val)) {
        return TRACE_EVENT_API_ADD_TRACE_EVENT(
            phase, categoryEnabled, name, scope, id, bindId, timestamp,
            numArgs, &arg1Name, argTypes, argValues,
            moveFromIfTracedValue(std::forward<ARG1_TYPE>(arg1Val)),
            nullptr,
            flags);
    }
    return TRACE_EVENT_API_ADD_TRACE_EVENT(
        phase, categoryEnabled, name, scope, id, bindId, timestamp,
        numArgs, &arg1Name, argTypes, argValues,
        flags);
}

template<typename ARG1_TYPE, typename ARG2_TYPE>
static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bindId,
    double timestamp,
    unsigned flags,
    const char* arg1Name,
    ARG1_TYPE&& arg1Val,
    const char* arg2Name,
    ARG2_TYPE&& arg2Val)
{
    const int numArgs = 2;
    const char* argNames[2] = { arg1Name, arg2Name };
    unsigned char argTypes[2];
    unsigned long long argValues[2];
    setTraceValue(arg1Val, &argTypes[0], &argValues[0]);
    setTraceValue(arg2Val, &argTypes[1], &argValues[1]);
    if (isTracedValue(arg1Val) || isTracedValue(arg2Val)) {
        return TRACE_EVENT_API_ADD_TRACE_EVENT(
            phase, categoryEnabled, name, scope, id, bindId, timestamp,
            numArgs, argNames, argTypes, argValues,
            moveFromIfTracedValue(std::forward<ARG1_TYPE>(arg1Val)),
            moveFromIfTracedValue(std::forward<ARG2_TYPE>(arg2Val)),
            flags);
    }
    return TRACE_EVENT_API_ADD_TRACE_EVENT(
        phase, categoryEnabled, name, scope, id, bindId, timestamp,
        numArgs, argNames, argTypes, argValues,
        flags);
}

static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned flags)
{
    return addTraceEvent(phase, categoryEnabled, name, scope, id,
        blink::TraceEvent::noBindId, EventTracer::systemTraceTime(), flags);
}

template<typename ARG1_TYPE>
static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned flags,
    const char* arg1Name,
    ARG1_TYPE&& arg1Val)
{
    return addTraceEvent(phase, categoryEnabled, name, scope, id,
        blink::TraceEvent::noBindId, EventTracer::systemTraceTime(), flags,
        arg1Name, std::forward<ARG1_TYPE>(arg1Val));
}


template<typename ARG1_TYPE, typename ARG2_TYPE>
static inline TraceEventHandle addTraceEvent(
    char phase,
    const unsigned char* categoryEnabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned flags,
    const char* arg1Name,
    ARG1_TYPE&& arg1Val,
    const char* arg2Name,
    ARG2_TYPE&& arg2Val)
{
    return addTraceEvent(phase, categoryEnabled, name, scope, id,
        blink::TraceEvent::noBindId, EventTracer::systemTraceTime(), flags,
        arg1Name, std::forward<ARG1_TYPE>(arg1Val),
        arg2Name, std::forward<ARG2_TYPE>(arg2Val));
}

// Used by TRACE_EVENTx macro. Do not use directly.
class ScopedTracer final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScopedTracer);
public:
    // Note: members of m_data intentionally left uninitialized. See initialize.
    ScopedTracer() : m_pdata(0) { }
    ~ScopedTracer()
    {
        if (m_pdata && *m_pdata->categoryGroupEnabled)
            TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(m_data.categoryGroupEnabled, m_data.name, m_data.eventHandle);
    }

    void initialize(const unsigned char* categoryGroupEnabled, const char* name, TraceEventHandle eventHandle)
    {
        m_data.categoryGroupEnabled = categoryGroupEnabled;
        m_data.name = name;
        m_data.eventHandle = eventHandle;
        m_pdata = &m_data;
    }

private:
    // This Data struct workaround is to avoid initializing all the members
    // in Data during construction of this object, since this object is always
    // constructed, even when tracing is disabled. If the members of Data were
    // members of this class instead, compiler warnings occur about potential
    // uninitialized accesses.
    struct Data {
        DISALLOW_NEW();
        const unsigned char* categoryGroupEnabled;
        const char* name;
        TraceEventHandle eventHandle;
    };
    Data* m_pdata;
    Data m_data;
};

// TraceEventSamplingStateScope records the current sampling state
// and sets a new sampling state. When the scope exists, it restores
// the sampling state having recorded.
template<size_t BucketNumber>
class SamplingStateScope final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(SamplingStateScope);
public:
    SamplingStateScope(const char* categoryAndName)
    {
        m_previousState = SamplingStateScope<BucketNumber>::current();
        SamplingStateScope<BucketNumber>::set(categoryAndName);
    }

    ~SamplingStateScope()
    {
        SamplingStateScope<BucketNumber>::set(m_previousState);
    }

    // FIXME: Make load/store to traceSamplingState[] thread-safe and atomic.
    static const char* current()
    {
        return reinterpret_cast<const char*>(*blink::traceSamplingState[BucketNumber]);
    }
    static void set(const char* categoryAndName)
    {
        *blink::traceSamplingState[BucketNumber] = reinterpret_cast<TraceEventAPIAtomicWord>(categoryAndName);
    }

private:
    const char* m_previousState;
};

template<typename IDType> class TraceScopedTrackableObject {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(TraceScopedTrackableObject);
public:
    TraceScopedTrackableObject(const char* categoryGroup, const char* name, IDType id)
        : m_categoryGroup(categoryGroup), m_name(name), m_id(id)
    {
        TRACE_EVENT_OBJECT_CREATED_WITH_ID(m_categoryGroup, m_name, m_id);
    }

    ~TraceScopedTrackableObject()
    {
        TRACE_EVENT_OBJECT_DELETED_WITH_ID(m_categoryGroup, m_name, m_id);
    }

private:
    const char* m_categoryGroup;
    const char* m_name;
    IDType m_id;
};

} // namespace TraceEvent

} // namespace blink

#endif
