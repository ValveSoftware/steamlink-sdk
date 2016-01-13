/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "modules/mediastream/NavigatorMediaStream.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "modules/mediastream/MediaDeviceInfoCallback.h"
#include "modules/mediastream/MediaDevicesRequest.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "modules/mediastream/UserMediaController.h"
#include "modules/mediastream/UserMediaRequest.h"

namespace WebCore {

NavigatorMediaStream::NavigatorMediaStream()
{
}

NavigatorMediaStream::~NavigatorMediaStream()
{
}

void NavigatorMediaStream::webkitGetUserMedia(Navigator& navigator, const Dictionary& options, PassOwnPtr<NavigatorUserMediaSuccessCallback> successCallback, PassOwnPtr<NavigatorUserMediaErrorCallback> errorCallback, ExceptionState& exceptionState)
{
    if (!successCallback)
        return;

    UserMediaController* userMedia = UserMediaController::from(navigator.frame());
    if (!userMedia) {
        exceptionState.throwDOMException(NotSupportedError, "No user media controller available; is this a detached window?");
        return;
    }

    RefPtrWillBeRawPtr<UserMediaRequest> request = UserMediaRequest::create(navigator.frame()->document(), userMedia, options, successCallback, errorCallback, exceptionState);
    if (!request) {
        ASSERT(exceptionState.hadException());
        return;
    }

    request->start();
}

void NavigatorMediaStream::getMediaDevices(Navigator& navigator, PassOwnPtr<MediaDeviceInfoCallback> callback, ExceptionState& exceptionState)
{
    UserMediaController* userMedia = UserMediaController::from(navigator.frame());
    if (!userMedia) {
        exceptionState.throwDOMException(NotSupportedError, "No media device controller available; is this a detached window?");
        return;
    }

    RefPtrWillBeRawPtr<MediaDevicesRequest> request = MediaDevicesRequest::create(navigator.frame()->document(), userMedia, callback, exceptionState);
    if (!request) {
        if (!exceptionState.hadException())
            exceptionState.throwDOMException(NotSupportedError, "Failed to request media devices.");
        return;
    }

    request->start();
}

} // namespace WebCore
