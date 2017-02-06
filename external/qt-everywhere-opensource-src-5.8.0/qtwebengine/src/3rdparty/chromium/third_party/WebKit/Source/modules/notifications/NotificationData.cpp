// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/vibration/VibrationController.h"
#include "public/platform/WebURL.h"
#include "wtf/CurrentTime.h"

namespace blink {
namespace {

WebNotificationData::Direction toDirectionEnumValue(const String& direction)
{
    if (direction == "ltr")
        return WebNotificationData::DirectionLeftToRight;
    if (direction == "rtl")
        return WebNotificationData::DirectionRightToLeft;

    return WebNotificationData::DirectionAuto;
}

WebURL completeURL(ExecutionContext* executionContext, const String& stringUrl)
{
    WebURL url = executionContext->completeURL(stringUrl);
    if (url.isValid())
        return url;
    return WebURL();
}

} // namespace

WebNotificationData createWebNotificationData(ExecutionContext* executionContext, const String& title, const NotificationOptions& options, ExceptionState& exceptionState)
{
    // If silent is true, the notification must not have a vibration pattern.
    if (options.hasVibrate() && options.silent()) {
        exceptionState.throwTypeError("Silent notifications must not specify vibration patterns.");
        return WebNotificationData();
    }

    // If renotify is true, the notification must have a tag.
    if (options.renotify() && options.tag().isEmpty()) {
        exceptionState.throwTypeError("Notifications which set the renotify flag must specify a non-empty tag.");
        return WebNotificationData();
    }

    WebNotificationData webData;

    webData.title = title;
    webData.direction = toDirectionEnumValue(options.dir());
    webData.lang = options.lang();
    webData.body = options.body();
    webData.tag = options.tag();

    if (options.hasIcon() && !options.icon().isEmpty())
        webData.icon = completeURL(executionContext, options.icon());

    if (options.hasBadge() && !options.badge().isEmpty())
        webData.badge = completeURL(executionContext, options.badge());

    webData.vibrate = VibrationController::sanitizeVibrationPattern(options.vibrate());
    webData.timestamp = options.hasTimestamp() ? static_cast<double>(options.timestamp()) : WTF::currentTimeMS();
    webData.renotify = options.renotify();
    webData.silent = options.silent();
    webData.requireInteraction = options.requireInteraction();

    if (options.hasData()) {
        const ScriptValue& data = options.data();
        v8::Isolate* isolate = data.isolate();
        DCHECK(isolate->InContext());
        RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::serialize(isolate, data.v8Value(), nullptr, nullptr, exceptionState);
        if (exceptionState.hadException())
            return WebNotificationData();

        Vector<char> serializedData;
        serializedScriptValue->toWireBytes(serializedData);

        webData.data = serializedData;
    }

    Vector<WebNotificationAction> actions;

    const size_t maxActions = Notification::maxActions();
    for (const NotificationAction& action : options.actions()) {
        if (actions.size() >= maxActions)
            break;

        WebNotificationAction webAction;
        webAction.action = action.action();
        webAction.title = action.title();

        if (action.type() == "button")
            webAction.type = WebNotificationAction::Button;
        else if (action.type() == "text")
            webAction.type = WebNotificationAction::Text;
        else
            NOTREACHED() << "Unknown action type: " << action.type();

        if (action.hasPlaceholder() && webAction.type == WebNotificationAction::Button) {
            exceptionState.throwTypeError("Notifications of type \"button\" cannot specify a placeholder.");
            return WebNotificationData();
        }

        webAction.placeholder = action.placeholder();

        if (action.hasIcon() && !action.icon().isEmpty())
            webAction.icon = completeURL(executionContext, action.icon());

        actions.append(webAction);
    }

    webData.actions = actions;

    return webData;
}

} // namespace blink
