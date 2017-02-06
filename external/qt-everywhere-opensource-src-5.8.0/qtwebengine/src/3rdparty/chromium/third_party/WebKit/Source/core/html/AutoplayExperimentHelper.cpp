// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayExperimentHelper.h"

#include "core/dom/Document.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/page/Page.h"
#include "platform/Logging.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntRect.h"

namespace blink {

using namespace HTMLNames;

// Seconds to wait after a video has stopped moving before playing it.
static const double kViewportTimerPollDelay = 0.5;

AutoplayExperimentHelper::AutoplayExperimentHelper(Client* client)
    : m_client(client)
    , m_mode(Mode::ExperimentOff)
    , m_playPending(false)
    , m_registeredWithLayoutObject(false)
    , m_wasInViewport(false)
    , m_autoplayMediaEncountered(false)
    , m_playbackStartedMetricRecorded(false)
    , m_waitingForAutoplayPlaybackStop(false)
    , m_recordedElement(false)
    , m_lastLocationUpdateTime(-std::numeric_limits<double>::infinity())
    , m_viewportTimer(this, &AutoplayExperimentHelper::viewportTimerFired)
    , m_autoplayDeferredMetric(GesturelessPlaybackNotOverridden)
{
    m_mode = fromString(this->client().autoplayExperimentMode());

    DVLOG_IF(3, isExperimentEnabled()) << "autoplay experiment set to " << m_mode;
}

AutoplayExperimentHelper::~AutoplayExperimentHelper()
{
}

void AutoplayExperimentHelper::becameReadyToPlay()
{
    // Assuming that we're eligible to override the user gesture requirement,
    // either play if we meet the visibility checks, or install a listener
    // to wait for them to pass.  We do not actually start playback; our
    // caller must do that.
    autoplayMediaEncountered();

    if (isEligible()) {
        if (meetsVisibilityRequirements())
            prepareToAutoplay(GesturelessPlaybackStartedByAutoplayFlagImmediately);
        else
            registerForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::playMethodCalled()
{
    // If a play is already pending, then do nothing.  We're already trying
    // to play.  Similarly, do nothing if we're already playing.
    if (m_playPending || !m_client->paused())
        return;

    if (!UserGestureIndicator::utilizeUserGesture()) {
        autoplayMediaEncountered();

        // Check for eligibility, but don't worry if playback is currently
        // pending.  If we're still not eligible, then this play() will fail.
        if (isEligible(IgnorePendingPlayback)) {
            m_playPending = true;

            // If we are able to override the gesture requirement now, then
            // do so.  Otherwise, install an event listener if we need one.
            // We do not actually start playback; play() will do that.
            if (meetsVisibilityRequirements()) {
                // Override the gesture and assume that play() will succeed.
                prepareToAutoplay(GesturelessPlaybackStartedByPlayMethodImmediately);
            } else {
                // Wait for viewport visibility.
                // TODO(liberato): if the autoplay is allowed soon enough, then
                // it should still record *Immediately.  Otherwise, we end up
                // here before the first layout sometimes, when the item is
                // visible but we just don't know that yet.
                registerForPositionUpdatesIfNeeded();
            }
        }
    } else if (isLockedPendingUserGesture()) {
        // If this media tried to autoplay, and we haven't played it yet, then
        // record that the user provided the gesture to start it the first time.
        if (m_autoplayMediaEncountered && !m_playbackStartedMetricRecorded)
            recordAutoplayMetric(AutoplayManualStart);
        // Don't let future gestureless playbacks affect metrics.
        m_autoplayMediaEncountered = true;
        m_playbackStartedMetricRecorded = true;
        m_playPending = false;

        unregisterForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::pauseMethodCalled()
{
    // Don't try to autoplay, if we would have.
    m_playPending = false;
    unregisterForPositionUpdatesIfNeeded();
}

void AutoplayExperimentHelper::loadMethodCalled()
{
    if (isLockedPendingUserGesture() && UserGestureIndicator::utilizeUserGesture()) {
        recordAutoplayMetric(AutoplayEnabledThroughLoad);
        unlockUserGesture(GesturelessPlaybackEnabledByLoad);
    }
}

void AutoplayExperimentHelper::mutedChanged()
{
    // Mute changes are always allowed if this is unlocked.
    if (!client().isLockedPendingUserGesture())
        return;

    // Changes with a user gesture are okay.
    if (UserGestureIndicator::utilizeUserGesture())
        return;

    // If the mute state has changed to 'muted', then it's okay.
    if (client().muted())
        return;

    // If nothing is playing, then changes are okay too.
    if (client().paused())
        return;

    // Trying to unmute without a user gesture.

    // If we don't care about muted state, then it's okay.
    if (!enabled(IfMuted) && !(client().isCrossOrigin() && enabled(OrMuted)))
        return;

    // Unmuting isn't allowed, so pause.
    client().pauseInternal();
}

void AutoplayExperimentHelper::registerForPositionUpdatesIfNeeded()
{
    // If we don't require that the player is in the viewport, then we don't
    // need the listener.
    if (!requiresViewportVisibility()) {
        if (!enabled(IfPageVisible))
            return;
    }

    m_client->setRequestPositionUpdates(true);

    // Set this unconditionally, in case we have no layout object yet.
    m_registeredWithLayoutObject = true;
}

void AutoplayExperimentHelper::unregisterForPositionUpdatesIfNeeded()
{
    if (m_registeredWithLayoutObject)
        m_client->setRequestPositionUpdates(false);

    // Clear this unconditionally so that we don't re-register if we didn't
    // have a LayoutObject now, but get one later.
    m_registeredWithLayoutObject = false;
}

void AutoplayExperimentHelper::positionChanged(const IntRect& visibleRect)
{
    // Something, maybe position, has changed.  If applicable, start a
    // timer to look for the end of a scroll operation.
    // Don't do much work here.
    // Also note that we are called quite often, including when the
    // page becomes visible.  That's why we don't bother to register
    // for page visibility changes explicitly.
    if (visibleRect.isEmpty())
        return;

    m_lastVisibleRect = visibleRect;

    IntRect currentLocation = client().absoluteBoundingBoxRect();
    if (currentLocation.isEmpty())
        return;

    bool inViewport = meetsVisibilityRequirements();

    if (m_lastLocation != currentLocation) {
        m_lastLocationUpdateTime = monotonicallyIncreasingTime();
        m_lastLocation = currentLocation;
    }

    if (inViewport && !m_wasInViewport) {
        // Only reset the timer when we transition from not visible to
        // visible, because resetting the timer isn't cheap.
        m_viewportTimer.startOneShot(kViewportTimerPollDelay, BLINK_FROM_HERE);
    }
    m_wasInViewport = inViewport;
}

void AutoplayExperimentHelper::updatePositionNotificationRegistration()
{
    if (m_registeredWithLayoutObject)
        m_client->setRequestPositionUpdates(true);
}

void AutoplayExperimentHelper::triggerAutoplayViewportCheckForTesting()
{
    // Make sure that the last update appears to be sufficiently far in the
    // past to appear that scrolling has stopped by now in viewportTimerFired.
    m_lastLocationUpdateTime = monotonicallyIncreasingTime() - kViewportTimerPollDelay - 1;
    viewportTimerFired(nullptr);
}

void AutoplayExperimentHelper::viewportTimerFired(Timer<AutoplayExperimentHelper>*)
{
    double now = monotonicallyIncreasingTime();
    double delta = now - m_lastLocationUpdateTime;
    if (delta < kViewportTimerPollDelay) {
        // If we are not visible, then skip the timer.  It will be started
        // again if we become visible again.
        if (m_wasInViewport)
            m_viewportTimer.startOneShot(kViewportTimerPollDelay - delta, BLINK_FROM_HERE);

        return;
    }

    // Sufficient time has passed since the last scroll that we'll
    // treat it as the end of scroll.  Autoplay if we should.
    maybeStartPlaying();
}

bool AutoplayExperimentHelper::meetsVisibilityRequirements() const
{
    if (enabled(IfPageVisible)
        && client().pageVisibilityState() != PageVisibilityStateVisible)
        return false;

    if (!requiresViewportVisibility())
        return true;

    if (m_lastVisibleRect.isEmpty())
        return false;

    IntRect currentLocation = client().absoluteBoundingBoxRect();
    if (currentLocation.isEmpty())
        return false;

    // In partial-viewport mode, we require only 1x1 area.
    if (enabled(IfPartialViewport)) {
        return m_lastVisibleRect.intersects(currentLocation);
    }

    // Element must be completely visible, or as much as fits.
    // If element completely fills the screen, then truncate it to exactly
    // match the screen.  Any element that is wider just has to cover.
    if (currentLocation.x() <= m_lastVisibleRect.x()
        && currentLocation.x() + currentLocation.width() >= m_lastVisibleRect.x() + m_lastVisibleRect.width()) {
        currentLocation.setX(m_lastVisibleRect.x());
        currentLocation.setWidth(m_lastVisibleRect.width());
    }

    if (currentLocation.y() <= m_lastVisibleRect.y()
        && currentLocation.y() + currentLocation.height() >= m_lastVisibleRect.y() + m_lastVisibleRect.height()) {
        currentLocation.setY(m_lastVisibleRect.y());
        currentLocation.setHeight(m_lastVisibleRect.height());
    }

    return m_lastVisibleRect.contains(currentLocation);
}

bool AutoplayExperimentHelper::maybeStartPlaying()
{
    // See if we're allowed to autoplay now.
    if (!isGestureRequirementOverridden())
        return false;

    // Start playing!
    prepareToAutoplay(client().shouldAutoplay()
        ? GesturelessPlaybackStartedByAutoplayFlagAfterScroll
        : GesturelessPlaybackStartedByPlayMethodAfterScroll);

    // Record that this played without a user gesture.
    // This should rarely actually do anything.  Usually, playMethodCalled()
    // and becameReadyToPlay will handle it, but toggling muted state can,
    // in some cases, also trigger autoplay if the autoplay attribute is set
    // after the media is ready to play.
    autoplayMediaEncountered();

    client().playInternal();

    return true;
}

bool AutoplayExperimentHelper::isGestureRequirementOverridden() const
{
    return isEligible() && meetsVisibilityRequirements();
}

bool AutoplayExperimentHelper::isPlaybackDeferred() const
{
    return m_playPending;
}

bool AutoplayExperimentHelper::isEligible(EligibilityMode mode) const
{
    if (m_mode == Mode::ExperimentOff)
        return false;

    // If autoplay is disabled, no one is eligible.
    if (!client().isAutoplayAllowedPerSettings())
        return false;

    // If no user gesture is required, then the experiment doesn't apply.
    // This is what prevents us from starting playback more than once.
    // Since this flag is never set to true once it's cleared, it will block
    // the autoplay experiment forever.
    if (!isLockedPendingUserGesture())
        return false;

    // Make sure that this is an element of the right type.
    if (!enabled(ForVideo) && client().isHTMLVideoElement())
        return false;

    if (!enabled(ForAudio) && client().isHTMLAudioElement())
        return false;

    // If nobody has requested playback, either by the autoplay attribute or
    // a play() call, then do nothing.

    if (mode !=  IgnorePendingPlayback && !m_playPending && !client().shouldAutoplay())
        return false;

    // Note that the viewport test always returns false on desktop, which is
    // why video-autoplay-experiment.html doesn't check -ifmobile .
    if (enabled(IfMobile)
        && !client().isLegacyViewportType())
        return false;

    // If we require same-origin, then check the origin.
    if (enabled(IfSameOrigin) && client().isCrossOrigin()) {
        // We're cross-origin, so block unless it's muted content and OrMuted
        // is enabled.  For good measure, we also block all audio elements.
        if (client().isHTMLAudioElement() || !client().muted()
            || !enabled(OrMuted)) {
            return false;
        }
    }

    // If we require muted media and this is muted, then it is eligible.
    if (enabled(IfMuted))
        return client().muted();

    // Element is eligible for gesture override, maybe muted.
    return true;
}

void AutoplayExperimentHelper::muteIfNeeded()
{
    if (enabled(PlayMuted))
        client().setMuted(true);
}

void AutoplayExperimentHelper::unlockUserGesture(AutoplayMetrics metric)
{
    // Note that this could be moved back into HTMLMediaElement fairly easily.
    // It's only here so that we can record the reason, and we can hide the
    // ordering between unlocking and recording from the element this way.
    if (!client().isLockedPendingUserGesture())
        return;

    setDeferredOverrideReason(metric);
    client().unlockUserGesture();
}

void AutoplayExperimentHelper::setDeferredOverrideReason(AutoplayMetrics metric)
{
    // If the player is unlocked, then we don't care about any later reason.
    if (!client().isLockedPendingUserGesture())
        return;

    m_autoplayDeferredMetric = metric;
}

void AutoplayExperimentHelper::prepareToAutoplay(AutoplayMetrics metric)
{
    // This also causes !isEligible, so that we don't allow autoplay more than
    // once.  Be sure to do this before muteIfNeeded().
    // Also note that, at this point, we know that we're goint to start
    // playback.  However, we still don't record the metric here.  Instead,
    // we let playbackStarted() do that later.
    setDeferredOverrideReason(metric);

    // Don't bother to call autoplayMediaEncountered, since whoever initiates
    // playback has do it anyway, in case we don't allow autoplay.

    unregisterForPositionUpdatesIfNeeded();
    muteIfNeeded();

    // Do not actually start playback here.
}

AutoplayExperimentHelper::Mode AutoplayExperimentHelper::fromString(const String& mode)
{
    Mode value = ExperimentOff;
    if (mode.contains("-forvideo"))
        value |= ForVideo;
    if (mode.contains("-foraudio"))
        value |= ForAudio;
    if (mode.contains("-ifpagevisible"))
        value |= IfPageVisible;
    if (mode.contains("-ifviewport"))
        value |= IfViewport;
    if (mode.contains("-ifpartialviewport"))
        value |= IfPartialViewport;
    if (mode.contains("-ifmuted"))
        value |= IfMuted;
    if (mode.contains("-ifmobile"))
        value |= IfMobile;
    if (mode.contains("-ifsameorigin"))
        value |= IfSameOrigin;
    if (mode.contains("-ormuted"))
        value |= OrMuted;
    if (mode.contains("-playmuted"))
        value |= PlayMuted;

    return value;
}

void AutoplayExperimentHelper::autoplayMediaEncountered()
{
    if (!m_autoplayMediaEncountered) {
        m_autoplayMediaEncountered = true;
        recordAutoplayMetric(AutoplayMediaFound);
    }
}

bool AutoplayExperimentHelper::isLockedPendingUserGesture() const
{
    return client().isLockedPendingUserGesture();
}

void AutoplayExperimentHelper::playbackStarted()
{
    recordAutoplayMetric(AnyPlaybackStarted);

    // Forget about our most recent visibility check.  If another override is
    // requested, then we'll have to refresh it.  That way, we don't need to
    // keep it up to date in the interim.
    m_lastVisibleRect = IntRect();
    m_wasInViewport = false;

    // Any pending play is now playing.
    m_playPending = false;

    if (m_playbackStartedMetricRecorded)
        return;

    // Whether we record anything or not, we only want to record metrics for
    // the initial playback.
    m_playbackStartedMetricRecorded = true;

    // If this is a gestureless start, then record why it was allowed.
    if (m_autoplayMediaEncountered) {
        m_waitingForAutoplayPlaybackStop = true;
        recordAutoplayMetric(m_autoplayDeferredMetric);
    }
}

void AutoplayExperimentHelper::playbackStopped()
{
    const bool ended = client().ended();
    const bool bailout = isBailout();

    // Record that play was paused.  We don't care if it was autoplay,
    // play(), or the user manually started it.
    recordAutoplayMetric(ended ? AnyPlaybackComplete : AnyPlaybackPaused);
    if (bailout)
        recordAutoplayMetric(AnyPlaybackBailout);

    // If this was a gestureless play, then record that separately.
    // These cover attr and play() gestureless starts.
    if (m_waitingForAutoplayPlaybackStop) {
        m_waitingForAutoplayPlaybackStop = false;

        recordAutoplayMetric(ended ? AutoplayComplete : AutoplayPaused);

        if (bailout)
            recordAutoplayMetric(AutoplayBailout);
    }
}

void AutoplayExperimentHelper::recordAutoplayMetric(AutoplayMetrics metric)
{
    client().recordAutoplayMetric(metric);
}

bool AutoplayExperimentHelper::isBailout() const
{
    // We count the user as having bailed-out on the video if they watched
    // less than one minute and less than 50% of it.
    const double playedTime = client().currentTime();
    const double progress = playedTime / client().duration();
    return (playedTime < 60) && (progress < 0.5);
}

void AutoplayExperimentHelper::recordSandboxFailure()
{
    // We record autoplayMediaEncountered here because we know
    // that the autoplay attempt will fail.
    autoplayMediaEncountered();
    recordAutoplayMetric(AutoplayDisabledBySandbox);
}

void AutoplayExperimentHelper::loadingStarted()
{
    if (m_recordedElement)
        return;

    m_recordedElement = true;
    recordAutoplayMetric(client().isHTMLVideoElement()
        ? AnyVideoElement
        : AnyAudioElement);
}

bool AutoplayExperimentHelper::requiresViewportVisibility() const
{
    return client().isHTMLVideoElement() && (enabled(IfViewport) || enabled(IfPartialViewport));
}

bool AutoplayExperimentHelper::isExperimentEnabled()
{
    return m_mode != Mode::ExperimentOff;
}

} // namespace blink
