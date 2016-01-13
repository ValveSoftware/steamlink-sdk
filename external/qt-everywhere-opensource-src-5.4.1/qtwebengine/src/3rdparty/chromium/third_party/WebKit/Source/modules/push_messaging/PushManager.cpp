// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushManager.h"

#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/push_messaging/PushController.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushRegistration.h"
#include "public/platform/WebPushClient.h"
#include "wtf/RefPtr.h"

namespace WebCore {

PushManager::PushManager()
{
    ScriptWrappable::init(this);
}

PushManager::~PushManager()
{
}

ScriptPromise PushManager::registerPushMessaging(ScriptState* scriptState, const String& senderId)
{
    ASSERT(scriptState->executionContext()->isDocument());
    RefPtr<ScriptPromiseResolverWithContext> resolver = ScriptPromiseResolverWithContext::create(scriptState);
    ScriptPromise promise = resolver->promise();
    blink::WebPushClient* client = PushController::clientFrom(toDocument(scriptState->executionContext())->page());
    ASSERT(client);
    client->registerPushMessaging(senderId, new CallbackPromiseAdapter<PushRegistration, PushError>(resolver));
    return promise;
}

} // namespace WebCore
