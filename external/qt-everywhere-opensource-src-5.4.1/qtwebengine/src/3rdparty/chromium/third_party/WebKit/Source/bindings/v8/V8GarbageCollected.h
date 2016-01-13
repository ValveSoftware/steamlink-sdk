/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8GarbageCollected_h
#define V8GarbageCollected_h

#include <v8.h>

namespace WebCore {

template<typename T>
class V8GarbageCollected {
    WTF_MAKE_NONCOPYABLE(V8GarbageCollected);
public:
    static T* Cast(v8::Handle<v8::Value> value)
    {
        ASSERT(value->IsExternal());
        T* result = static_cast<T*>(value.As<v8::External>()->Value());
        RELEASE_ASSERT(result->m_handle == value);
        return result;
    }

protected:
    V8GarbageCollected(v8::Isolate* isolate)
        : m_isolate(isolate)
        , m_handle(isolate, v8::External::New(isolate, static_cast<T*>(this)))
        , m_releasedToV8GarbageCollector(false)
    {
    }

    v8::Handle<v8::External> releaseToV8GarbageCollector()
    {
        ASSERT(!m_handle.isWeak()); // Call this exactly once.
        m_releasedToV8GarbageCollector = true;
        v8::Handle<v8::External> result = m_handle.newLocal(m_isolate);
        m_handle.setWeak(static_cast<T*>(this), &weakCallback);
        return result;
    }

    ~V8GarbageCollected()
    {
        ASSERT(!m_releasedToV8GarbageCollector || m_handle.isEmpty());
    }

    v8::Isolate* isolate() { return m_isolate; }

private:
    static void weakCallback(const v8::WeakCallbackData<v8::External, T>& data)
    {
        T* self = data.GetParameter();
        self->m_handle.clear();
        delete self;
    }

    v8::Isolate* m_isolate;
    ScopedPersistent<v8::External> m_handle;
    bool m_releasedToV8GarbageCollector;
};

} // namespace WebCore

#endif // V8GarbageCollected_h
