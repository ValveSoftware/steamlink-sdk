/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/geolocation/Geolocation.h"

#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/Settings.h"
#include "modules/geolocation/Coordinates.h"
#include "modules/geolocation/GeolocationError.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"

namespace blink {
namespace {

static const char permissionDeniedErrorMessage[] = "User denied Geolocation";
static const char failedToStartServiceErrorMessage[] = "Failed to start Geolocation service";
static const char framelessDocumentErrorMessage[] = "Geolocation cannot be used in frameless documents";

static Geoposition* createGeoposition(const mojom::blink::Geoposition& position)
{
    Coordinates* coordinates = Coordinates::create(
        position.latitude,
        position.longitude,
        // Lowest point on land is at approximately -400 meters.
        position.altitude > -10000.,
        position.altitude,
        position.accuracy,
        position.altitude_accuracy >= 0.,
        position.altitude_accuracy,
        position.heading >= 0. && position.heading <= 360.,
        position.heading,
        position.speed >= 0.,
        position.speed);
    return Geoposition::create(coordinates, convertSecondsToDOMTimeStamp(position.timestamp));
}

static PositionError* createPositionError(mojom::blink::Geoposition::ErrorCode mojomErrorCode, const String& error)
{
    PositionError::ErrorCode errorCode = PositionError::POSITION_UNAVAILABLE;
    switch (mojomErrorCode) {
    case mojom::blink::Geoposition::ErrorCode::PERMISSION_DENIED:
        errorCode = PositionError::PERMISSION_DENIED;
        break;
    case mojom::blink::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
        errorCode = PositionError::POSITION_UNAVAILABLE;
        break;
    case mojom::blink::Geoposition::ErrorCode::NONE:
    case mojom::blink::Geoposition::ErrorCode::TIMEOUT:
        NOTREACHED();
        break;
    }
    return PositionError::create(errorCode, error);
}

} // namespace

Geolocation* Geolocation::create(ExecutionContext* context)
{
    Geolocation* geolocation = new Geolocation(context);
    return geolocation;
}

Geolocation::Geolocation(ExecutionContext* context)
    : ContextLifecycleObserver(context)
    , PageLifecycleObserver(document()->page())
    , m_geolocationPermission(PermissionUnknown)
{
}

Geolocation::~Geolocation()
{
    DCHECK(m_geolocationPermission != PermissionRequested);
}

DEFINE_TRACE(Geolocation)
{
    visitor->trace(m_oneShots);
    visitor->trace(m_watchers);
    visitor->trace(m_pendingForPermissionNotifiers);
    visitor->trace(m_lastPosition);
    ContextLifecycleObserver::trace(visitor);
    PageLifecycleObserver::trace(visitor);
}

Document* Geolocation::document() const
{
    return toDocument(getExecutionContext());
}

LocalFrame* Geolocation::frame() const
{
    return document() ? document()->frame() : 0;
}

void Geolocation::contextDestroyed()
{
    m_permissionService.reset();
    cancelAllRequests();
    stopUpdating();
    m_geolocationPermission = PermissionDenied;
    m_pendingForPermissionNotifiers.clear();
    m_lastPosition = nullptr;
    ContextLifecycleObserver::clearContext();
    PageLifecycleObserver::clearContext();
}

void Geolocation::recordOriginTypeAccess() const
{
    DCHECK(frame());

    Document* document = this->document();
    DCHECK(document);

    // It is required by isSecureContext() but isn't
    // actually used. This could be used later if a warning is shown in the
    // developer console.
    String insecureOriginMsg;
    if (document->isSecureContext(insecureOriginMsg)) {
        UseCounter::count(document, UseCounter::GeolocationSecureOrigin);
        UseCounter::countCrossOriginIframe(*document, UseCounter::GeolocationSecureOriginIframe);
    } else if (frame()->settings()->allowGeolocationOnInsecureOrigins()) {
        // TODO(jww): This should be removed after WebView is fixed so that it
        // disallows geolocation in insecure contexts.
        //
        // See https://crbug.com/603574.
        Deprecation::countDeprecation(document, UseCounter::GeolocationInsecureOriginDeprecatedNotRemoved);
        Deprecation::countDeprecationCrossOriginIframe(*document, UseCounter::GeolocationInsecureOriginIframeDeprecatedNotRemoved);
        HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::GeolocationInsecureHost);
    } else {
        Deprecation::countDeprecation(document, UseCounter::GeolocationInsecureOrigin);
        Deprecation::countDeprecationCrossOriginIframe(*document, UseCounter::GeolocationInsecureOriginIframe);
        HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::GeolocationInsecureHost);
    }
}

void Geolocation::getCurrentPosition(PositionCallback* successCallback, PositionErrorCallback* errorCallback, const PositionOptions& options)
{
    if (!frame())
        return;

    GeoNotifier* notifier = GeoNotifier::create(this, successCallback, errorCallback, options);
    startRequest(notifier);

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PositionCallback* successCallback, PositionErrorCallback* errorCallback, const PositionOptions& options)
{
    if (!frame())
        return 0;

    GeoNotifier* notifier = GeoNotifier::create(this, successCallback, errorCallback, options);
    startRequest(notifier);

    int watchID;
    // Keep asking for the next id until we're given one that we don't already have.
    do {
        watchID = getExecutionContext()->circularSequentialID();
    } while (!m_watchers.add(watchID, notifier));
    return watchID;
}

void Geolocation::startRequest(GeoNotifier *notifier)
{
    recordOriginTypeAccess();
    String errorMessage;
    if (!frame()->settings()->allowGeolocationOnInsecureOrigins() && !getExecutionContext()->isSecureContext(errorMessage)) {
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, errorMessage));
        return;
    }

    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied())
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    else if (haveSuitableCachedPosition(notifier->options()))
        notifier->setUseCachedPosition();
    else if (!notifier->options().timeout())
        notifier->startTimer();
    else if (!isAllowed()) {
        // if we don't yet have permission, request for permission before calling startUpdating()
        m_pendingForPermissionNotifiers.add(notifier);
        requestPermission();
    } else {
        startUpdating(notifier);
        notifier->startTimer();
    }
}

void Geolocation::fatalErrorOccurred(GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    m_watchers.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestUsesCachedPosition(GeoNotifier* notifier)
{
    DCHECK(isAllowed());

    notifier->runSuccessCallback(m_lastPosition);

    // If this is a one-shot request, stop it. Otherwise, if the watch still
    // exists, start the service to get updates.
    if (m_oneShots.contains(notifier)) {
        m_oneShots.remove(notifier);
    } else if (m_watchers.contains(notifier)) {
        if (notifier->options().timeout())
            startUpdating(notifier);
        notifier->startTimer();
    }

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestTimedOut(GeoNotifier* notifier)
{
    // If this is a one-shot request, stop it.
    m_oneShots.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

bool Geolocation::haveSuitableCachedPosition(const PositionOptions& options)
{
    if (!m_lastPosition)
        return false;
    DCHECK(isAllowed());
    if (!options.maximumAge())
        return false;
    DOMTimeStamp currentTimeMillis = convertSecondsToDOMTimeStamp(currentTime());
    return m_lastPosition->timestamp() > currentTimeMillis - options.maximumAge();
}

void Geolocation::clearWatch(int watchID)
{
    if (watchID <= 0)
        return;

    if (GeoNotifier* notifier = m_watchers.find(watchID))
        m_pendingForPermissionNotifiers.remove(notifier);
    m_watchers.remove(watchID);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::onGeolocationPermissionUpdated(mojom::blink::PermissionStatus status)
{
    // This may be due to either a new position from the service, or a cached position.
    m_geolocationPermission = status == mojom::blink::PermissionStatus::GRANTED ? PermissionAllowed : PermissionDenied;
    m_permissionService.reset();

    // While we iterate through the list, we need not worry about the list being modified as the permission
    // is already set to Yes/No and no new listeners will be added to the pending list.
    for (GeoNotifier* notifier : m_pendingForPermissionNotifiers) {
        if (isAllowed()) {
            // Start all pending notification requests as permission granted.
            // The notifier is always ref'ed by m_oneShots or m_watchers.
            startUpdating(notifier);
            notifier->startTimer();
        } else {
            notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
        }
    }
    m_pendingForPermissionNotifiers.clear();
}

void Geolocation::sendError(GeoNotifierVector& notifiers, PositionError* error)
{
    for (GeoNotifier* notifier : notifiers)
        notifier->runErrorCallback(error);
}

void Geolocation::sendPosition(GeoNotifierVector& notifiers, Geoposition* position)
{
    for (GeoNotifier* notifier : notifiers)
        notifier->runSuccessCallback(position);
}

void Geolocation::stopTimer(GeoNotifierVector& notifiers)
{
    for (GeoNotifier* notifier : notifiers)
        notifier->stopTimer();
}

void Geolocation::stopTimersForOneShots()
{
    GeoNotifierVector copy;
    copyToVector(m_oneShots, copy);

    stopTimer(copy);
}

void Geolocation::stopTimersForWatchers()
{
    GeoNotifierVector copy;
    m_watchers.getNotifiersVector(copy);

    stopTimer(copy);
}

void Geolocation::stopTimers()
{
    stopTimersForOneShots();
    stopTimersForWatchers();
}

void Geolocation::cancelRequests(GeoNotifierVector& notifiers)
{
    for (GeoNotifier* notifier : notifiers)
        notifier->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, framelessDocumentErrorMessage));
}

void Geolocation::cancelAllRequests()
{
    GeoNotifierVector copy;
    copyToVector(m_oneShots, copy);
    cancelRequests(copy);
    m_watchers.getNotifiersVector(copy);
    cancelRequests(copy);
}

void Geolocation::extractNotifiersWithCachedPosition(GeoNotifierVector& notifiers, GeoNotifierVector* cached)
{
    GeoNotifierVector nonCached;
    for (GeoNotifier* notifier : notifiers) {
        if (notifier->useCachedPosition()) {
            if (cached)
                cached->append(notifier);
        } else
            nonCached.append(notifier);
    }
    notifiers.swap(nonCached);
}

void Geolocation::copyToSet(const GeoNotifierVector& src, GeoNotifierSet& dest)
{
    for (GeoNotifier* notifier : src)
         dest.add(notifier);
}

void Geolocation::handleError(PositionError* error)
{
    DCHECK(error);

    GeoNotifierVector oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    GeoNotifierVector oneShotsWithCachedPosition;
    m_oneShots.clear();
    if (error->isFatal())
        m_watchers.clear();
    else {
        // Don't send non-fatal errors to notifiers due to receive a cached position.
        extractNotifiersWithCachedPosition(oneShotsCopy, &oneShotsWithCachedPosition);
        extractNotifiersWithCachedPosition(watchersCopy, 0);
    }

    sendError(oneShotsCopy, error);
    sendError(watchersCopy, error);

    // hasListeners() doesn't distinguish between notifiers due to receive a
    // cached position and those requiring a fresh position. Perform the check
    // before restoring the notifiers below.
    if (!hasListeners())
        stopUpdating();

    // Maintain a reference to the cached notifiers until their timer fires.
    copyToSet(oneShotsWithCachedPosition, m_oneShots);
}

void Geolocation::requestPermission()
{
    if (m_geolocationPermission != PermissionUnknown)
        return;

    LocalFrame* frame = this->frame();
    if (!frame)
        return;

    m_geolocationPermission = PermissionRequested;
    frame->serviceRegistry()->connectToRemoteService(
        mojo::GetProxy(&m_permissionService));
    m_permissionService.set_connection_error_handler(createBaseCallback(WTF::bind(&Geolocation::onPermissionConnectionError, wrapWeakPersistent(this))));

    // Ask the embedder: it maintains the geolocation challenge policy itself.
    m_permissionService->RequestPermission(
        mojom::blink::PermissionName::GEOLOCATION,
        getExecutionContext()->getSecurityOrigin()->toString(),
        UserGestureIndicator::processingUserGesture(),
        createBaseCallback(WTF::bind(&Geolocation::onGeolocationPermissionUpdated, wrapPersistent(this))));
}

void Geolocation::makeSuccessCallbacks()
{
    DCHECK(m_lastPosition);
    DCHECK(isAllowed());

    GeoNotifierVector oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();

    sendPosition(oneShotsCopy, m_lastPosition);
    sendPosition(watchersCopy, m_lastPosition);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::positionChanged()
{
    DCHECK(isAllowed());

    // Stop all currently running timers.
    stopTimers();

    makeSuccessCallbacks();
}

void Geolocation::startUpdating(GeoNotifier* notifier)
{
    m_updating = true;
    if (notifier->options().enableHighAccuracy() && !m_enableHighAccuracy) {
        m_enableHighAccuracy = true;
        if (m_geolocationService)
            m_geolocationService->SetHighAccuracy(true);
    }
    updateGeolocationServiceConnection();
}

void Geolocation::stopUpdating()
{
    m_updating = false;
    updateGeolocationServiceConnection();
    m_enableHighAccuracy = false;
}

void Geolocation::updateGeolocationServiceConnection()
{
    if (!getExecutionContext() || !page() || !page()->isPageVisible() || !m_updating) {
        m_geolocationService.reset();
        m_disconnectedGeolocationService = true;
        return;
    }
    if (m_geolocationService)
        return;

    frame()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_geolocationService));
    m_geolocationService.set_connection_error_handler(createBaseCallback(WTF::bind(&Geolocation::onGeolocationConnectionError, wrapWeakPersistent(this))));
    if (m_enableHighAccuracy)
        m_geolocationService->SetHighAccuracy(true);
    queryNextPosition();
}

void Geolocation::queryNextPosition()
{
    m_geolocationService->QueryNextPosition(createBaseCallback(WTF::bind(&Geolocation::onPositionUpdated, wrapPersistent(this))));
}

void Geolocation::onPositionUpdated(mojom::blink::GeopositionPtr position)
{
    m_disconnectedGeolocationService = false;
    if (position->valid) {
        m_lastPosition = createGeoposition(*position);
        positionChanged();
    } else {
        handleError(createPositionError(position->error_code, position->error_message));
    }
    if (!m_disconnectedGeolocationService)
        queryNextPosition();
}

void Geolocation::pageVisibilityChanged()
{
    updateGeolocationServiceConnection();
}

void Geolocation::onGeolocationConnectionError()
{
    // If a request is outstanding at process shutdown, this error handler will
    // be called. In that case, blink has already shut down so do nothing.
    //
    // TODO(sammc): Remove this once renderer shutdown is no longer graceful.
    if (!Platform::current())
        return;

    PositionError* error = PositionError::create(PositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage);
    error->setIsFatal(true);
    handleError(error);
}

void Geolocation::onPermissionConnectionError()
{
    // If a request is outstanding at process shutdown, this error handler will
    // be called. In that case, blink has already shut down so do nothing.
    //
    // TODO(sammc): Remove this once renderer shutdown is no longer graceful.
    if (!Platform::current())
        return;

    onGeolocationPermissionUpdated(mojom::blink::PermissionStatus::DENIED);
}

} // namespace blink
