/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "core/events/SecurityPolicyViolationEvent.h"

namespace blink {

namespace {

const char kEnforce[] = "enforce";
const char kReport[] = "report";

}  // namespace

SecurityPolicyViolationEvent::SecurityPolicyViolationEvent(
    const AtomicString& type,
    const SecurityPolicyViolationEventInit& initializer)
    : Event(type, true, false, ComposedMode::Composed),
      m_disposition(ContentSecurityPolicyHeaderTypeEnforce),
      m_lineNumber(0),
      m_columnNumber(0),
      m_statusCode(0) {
  if (initializer.hasDocumentURI())
    m_documentURI = initializer.documentURI();
  if (initializer.hasReferrer())
    m_referrer = initializer.referrer();
  if (initializer.hasBlockedURI())
    m_blockedURI = initializer.blockedURI();
  if (initializer.hasViolatedDirective())
    m_violatedDirective = initializer.violatedDirective();
  if (initializer.hasEffectiveDirective())
    m_effectiveDirective = initializer.effectiveDirective();
  if (initializer.hasOriginalPolicy())
    m_originalPolicy = initializer.originalPolicy();
  m_disposition = initializer.disposition() == kReport
                      ? ContentSecurityPolicyHeaderTypeReport
                      : ContentSecurityPolicyHeaderTypeEnforce;
  if (initializer.hasSourceFile())
    m_sourceFile = initializer.sourceFile();
  if (initializer.hasLineNumber())
    m_lineNumber = initializer.lineNumber();
  if (initializer.hasColumnNumber())
    m_columnNumber = initializer.columnNumber();
  if (initializer.hasStatusCode())
    m_statusCode = initializer.statusCode();
}

const String& SecurityPolicyViolationEvent::disposition() const {
  DEFINE_STATIC_LOCAL(const String, enforce, (kEnforce));
  DEFINE_STATIC_LOCAL(const String, report, (kReport));

  if (m_disposition == ContentSecurityPolicyHeaderTypeReport)
    return report;
  return enforce;
}

}  // namespace blink
