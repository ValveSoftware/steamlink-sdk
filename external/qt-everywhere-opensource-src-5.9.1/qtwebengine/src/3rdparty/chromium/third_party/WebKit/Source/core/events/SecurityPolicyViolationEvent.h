/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef SecurityPolicyViolationEvent_h
#define SecurityPolicyViolationEvent_h

#include "core/events/Event.h"
#include "core/events/SecurityPolicyViolationEventInit.h"
#include "platform/network/ContentSecurityPolicyParsers.h"

namespace blink {

class SecurityPolicyViolationEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SecurityPolicyViolationEvent* create(
      const AtomicString& type,
      const SecurityPolicyViolationEventInit& initializer) {
    return new SecurityPolicyViolationEvent(type, initializer);
  }

  const String& documentURI() const { return m_documentURI; }
  const String& referrer() const { return m_referrer; }
  const String& blockedURI() const { return m_blockedURI; }
  const String& violatedDirective() const { return m_violatedDirective; }
  const String& effectiveDirective() const { return m_effectiveDirective; }
  const String& originalPolicy() const { return m_originalPolicy; }
  const String& disposition() const;
  const String& sourceFile() const { return m_sourceFile; }
  int lineNumber() const { return m_lineNumber; }
  int columnNumber() const { return m_columnNumber; }
  uint16_t statusCode() const { return m_statusCode; }

  const AtomicString& interfaceName() const override {
    return EventNames::SecurityPolicyViolationEvent;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::trace(visitor); }

 private:
  SecurityPolicyViolationEvent(
      const AtomicString& type,
      const SecurityPolicyViolationEventInit& initializer);

  String m_documentURI;
  String m_referrer;
  String m_blockedURI;
  String m_violatedDirective;
  String m_effectiveDirective;
  String m_originalPolicy;
  ContentSecurityPolicyHeaderType m_disposition;
  String m_sourceFile;
  int m_lineNumber;
  int m_columnNumber;
  int m_statusCode;
};

}  // namespace blink

#endif  // SecurityPolicyViolationEvent_h
