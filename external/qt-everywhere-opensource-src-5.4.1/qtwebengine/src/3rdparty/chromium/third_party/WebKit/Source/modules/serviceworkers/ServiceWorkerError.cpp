/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "ServiceWorkerError.h"

#include "core/dom/ExceptionCode.h"

using blink::WebServiceWorkerError;
namespace WebCore {

// static
PassRefPtrWillBeRawPtr<DOMException> ServiceWorkerError::from(ScriptPromiseResolverWithContext*, WebType* webErrorRaw)
{
    OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
    switch (webError->errorType) {
    case WebServiceWorkerError::ErrorTypeDisabled:
        return DOMException::create(NotSupportedError, "Service Worker support is disabled.");
    case WebServiceWorkerError::ErrorTypeAbort:
        return DOMException::create(AbortError, "The Service Worker operation was aborted.");
    case WebServiceWorkerError::ErrorTypeSecurity:
        return DOMException::create(SecurityError, "The Service Worker security policy prevented an action.");
    case WebServiceWorkerError::ErrorTypeInstall:
        // FIXME: Introduce new InstallError type to ExceptionCodes?
        return DOMException::create(AbortError, "The Service Worker installation failed.");
    case WebServiceWorkerError::ErrorTypeActivate:
        // Not currently returned as a promise rejection.
        // FIXME: Introduce new ActivateError type to ExceptionCodes?
        return DOMException::create(AbortError, "The Service Worker activation failed.");
    case WebServiceWorkerError::ErrorTypeNotFound:
        return DOMException::create(NotFoundError, "The specified Service Worker resource was not found.");
    case WebServiceWorkerError::ErrorTypeUnknown:
        return DOMException::create(UnknownError, "An unknown error occurred within Service Worker.");
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace WebCore
