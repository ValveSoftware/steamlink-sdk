// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/beacon/NavigatorBeacon.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DOMFormData.h"
#include "core/loader/BeaconLoader.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : m_transmittedBytes(0)
    , m_navigator(navigator)
{
}

const char* NavigatorBeacon::supplementName()
{
    return "NavigatorBeacon";
}

NavigatorBeacon& NavigatorBeacon::from(Navigator& navigator)
{
    NavigatorBeacon* supplement = static_cast<NavigatorBeacon*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorBeacon(navigator);
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

bool NavigatorBeacon::canSendBeacon(ExecutionContext* context, const KURL& url, ExceptionState& exceptionState)
{
    if (!url.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The URL argument is ill-formed or unsupported.");
        return false;
    }
    // For now, only support HTTP and related.
    if (!url.protocolIsInHTTPFamily()) {
        exceptionState.throwDOMException(SyntaxError, "Beacons are only supported over HTTP(S).");
        return false;
    }
    // FIXME: CSP is not enforced on redirects, crbug.com/372197
    if (!ContentSecurityPolicy::shouldBypassMainWorld(context) && !context->contentSecurityPolicy()->allowConnectToSource(url)) {
        // We can safely expose the URL to JavaScript, as these checks happen synchronously before redirection. JavaScript receives no new information.
        exceptionState.throwSecurityError("Refused to send beacon to '" + url.elidedString() + "' because it violates the document's Content Security Policy.");
        return false;
    }

    // Do not allow sending Beacons over a Navigator that is detached.
    if (!m_navigator.frame())
        return false;

    return true;
}

int NavigatorBeacon::maxAllowance() const
{
    const Settings* settings = m_navigator.frame()->settings();
    if (settings) {
        int maxAllowed = settings->maxBeaconTransmission();
        if (maxAllowed < m_transmittedBytes)
            return 0;
        return maxAllowed - m_transmittedBytes;
    }
    return m_transmittedBytes;
}

void NavigatorBeacon::updateTransmittedBytes(int length)
{
    ASSERT(length >= 0);
    m_transmittedBytes += length;
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, Navigator& navigator, const String& urlstring, const String& data, ExceptionState& exceptionState)
{
    return NavigatorBeacon::from(navigator).sendBeacon(context, urlstring, data, exceptionState);
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, const String& urlstring, const String& data, ExceptionState& exceptionState)
{
    KURL url = context->completeURL(urlstring);
    if (!canSendBeacon(context, url, exceptionState))
        return false;

    int bytes = 0;
    bool result = BeaconLoader::sendBeacon(m_navigator.frame(), maxAllowance(), url, data, bytes);
    if (result)
        updateTransmittedBytes(bytes);

    return result;
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, Navigator& navigator, const String& urlstring, PassRefPtr<ArrayBufferView> data, ExceptionState& exceptionState)
{
    return NavigatorBeacon::from(navigator).sendBeacon(context, urlstring, data, exceptionState);
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, const String& urlstring, PassRefPtr<ArrayBufferView> data, ExceptionState& exceptionState)
{
    KURL url = context->completeURL(urlstring);
    if (!canSendBeacon(context, url, exceptionState))
        return false;

    int bytes = 0;
    bool result = BeaconLoader::sendBeacon(m_navigator.frame(), maxAllowance(), url, data, bytes);
    if (result)
        updateTransmittedBytes(bytes);

    return result;
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, Navigator& navigator, const String& urlstring, PassRefPtrWillBeRawPtr<Blob> data, ExceptionState& exceptionState)
{
    return NavigatorBeacon::from(navigator).sendBeacon(context, urlstring, data, exceptionState);
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, const String& urlstring, PassRefPtrWillBeRawPtr<Blob> data, ExceptionState& exceptionState)
{
    KURL url = context->completeURL(urlstring);
    if (!canSendBeacon(context, url, exceptionState))
        return false;

    int bytes = 0;
    bool result = BeaconLoader::sendBeacon(m_navigator.frame(), maxAllowance(), url, data, bytes);
    if (result)
        updateTransmittedBytes(bytes);

    return result;
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, Navigator& navigator, const String& urlstring, PassRefPtrWillBeRawPtr<DOMFormData> data, ExceptionState& exceptionState)
{
    return NavigatorBeacon::from(navigator).sendBeacon(context, urlstring, data, exceptionState);
}

bool NavigatorBeacon::sendBeacon(ExecutionContext* context, const String& urlstring, PassRefPtrWillBeRawPtr<DOMFormData> data, ExceptionState& exceptionState)
{
    KURL url = context->completeURL(urlstring);
    if (!canSendBeacon(context, url, exceptionState))
        return false;

    int bytes = 0;
    bool result = BeaconLoader::sendBeacon(m_navigator.frame(), maxAllowance(), url, data, bytes);
    if (result)
        updateTransmittedBytes(bytes);

    return result;
}

} // namespace WebCore
