/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
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

#ifndef Geolocation_h
#define Geolocation_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/geolocation/Geoposition.h"
#include "modules/geolocation/PositionCallback.h"
#include "modules/geolocation/PositionError.h"
#include "modules/geolocation/PositionErrorCallback.h"
#include "modules/geolocation/PositionOptions.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Document;
class LocalFrame;
class GeolocationController;
class GeolocationError;
class GeolocationPosition;
class ExecutionContext;

class Geolocation FINAL
    : public GarbageCollectedFinalized<Geolocation>
    , public ScriptWrappable
    , public ActiveDOMObject {
public:
    static Geolocation* create(ExecutionContext*);
    virtual ~Geolocation();
    void trace(Visitor*);

    virtual void stop() OVERRIDE;
    Document* document() const;
    LocalFrame* frame() const;

    // Creates a oneshot and attempts to obtain a position that meets the
    // constraints of the options.
    void getCurrentPosition(PassOwnPtr<PositionCallback>, PassOwnPtr<PositionErrorCallback>, PositionOptions*);

    // Creates a watcher that will be notified whenever a new position is
    // available that meets the constraints of the options.
    int watchPosition(PassOwnPtr<PositionCallback>, PassOwnPtr<PositionErrorCallback>, PositionOptions*);

    // Removes all references to the watcher, it will not be updated again.
    void clearWatch(int watchID);

    void setIsAllowed(bool);

    bool isAllowed() const { return m_allowGeolocation == Yes; }

    // Notifies this that a new position is available. Must never be called
    // before permission is granted by the user.
    void positionChanged();

    // Notifies this that an error has occurred, it must be handled immediately.
    void setError(GeolocationError*);

private:
    // Returns the last known position, if any. May return null.
    Geoposition* lastPosition();

    bool isDenied() const { return m_allowGeolocation == No; }

    explicit Geolocation(ExecutionContext*);

    // Holds the success and error callbacks and the options that were provided
    // when a oneshot or watcher were created. Also, if specified in the
    // options, manages a timer to limit the time to wait for the system to
    // obtain a position.
    class GeoNotifier : public GarbageCollectedFinalized<GeoNotifier> {
    public:
        static GeoNotifier* create(Geolocation* geolocation, PassOwnPtr<PositionCallback> positionCallback, PassOwnPtr<PositionErrorCallback> positionErrorCallback, PositionOptions* options)
        {
            return new GeoNotifier(geolocation, positionCallback, positionErrorCallback, options);
        }
        void trace(Visitor*);

        PositionOptions* options() const { return m_options.get(); };

        // Sets the given error as the fatal error if there isn't one yet.
        // Starts the timer with an interval of 0.
        void setFatalError(PositionError*);

        bool useCachedPosition() const { return m_useCachedPosition; }

        // Tells the notifier to use a cached position and starts its timer with
        // an interval of 0.
        void setUseCachedPosition();

        void runSuccessCallback(Geoposition*);
        void runErrorCallback(PositionError*);

        void startTimer();
        void stopTimer();

        // Runs the error callback if there is a fatal error. Otherwise, if a
        // cached position must be used, registers itself for receiving one.
        // Otherwise, the notifier has expired, and its error callback is run.
        void timerFired(Timer<GeoNotifier>*);

    private:
        GeoNotifier(Geolocation*, PassOwnPtr<PositionCallback>, PassOwnPtr<PositionErrorCallback>, PositionOptions*);

        Member<Geolocation> m_geolocation;
        OwnPtr<PositionCallback> m_successCallback;
        OwnPtr<PositionErrorCallback> m_errorCallback;
        Member<PositionOptions> m_options;
        Timer<GeoNotifier> m_timer;
        Member<PositionError> m_fatalError;
        bool m_useCachedPosition;
    };

    typedef HeapVector<Member<GeoNotifier> > GeoNotifierVector;
    typedef HeapHashSet<Member<GeoNotifier> > GeoNotifierSet;

    class Watchers {
        DISALLOW_ALLOCATION();
    public:
        void trace(Visitor*);
        bool add(int id, GeoNotifier*);
        GeoNotifier* find(int id);
        void remove(int id);
        void remove(GeoNotifier*);
        bool contains(GeoNotifier*) const;
        void clear();
        bool isEmpty() const;
        void getNotifiersVector(GeoNotifierVector&) const;
    private:
        typedef HeapHashMap<int, Member<GeoNotifier> > IdToNotifierMap;
        typedef HeapHashMap<Member<GeoNotifier>, int> NotifierToIdMap;
        IdToNotifierMap m_idToNotifierMap;
        NotifierToIdMap m_notifierToIdMap;
    };

    bool hasListeners() const { return !m_oneShots.isEmpty() || !m_watchers.isEmpty(); }

    void sendError(GeoNotifierVector&, PositionError*);
    void sendPosition(GeoNotifierVector&, Geoposition*);

    // Removes notifiers that use a cached position from |notifiers| and
    // if |cached| is not null they are added to it.
    static void extractNotifiersWithCachedPosition(GeoNotifierVector& notifiers, GeoNotifierVector* cached);

    // Copies notifiers from |src| vector to |dest| set.
    static void copyToSet(const GeoNotifierVector& src, GeoNotifierSet& dest);

    static void stopTimer(GeoNotifierVector&);
    void stopTimersForOneShots();
    void stopTimersForWatchers();
    void stopTimers();

    // Sets a fatal error on the given notifiers.
    void cancelRequests(GeoNotifierVector&);

    // Sets a fatal error on all notifiers.
    void cancelAllRequests();

    // Runs the success callbacks on all notifiers. A position must be available
    // and the user must have given permission.
    void makeSuccessCallbacks();

    // Sends the given error to all notifiers, unless the error is not fatal and
    // the notifier is due to receive a cached position. Clears the oneshots,
    // and also  clears the watchers if the error is fatal.
    void handleError(PositionError*);

    // Requests permission to share positions with the page.
    void requestPermission();

    // Attempts to register this with the controller for receiving updates.
    // Returns false if there is no controller to register with.
    bool startUpdating(GeoNotifier*);

    void stopUpdating();

    // Processes the notifiers that were waiting for a permission decision. If
    // granted and this can be registered with the controller then the
    // notifier's timers are started. Otherwise, a fatal error is set on them.
    void handlePendingPermissionNotifiers();

    // Attempts to obtain a position for the given notifier, either by using
    // the cached position or by requesting one from the controller. Sets a
    // fatal error if permission is denied or no position can be obtained.
    void startRequest(GeoNotifier*);

    // Discards the notifier because a fatal error occurred for it.
    void fatalErrorOccurred(GeoNotifier*);

    // Discards the notifier if it is a oneshot because it timed it.
    void requestTimedOut(GeoNotifier*);

    // Adds the notifier to the set awaiting a cached position. Runs the success
    // callbacks for them if permission has been granted. Requests permission if
    // it is unknown.
    void requestUsesCachedPosition(GeoNotifier*);

    bool haveSuitableCachedPosition(PositionOptions*);

    // Runs the success callbacks for the set of notifiers awaiting a cached
    // position, the set is then cleared. The oneshots are removed everywhere.
    void makeCachedPositionCallbacks();

    GeoNotifierSet m_oneShots;
    Watchers m_watchers;
    GeoNotifierSet m_pendingForPermissionNotifiers;
    Member<Geoposition> m_lastPosition;

    enum {
        Unknown,
        InProgress,
        Yes,
        No
    } m_allowGeolocation;

    GeoNotifierSet m_requestsAwaitingCachedPosition;
};

} // namespace WebCore

#endif // Geolocation_h
