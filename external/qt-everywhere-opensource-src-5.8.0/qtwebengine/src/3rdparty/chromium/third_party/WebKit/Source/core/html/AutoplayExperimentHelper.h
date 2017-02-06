// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayExperimentHelper_h
#define AutoplayExperimentHelper_h

#include "core/page/PageVisibilityState.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"

namespace blink {

class Document;
class AutoplayExperimentTest;

// These values are used for a histogram. Do not reorder.
enum AutoplayMetrics {
    // Media element with autoplay seen.
    AutoplayMediaFound = 0,
    // Autoplay enabled and user stopped media play at any point.
    AutoplayPaused = 1,
    // Autoplay enabled but user bailed out on media play early.
    AutoplayBailout = 2,
    // Autoplay disabled but user manually started media.
    AutoplayManualStart = 3,
    // Autoplay was (re)enabled through a user-gesture triggered load()
    AutoplayEnabledThroughLoad = 4,
    // Autoplay disabled by sandbox flags.
    AutoplayDisabledBySandbox = 5,

    // These metrics indicate "no gesture detected, but the gesture
    // requirement was overridden by experiment".  They do not include cases
    // where no user gesture is required (!m_userGestureRequiredForPlay).

    // User gesture requirement was bypassed, and playback started, during
    // initial load via the autoplay flag.  If playback was deferred due to
    // visibility requirements, then this does not apply.
    GesturelessPlaybackStartedByAutoplayFlagImmediately = 6,

    // User gesture requirement was bypassed, and playback started, when
    // the play() method was called.  If playback is deferred due to
    // visibility requirements, then this does not apply.
    GesturelessPlaybackStartedByPlayMethodImmediately = 7,

    // User gesture requirement was bypassed, and playback started, after
    // an element with the autoplay flag moved into the viewport.  Playback
    // was deferred earlier due to visibility requirements.
    GesturelessPlaybackStartedByAutoplayFlagAfterScroll = 8,

    // User gesture requirement was bypassed, and playback started, after
    // an element on which the play() method was called was moved into the
    // viewport.  Playback had been deferred due to visibility requirements.
    GesturelessPlaybackStartedByPlayMethodAfterScroll = 9,

    // play() failed to play due to gesture requirement.
    PlayMethodFailed = 10,

    // Some play, whether user initiated or not, started.
    AnyPlaybackStarted = 11,
    // Some play, whether user initiated or not, paused.
    AnyPlaybackPaused = 12,
    // Some playback, whether user initiated or not, bailed out early.
    AnyPlaybackBailout = 13,
    // Some playback, whether user initiated or not, played to completion.
    AnyPlaybackComplete = 14,

    // Number of audio elements detected that reach the resource fetch algorithm.
    AnyAudioElement = 15,
    // Numer of video elements detected that reach the resource fetch algorithm.
    AnyVideoElement = 16,

    // User gesture was bypassed, and playback started, and media played to
    // completion without a user-initiated pause.
    AutoplayComplete = 17,

    // Autoplay started after the gesture requirement was removed by a
    // user gesture load().
    GesturelessPlaybackEnabledByLoad = 18,

    // Gestureless playback started after the gesture requirement was removed
    // because src is media stream.
    GesturelessPlaybackEnabledByStream = 19,

    // Gestureless playback was started, but was never overridden.  This
    // includes the case in which no gesture was ever required.
    GesturelessPlaybackNotOverridden = 20,

    // Gestureless playback was enabled by a user gesture play() call.
    GesturelessPlaybackEnabledByPlayMethod = 21,

    // This enum value must be last.
    NumberOfAutoplayMetrics,
};

class CORE_EXPORT AutoplayExperimentHelper final :
    public GarbageCollectedFinalized<AutoplayExperimentHelper> {
    friend class AutoplayExperimentTest;

public:
    // For easier testing, collect all the things we care about here.
    class Client : public GarbageCollectedFinalized<Client> {
    public:
        virtual ~Client() {}

        // HTMLMediaElement
        virtual double currentTime() const = 0;
        virtual double duration() const = 0;
        virtual bool paused() const = 0;
        virtual bool ended() const = 0;
        virtual bool muted() const = 0;
        virtual void setMuted(bool) = 0;
        virtual void playInternal() = 0;
        virtual void pauseInternal() = 0;
        virtual bool isLockedPendingUserGesture() const = 0;
        virtual void unlockUserGesture() = 0;
        virtual void recordAutoplayMetric(AutoplayMetrics) = 0;
        virtual bool shouldAutoplay() = 0;
        virtual bool isHTMLVideoElement() const = 0;
        virtual bool isHTMLAudioElement() const = 0;

        // Document
        virtual bool isLegacyViewportType() = 0;
        virtual PageVisibilityState pageVisibilityState() const = 0;
        virtual String autoplayExperimentMode() const = 0;

        // Frame
        virtual bool isCrossOrigin() const = 0;
        virtual bool isAutoplayAllowedPerSettings() const = 0;

        // LayoutObject
        virtual void setRequestPositionUpdates(bool) = 0;
        virtual IntRect absoluteBoundingBoxRect() const = 0;

        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    static AutoplayExperimentHelper* create(Client* client)
    {
        return new AutoplayExperimentHelper(client);
    }

    ~AutoplayExperimentHelper();

    void becameReadyToPlay();
    void playMethodCalled();
    void pauseMethodCalled();
    void loadMethodCalled();
    void mutedChanged();
    void positionChanged(const IntRect&);
    void updatePositionNotificationRegistration();
    void recordSandboxFailure();
    void loadingStarted();
    void playbackStarted();
    void playbackStopped();
    void initialPlayWithUserGesture();

    // Returns true if and only if any experiment is enabled (i.e., |m_mode|
    // is not ExperimentOff).
    bool isExperimentEnabled();

    // Remove the user gesture requirement, and record why.  If there is no
    // gesture requirement, then this does nothing.
    void unlockUserGesture(AutoplayMetrics);

    // Set the reason that we're overridding the user gesture.  If there is no
    // gesture requirement, then this does nothing.
    void setDeferredOverrideReason(AutoplayMetrics);

    // Return true if and only if the user gesture requirement is currently
    // overridden by the experiment, permitting playback.
    bool isGestureRequirementOverridden() const;

    // Return true if and only if playback is queued but hasn't started yet,
    // such as if the element doesn't meet visibility requirements.
    bool isPlaybackDeferred() const;

    // Set the position to the current view's position, and
    void triggerAutoplayViewportCheckForTesting();

    enum Mode {
        // Do not enable the autoplay experiment.
        ExperimentOff     = 0,
        // Enable gestureless autoplay for video elements.
        ForVideo          = 1 << 0,
        // Enable gestureless autoplay for audio elements.
        ForAudio          = 1 << 1,
        // Restrict gestureless autoplay to media that is in a visible page.
        IfPageVisible     = 1 << 2,
        // Restrict gestureless autoplay to media that is entirely visible in
        // the viewport.
        IfViewport        = 1 << 3,
        // Restrict gestureless autoplay to media that is partially visible in
        // the viewport.
        IfPartialViewport = 1 << 4,
        // Restrict gestureless autoplay to audio-less or muted media.
        IfMuted           = 1 << 5,
        // Restrict gestureless autoplay to sites which contain the
        // viewport tag.
        IfMobile          = 1 << 6,
        // Restrict gestureless autoplay to sites which are from the same origin
        // as the top-level frame.
        IfSameOrigin      = 1 << 7,
        // Extend IfSameOrigin to allow autoplay of cross-origin elements if
        // they're muted.  This has no effect on same-origin or if IfSameOrigin
        // isn't enabled.
        OrMuted           = 1 << 8,
        // If gestureless autoplay is allowed, then mute the media before
        // starting to play.
        PlayMuted         = 1 << 9,
    };

    DEFINE_INLINE_TRACE() { visitor->trace(m_client); }

private:
    explicit AutoplayExperimentHelper(Client*);

    // Register to receive position updates, if we haven't already.  If we
    // have, then this does nothing.
    void registerForPositionUpdatesIfNeeded();

    // Un-register for position updates, if we are currently registered.
    void unregisterForPositionUpdatesIfNeeded();

    // Modifiers for checking isEligible().
    enum EligibilityMode {
        // Perform all normal eligibility checks.
        Normal = 0,

        // Perform normal eligibility checks, but skip checking if autoplay has
        // actually been requested.  In other words, don't fail just becase
        // nobody has called play() and/or set the autoplay attribute.
        IgnorePendingPlayback = 1
    };

    // Return true if any only if this player meets (most) of the eligibility
    // requirements for the experiment to override the need for a user
    // gesture.  This includes everything except the visibility test.
    // |mode| modifies the eligibility check, as described above.
    bool isEligible(EligibilityMode = Normal) const;

    // Return false if and only if m_element is not visible, and we care
    // that it must be visible.
    bool meetsVisibilityRequirements() const;

    // Set the muted flag on the media if we're in an experiment mode that
    // requires it, else do nothing.
    void muteIfNeeded();

    // Maybe override the requirement for a user gesture, and start playing
    // autoplay media.  Returns true if only if it starts playback.
    bool maybeStartPlaying();

    // Configure internal state to record that the autoplay experiment is
    // going to start playback.  This doesn't actually start playback, since
    // there are several different cases.
    void prepareToAutoplay(AutoplayMetrics);

    // Record that an attempt to play without a user gesture has happened.
    // This does not assume whether or not the play attempt will succeed.
    // This method takes no action after it is called once.
    void autoplayMediaEncountered();

    // If we are about to enter a paused state, call this to record
    // autoplay metrics.
    void recordMetricsBeforePause();

    // Process a timer for checking visibility.
    void viewportTimerFired(Timer<AutoplayExperimentHelper>*);

    Client& client() const { return *m_client; }

    bool isLockedPendingUserGesture() const;

    inline bool enabled(Mode mode) const
    {
        return static_cast<int>(m_mode) & static_cast<int>(mode);
    }

    Mode fromString(const String& mode);

    void recordAutoplayMetric(AutoplayMetrics);

    // Could stopping at this point be considered a bailout of playback?
    // (as in, "The user really didn't want to play this").
    bool isBailout() const;

    // Returns true if and only if the experiment requires some sort of viewport
    // visibility check for autoplay.
    bool requiresViewportVisibility() const;

    Member<Client> m_client;

    Mode m_mode;

    // Autoplay experiment state.
    // True if we've received a play() without a pause().
    bool m_playPending : 1;

    // Are we registered with the view for position updates?
    bool m_registeredWithLayoutObject : 1;

    // According to our last position update, are we in the viewport?
    bool m_wasInViewport : 1;

    // Have we counted this autoplay media in the metrics yet?  This is set when
    // a media element tries to autoplay, and we record that via the
    // AutoplayMediaFound metric.
    bool m_autoplayMediaEncountered : 1;

    // Have we recorded a metric about the cause of the initial playback of
    // this media yet?
    bool m_playbackStartedMetricRecorded : 1;

    // Is the current playback the result of autoplay?  If so, then this flag
    // records that the pause / stop should be counted in the autoplay metrics.
    bool m_waitingForAutoplayPlaybackStop : 1;

    // Did we record that this media element exists in the metrics yet?  This is
    // independent of whether it autoplays; we just want to know how many
    // elements exist for the Any{Audio|Video}Element metrics.
    bool m_recordedElement : 1;

    // According to our last position update, where was our element?
    IntRect m_lastLocation;
    IntRect m_lastVisibleRect;

    // When was m_lastLocation set?
    double m_lastLocationUpdateTime;

    Timer<AutoplayExperimentHelper> m_viewportTimer;

    // Reason that autoplay would be allowed to proceed without a user gesture.
    AutoplayMetrics m_autoplayDeferredMetric;
};

inline AutoplayExperimentHelper::Mode& operator|=(AutoplayExperimentHelper::Mode& a,
    const AutoplayExperimentHelper::Mode& b)
{
    a = static_cast<AutoplayExperimentHelper::Mode>(static_cast<int>(a) | static_cast<int>(b));
    return a;
}

} // namespace blink

#endif // AutoplayExperimentHelper_h
