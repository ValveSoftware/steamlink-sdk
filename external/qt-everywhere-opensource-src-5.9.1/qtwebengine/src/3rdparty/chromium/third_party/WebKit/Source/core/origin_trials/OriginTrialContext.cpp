// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebOriginTrialTokenStatus.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "wtf/text/StringBuilder.h"

#include <v8.h>

namespace blink {

namespace {

static EnumerationHistogram& tokenValidationResultHistogram() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, histogram,
      new EnumerationHistogram(
          "OriginTrials.ValidationResult",
          static_cast<int>(WebOriginTrialTokenStatus::Last)));
  return histogram;
}

bool isWhitespace(UChar chr) {
  return (chr == ' ') || (chr == '\t');
}

bool skipWhiteSpace(const String& str, unsigned& pos) {
  unsigned len = str.length();
  while (pos < len && isWhitespace(str[pos]))
    ++pos;
  return pos < len;
}

// Extracts a quoted or unquoted token from an HTTP header. If the token was a
// quoted string, this also removes the quotes and unescapes any escaped
// characters. Also skips all whitespace before and after the token.
String extractTokenOrQuotedString(const String& headerValue, unsigned& pos) {
  unsigned len = headerValue.length();
  String result;
  if (!skipWhiteSpace(headerValue, pos))
    return String();

  if (headerValue[pos] == '\'' || headerValue[pos] == '"') {
    StringBuilder out;
    // Quoted string, append characters until matching quote is found,
    // unescaping as we go.
    UChar quote = headerValue[pos++];
    while (pos < len && headerValue[pos] != quote) {
      if (headerValue[pos] == '\\')
        pos++;
      if (pos < len)
        out.append(headerValue[pos++]);
    }
    if (pos < len)
      pos++;
    result = out.toString();
  } else {
    // Unquoted token. Consume all characters until whitespace or comma.
    int startPos = pos;
    while (pos < len && !isWhitespace(headerValue[pos]) &&
           headerValue[pos] != ',')
      pos++;
    result = headerValue.substring(startPos, pos - startPos);
  }
  skipWhiteSpace(headerValue, pos);
  return result;
}

}  // namespace

OriginTrialContext::OriginTrialContext(ExecutionContext* host,
                                       WebTrialTokenValidator* validator)
    : m_host(host), m_trialTokenValidator(validator) {}

// static
const char* OriginTrialContext::supplementName() {
  return "OriginTrialContext";
}

// static
OriginTrialContext* OriginTrialContext::from(ExecutionContext* host,
                                             CreateMode create) {
  OriginTrialContext* originTrials = static_cast<OriginTrialContext*>(
      Supplement<ExecutionContext>::from(host, supplementName()));
  if (!originTrials && create == CreateIfNotExists) {
    originTrials = new OriginTrialContext(
        host, Platform::current()->trialTokenValidator());
    Supplement<ExecutionContext>::provideTo(*host, supplementName(),
                                            originTrials);
  }
  return originTrials;
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::parseHeaderValue(
    const String& headerValue) {
  std::unique_ptr<Vector<String>> tokens(new Vector<String>);
  unsigned pos = 0;
  unsigned len = headerValue.length();
  while (pos < len) {
    String token = extractTokenOrQuotedString(headerValue, pos);
    if (!token.isEmpty())
      tokens->append(token);
    // Make sure tokens are comma-separated.
    if (pos < len && headerValue[pos++] != ',')
      return nullptr;
  }
  return tokens;
}

// static
void OriginTrialContext::addTokensFromHeader(ExecutionContext* host,
                                             const String& headerValue) {
  if (headerValue.isEmpty())
    return;
  std::unique_ptr<Vector<String>> tokens(parseHeaderValue(headerValue));
  if (!tokens)
    return;
  addTokens(host, tokens.get());
}

// static
void OriginTrialContext::addTokens(ExecutionContext* host,
                                   const Vector<String>* tokens) {
  if (!tokens || tokens->isEmpty())
    return;
  from(host)->addTokens(*tokens);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::getTokens(
    ExecutionContext* host) {
  OriginTrialContext* context = from(host, DontCreateIfNotExists);
  if (!context || context->m_tokens.isEmpty())
    return nullptr;
  return std::unique_ptr<Vector<String>>(new Vector<String>(context->m_tokens));
}

void OriginTrialContext::addToken(const String& token) {
  if (!token.isEmpty()) {
    m_tokens.append(token);
    validateToken(token);
  }
  initializePendingFeatures();
}

void OriginTrialContext::addTokens(const Vector<String>& tokens) {
  for (const String& token : tokens) {
    if (!token.isEmpty()) {
      m_tokens.append(token);
      validateToken(token);
    }
  }
  initializePendingFeatures();
}

void OriginTrialContext::initializePendingFeatures() {
  if (!m_host->isDocument())
    return;
  LocalFrame* frame = toDocument(m_host.get())->frame();
  if (!frame)
    return;
  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState)
    return;
  if (!scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(scriptState);
  installPendingConditionalFeaturesOnWindow(scriptState);
}

bool OriginTrialContext::isTrialEnabled(const String& trialName) {
  if (!RuntimeEnabledFeatures::originTrialsEnabled())
    return false;

  return m_enabledTrials.contains(trialName);
}

void OriginTrialContext::validateToken(const String& token) {
  DCHECK(!token.isEmpty());

  // Origin trials are only enabled for secure origins
  if (!m_host->isSecureContext()) {
    tokenValidationResultHistogram().count(
        static_cast<int>(WebOriginTrialTokenStatus::Insecure));
    return;
  }

  if (!m_trialTokenValidator) {
    tokenValidationResultHistogram().count(
        static_cast<int>(WebOriginTrialTokenStatus::NotSupported));
    return;
  }

  WebSecurityOrigin origin(m_host->getSecurityOrigin());
  WebString trialName;
  WebOriginTrialTokenStatus tokenResult =
      m_trialTokenValidator->validateToken(token, origin, &trialName);
  if (tokenResult == WebOriginTrialTokenStatus::Success)
    m_enabledTrials.add(trialName);

  tokenValidationResultHistogram().count(static_cast<int>(tokenResult));
}

DEFINE_TRACE(OriginTrialContext) {
  visitor->trace(m_host);
  Supplement<ExecutionContext>::trace(visitor);
}

}  // namespace blink
