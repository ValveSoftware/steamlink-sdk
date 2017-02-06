// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/NotificationPermissionClientImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationPermissionCallback.h"
#include "public/web/WebFrameClient.h"
#include "public/web/modules/notifications/WebNotificationPermissionCallback.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

namespace {

class WebNotificationPermissionCallbackImpl : public WebNotificationPermissionCallback {
public:
    WebNotificationPermissionCallbackImpl(ScriptPromiseResolver* resolver, NotificationPermissionCallback* deprecatedCallback)
        : m_resolver(resolver)
        , m_deprecatedCallback(deprecatedCallback)
    {
    }

    ~WebNotificationPermissionCallbackImpl() override { }

    void permissionRequestComplete(mojom::blink::PermissionStatus permissionStatus) override
    {
        String permissionString = Notification::permissionString(permissionStatus);
        if (m_deprecatedCallback)
            m_deprecatedCallback->handleEvent(permissionString);

        m_resolver->resolve(permissionString);
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    Persistent<NotificationPermissionCallback> m_deprecatedCallback;
};

} // namespace

NotificationPermissionClientImpl* NotificationPermissionClientImpl::create()
{
    return new NotificationPermissionClientImpl();
}

NotificationPermissionClientImpl::NotificationPermissionClientImpl()
{
}

NotificationPermissionClientImpl::~NotificationPermissionClientImpl()
{
}

ScriptPromise NotificationPermissionClientImpl::requestPermission(ScriptState* scriptState, NotificationPermissionCallback* deprecatedCallback)
{
    DCHECK(scriptState);

    ExecutionContext* context = scriptState->getExecutionContext();
    DCHECK(context);
    DCHECK(context->isDocument());

    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    webFrame->client()->requestNotificationPermission(WebSecurityOrigin(context->getSecurityOrigin()), new WebNotificationPermissionCallbackImpl(resolver, deprecatedCallback));

    return promise;
}

} // namespace blink
