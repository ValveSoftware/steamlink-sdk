/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef ErrorEvent_h
#define ErrorEvent_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/events/ErrorEventInit.h"
#include "core/events/Event.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ErrorEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ErrorEvent* create()
    {
        return new ErrorEvent;
    }
    static ErrorEvent* create(const String& message, std::unique_ptr<SourceLocation> location, DOMWrapperWorld* world)
    {
        return new ErrorEvent(message, std::move(location), world);
    }
    static ErrorEvent* create(const AtomicString& type, const ErrorEventInit& initializer)
    {
        return new ErrorEvent(type, initializer);
    }
    static ErrorEvent* createSanitizedError(DOMWrapperWorld* world)
    {
        return new ErrorEvent("Script error.", SourceLocation::create(String(), 0, 0, nullptr), world);
    }
    ~ErrorEvent() override;

    // As 'message' is exposed to JavaScript, never return unsanitizedMessage.
    const String& message() const { return m_sanitizedMessage; }
    const String& filename() const { return m_location->url(); }
    unsigned lineno() const { return m_location->lineNumber(); }
    unsigned colno() const { return m_location->columnNumber(); }
    ScriptValue error(ScriptState*) const;

    // 'messageForConsole' is not exposed to JavaScript, and prefers 'm_unsanitizedMessage'.
    const String& messageForConsole() const { return !m_unsanitizedMessage.isEmpty() ? m_unsanitizedMessage : m_sanitizedMessage; }
    SourceLocation* location() const { return m_location.get(); }

    const AtomicString& interfaceName() const override;

    DOMWrapperWorld* world() const { return m_world.get(); }

    void setUnsanitizedMessage(const String&);

    DECLARE_VIRTUAL_TRACE();

private:
    ErrorEvent();
    ErrorEvent(const String& message, std::unique_ptr<SourceLocation>, DOMWrapperWorld*);
    ErrorEvent(const AtomicString&, const ErrorEventInit&);

    String m_unsanitizedMessage;
    String m_sanitizedMessage;
    std::unique_ptr<SourceLocation> m_location;
    ScriptValue m_error;

    RefPtr<DOMWrapperWorld> m_world;
};

} // namespace blink

#endif // ErrorEvent_h
