// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

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

// The enum entries below are written to histograms and thus cannot be deleted
// or reordered.
// New entries must be added immediately before the end.
enum MessageGeneratedResult {
    MessageGeneratedResultNotRequested = 0,
    MessageGeneratedResultYes = 1,
    MessageGeneratedResultNo = 2,
    MessageGeneratedResultLast = MessageGeneratedResultNo
};

static EnumerationHistogram& enabledResultHistogram()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, histogram, new EnumerationHistogram("OriginTrials.FeatureEnabled", static_cast<int>(WebOriginTrialTokenStatus::Last)));
    return histogram;
}

static EnumerationHistogram& messageGeneratedResultHistogram()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, histogram, new EnumerationHistogram("OriginTrials.FeatureEnabled.MessageGenerated", MessageGeneratedResultLast));
    return histogram;
}

String getDisabledMessage(const String& featureName)
{
    return "The '" + featureName + "' feature is currently enabled in limited trials. Please see https://bit.ly/OriginTrials for information on enabling a trial for your site.";
}

String getInvalidTokenMessage(const String& featureName)
{
    return "The provided token(s) are not valid for the '" + featureName + "' feature.";
}

String getNotSupportedMessage()
{
    return "This browser does not support origin trials.";
}

int getTokenValidationResultPriority(
    WebOriginTrialTokenStatus validationResult)
{
    // There could be zero, one or multiple trial tokens provided for a given
    // context, and any number of them could be invalid. The tokens could be
    // invalid for a variety of reasons. For multiple invalid tokens, the
    // failures are collected into a single result for collecting metrics. At a
    // high level, the idea is to report on the tokens that were closest to
    // being valid for enabling a feature. This gives the priority as:
    //   1. Expired, but otherwise valid
    //   2. Wrong feature, but correct origin
    //   3. Wrong origin, but correct format for token data
    //   4. Invalid signature for token
    //   5. Wrong version for token (not currently supported version number(s))
    //   6. Invalid data in token (can be before/after validating signature)
    //   7. Embedder does not support origin trials
    //   8. No tokens provided
    // NOTE: Lower numbers are higher priority
    // See this document for details:
    // https://docs.google.com/document/d/1qVP2CK1lbfmtIJRIm6nwuEFFhGhYbtThLQPo3CSTtmg/edit#bookmark=id.k1j0q938so3b
    switch (validationResult) {
    case WebOriginTrialTokenStatus::Success:
    case WebOriginTrialTokenStatus::Insecure:
    case WebOriginTrialTokenStatus::FeatureDisabled:
        // This function should only be used for token validation failures
        NOTREACHED();
        return 99;
    case WebOriginTrialTokenStatus::Expired:
        return 1;
    case WebOriginTrialTokenStatus::WrongFeature:
        return 2;
    case WebOriginTrialTokenStatus::WrongOrigin:
        return 3;
    case WebOriginTrialTokenStatus::InvalidSignature:
        return 4;
    case WebOriginTrialTokenStatus::WrongVersion:
        return 5;
    case WebOriginTrialTokenStatus::Malformed:
        return 6;
    case WebOriginTrialTokenStatus::NotSupported:
        return 7;
    case WebOriginTrialTokenStatus::NoTokens:
        return 8;
    }

    NOTREACHED();
    return 99;
}

WebOriginTrialTokenStatus UpdateResultFromValidationFailure(
    WebOriginTrialTokenStatus newResult,
    WebOriginTrialTokenStatus currentResult)
{
    if (getTokenValidationResultPriority(newResult) < getTokenValidationResultPriority(currentResult)) {
        return newResult;
    }
    return currentResult;
}

bool isWhitespace(UChar chr)
{
    return (chr == ' ') || (chr == '\t');
}

bool skipWhiteSpace(const String& str, unsigned& pos)
{
    unsigned len = str.length();
    while (pos < len && isWhitespace(str[pos]))
        ++pos;
    return pos < len;
}

// Extracts a quoted or unquoted token from an HTTP header. If the token was a
// quoted string, this also removes the quotes and unescapes any escaped
// characters. Also skips all whitespace before and after the token.
String extractTokenOrQuotedString(const String& headerValue, unsigned& pos)
{
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
        while (pos < len && !isWhitespace(headerValue[pos]) && headerValue[pos] != ',')
            pos++;
        result = headerValue.substring(startPos, pos - startPos);
    }
    skipWhiteSpace(headerValue, pos);
    return result;
}

} // namespace

OriginTrialContext::OriginTrialContext(ExecutionContext* host, WebTrialTokenValidator* validator) : m_host(host), m_trialTokenValidator(validator)
{
}

// static
const char* OriginTrialContext::supplementName()
{
    return "OriginTrialContext";
}

// static
OriginTrialContext* OriginTrialContext::from(ExecutionContext* host, CreateMode create)
{
    OriginTrialContext* originTrials = static_cast<OriginTrialContext*>(Supplement<ExecutionContext>::from(host, supplementName()));
    if (!originTrials && create == CreateIfNotExists) {
        originTrials = new OriginTrialContext(host, Platform::current()->trialTokenValidator());
        Supplement<ExecutionContext>::provideTo(*host, supplementName(), originTrials);
    }
    return originTrials;
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::parseHeaderValue(const String& headerValue)
{
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
void OriginTrialContext::addTokensFromHeader(ExecutionContext* host, const String& headerValue)
{
    if (headerValue.isEmpty())
        return;
    std::unique_ptr<Vector<String>> tokens(parseHeaderValue(headerValue));
    if (!tokens)
        return;
    addTokens(host, parseHeaderValue(headerValue).get());
}

// static
void OriginTrialContext::addTokens(ExecutionContext* host, const Vector<String>* tokens)
{
    if (!tokens || tokens->isEmpty())
        return;
    from(host)->addTokens(*tokens);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::getTokens(ExecutionContext* host)
{
    OriginTrialContext* context = from(host, DontCreateIfNotExists);
    if (!context || context->m_tokens.isEmpty())
        return nullptr;
    return std::unique_ptr<Vector<String>>(new Vector<String>(context->m_tokens));
}


void OriginTrialContext::addToken(const String& token)
{
    if (!token.isEmpty())
        m_tokens.append(token);
    initializePendingFeatures();
}

void OriginTrialContext::addTokens(const Vector<String>& tokens)
{
    m_tokens.appendVector(tokens);
    initializePendingFeatures();
}

void OriginTrialContext::initializePendingFeatures()
{
    // TODO(iclelland): Factor this logic out to methods on the various
    // execution contexts
    if (m_host->isDocument()) {
        LocalFrame* frame = toDocument(m_host.get())->frame();
        if (!frame)
            return;
        ScriptState* state = ScriptState::forMainWorld(frame);
        if (!state)
            return;
        if (!frame->script().windowProxy(state->world())->isContextInitialized())
            return;
        v8::HandleScope handleScope(state->isolate());
        installOriginTrials(state);
    } else if (m_host->isWorkerGlobalScope()) {
        WorkerOrWorkletScriptController* scriptController = toWorkerGlobalScope(m_host.get())->scriptController();
        if (!scriptController)
            return;
        ScriptState* state = scriptController->getScriptState();
        if (!state)
            return;
        if (!scriptController->isContextInitialized())
            return;
        v8::HandleScope handleScope(state->isolate());
        installOriginTrials(state);
    }
}

void OriginTrialContext::setFeatureBindingsInstalled(const String& featureName)
{
    m_bindingsInstalled.add(featureName);
}

bool OriginTrialContext::featureBindingsInstalled(const String& featureName)
{
    return m_bindingsInstalled.contains(featureName);
}

bool OriginTrialContext::isFeatureEnabled(const String& featureName, String* errorMessage)
{
    if (!RuntimeEnabledFeatures::originTrialsEnabled()) {
        // Do not set an error message. When the framework is disabled, it
        // should behave the same as when only runtime flags are used.
        return false;
    }

    WebOriginTrialTokenStatus result = checkFeatureEnabled(featureName, errorMessage);

    // Record metrics for the enabled result, but only once per context.
    if (!m_enabledResultCountedForFeature.contains(featureName)) {
        enabledResultHistogram().count(static_cast<int>(result));
        m_enabledResultCountedForFeature.add(featureName);
    }

    if (result == WebOriginTrialTokenStatus::Success) {
        return true;
    }

    // If an error message has already been generated in this context, for this
    // feature, do not generate another one. This avoids cluttering the console
    // with error messages on every attempt to access the feature.
    // Metrics are also recorded, to track whether too many messages are being
    // generated over time.
    MessageGeneratedResult generateMessage = MessageGeneratedResultYes;
    if (!errorMessage) {
        generateMessage = MessageGeneratedResultNotRequested;
    } else if (m_errorMessageGeneratedForFeature.contains(featureName)) {
        *errorMessage = "";
        generateMessage = MessageGeneratedResultNo;
    }
    messageGeneratedResultHistogram().count(generateMessage);

    if (generateMessage != MessageGeneratedResultYes) {
        return false;
    }

    // Generate an error message, only if one was not already provided by the
    // enabled check.
    if (!errorMessage->length()) {
        switch (result) {
        case WebOriginTrialTokenStatus::NotSupported:
            *errorMessage = getNotSupportedMessage();
            break;
        case WebOriginTrialTokenStatus::NoTokens:
            *errorMessage = getDisabledMessage(featureName);
            break;
        default:
            *errorMessage = getInvalidTokenMessage(featureName);
            break;
        }
    }
    m_errorMessageGeneratedForFeature.add(featureName);
    return false;
}

WebOriginTrialTokenStatus OriginTrialContext::checkFeatureEnabled(const String& featureName, String* errorMessage)
{
    // Feature trials are only enabled for secure origins
    bool isSecure = errorMessage
        ? m_host->isSecureContext(*errorMessage)
        : m_host->isSecureContext();
    if (!isSecure) {
        // The execution context should always set a message here, if a valid
        // pointer was passed in. If it does not, then we should find out why
        // not, and decide whether the OriginTrialContext should be using its
        // own error messages for this case.
        DCHECK(!errorMessage || !errorMessage->isEmpty());
        return WebOriginTrialTokenStatus::Insecure;
    }

    if (!m_trialTokenValidator) {
        return WebOriginTrialTokenStatus::NotSupported;
    }

    WebOriginTrialTokenStatus failedValidationResult = WebOriginTrialTokenStatus::NoTokens;
    WebSecurityOrigin origin(m_host->getSecurityOrigin());
    for (const String& token : m_tokens) {
        // Check with the validator service to verify the signature and that
        // the token is valid for the combination of origin and feature.
        WebOriginTrialTokenStatus tokenResult = m_trialTokenValidator->validateToken(token, origin, featureName);
        // If the feature is disabled by policy, or if the token is valid, we
        // can return immediately now.
        if (tokenResult == WebOriginTrialTokenStatus::FeatureDisabled || tokenResult == WebOriginTrialTokenStatus::Success) {
            return tokenResult;
        }
        failedValidationResult = UpdateResultFromValidationFailure(tokenResult, failedValidationResult);
    }

    return failedValidationResult;
}

DEFINE_TRACE(OriginTrialContext)
{
    visitor->trace(m_host);
}

} // namespace blink
