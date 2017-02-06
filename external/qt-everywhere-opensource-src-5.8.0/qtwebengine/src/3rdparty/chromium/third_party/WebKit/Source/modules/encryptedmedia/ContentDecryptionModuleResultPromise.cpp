// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/ContentDecryptionModuleResultPromise.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "public/platform/WebString.h"
#include "wtf/Assertions.h"

namespace blink {

ExceptionCode WebCdmExceptionToExceptionCode(WebContentDecryptionModuleException cdmException)
{
    switch (cdmException) {
    case WebContentDecryptionModuleExceptionNotSupportedError:
        return NotSupportedError;
    case WebContentDecryptionModuleExceptionInvalidStateError:
        return InvalidStateError;
    case WebContentDecryptionModuleExceptionInvalidAccessError:
        return InvalidAccessError;
    case WebContentDecryptionModuleExceptionQuotaExceededError:
        return QuotaExceededError;
    case WebContentDecryptionModuleExceptionUnknownError:
        return UnknownError;
    case WebContentDecryptionModuleExceptionClientError:
    case WebContentDecryptionModuleExceptionOutputError:
        // Currently no matching DOMException for these 2 errors.
        // FIXME: Update DOMException to handle these if actually added to
        // the EME spec.
        return UnknownError;
    }

    NOTREACHED();
    return UnknownError;
}

ContentDecryptionModuleResultPromise::ContentDecryptionModuleResultPromise(ScriptState* scriptState)
    : m_resolver(ScriptPromiseResolver::create(scriptState))
{
}

ContentDecryptionModuleResultPromise::~ContentDecryptionModuleResultPromise()
{
}

void ContentDecryptionModuleResultPromise::complete()
{
    NOTREACHED();
    reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithContentDecryptionModule(WebContentDecryptionModule* cdm)
{
    NOTREACHED();
    reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithSession(WebContentDecryptionModuleResult::SessionStatus status)
{
    NOTREACHED();
    reject(InvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::completeWithError(WebContentDecryptionModuleException exceptionCode, unsigned long systemCode, const WebString& errorMessage)
{
    // Non-zero |systemCode| is appended to the |errorMessage|. If the
    // |errorMessage| is empty, we'll report "Rejected with system code
    // (systemCode)".
    String errorString = errorMessage;
    if (systemCode != 0) {
        if (errorString.isEmpty())
            errorString.append("Rejected with system code");
        errorString.append(" (" + String::number(systemCode) + ")");
    }
    reject(WebCdmExceptionToExceptionCode(exceptionCode), errorString);
}

ScriptPromise ContentDecryptionModuleResultPromise::promise()
{
    return m_resolver->promise();
}

void ContentDecryptionModuleResultPromise::reject(ExceptionCode code, const String& errorMessage)
{
    // Reject the promise asynchronously. This avoids problems when gc is
    // destroying objects that result in unfulfilled promises being rejected.
    // (Resolving promises is still done synchronously as there may be events
    // already posted that need to happen only after the promise is resolved.)
    // TODO(jrummell): Make resolving a promise asynchronous as well (including
    // making sure events still happen after the promise is resolved).
    getExecutionContext()->postTask(BLINK_FROM_HERE, createSameThreadTask(&ContentDecryptionModuleResultPromise::rejectInternal, wrapPersistent(this), code, errorMessage));
}

void ContentDecryptionModuleResultPromise::rejectInternal(ExceptionCode code, const String& errorMessage)
{
    m_resolver->reject(DOMException::create(code, errorMessage));
    m_resolver.clear();
}

ExecutionContext* ContentDecryptionModuleResultPromise::getExecutionContext() const
{
    return m_resolver->getExecutionContext();
}

DEFINE_TRACE(ContentDecryptionModuleResultPromise)
{
    visitor->trace(m_resolver);
    ContentDecryptionModuleResult::trace(visitor);
}

} // namespace blink
