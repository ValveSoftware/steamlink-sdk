/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/MediaControls.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Fullscreen.h"
#include "core/events/MouseEvent.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/layout/LayoutTheme.h"
#include "platform/EventDispatchForbiddenScope.h"

namespace blink {

// If you change this value, then also update the corresponding value in
// LayoutTests/media/media-controls.js.
static const double timeWithoutMouseMovementBeforeHidingMediaControls = 3;

static bool shouldShowFullscreenButton(const HTMLMediaElement& mediaElement)
{
    // Unconditionally allow the user to exit fullscreen if we are in it
    // now.  Especially on android, when we might not yet know if
    // fullscreen is supported, we sometimes guess incorrectly and show
    // the button earlier, and we don't want to remove it here if the
    // user chose to enter fullscreen.  crbug.com/500732 .
    if (mediaElement.isFullscreen())
        return true;

    if (!mediaElement.isHTMLVideoElement())
        return false;

    if (!mediaElement.hasVideo())
        return false;

    if (!Fullscreen::fullscreenEnabled(mediaElement.document()))
        return false;

    return true;
}

static bool shouldShowCastButton(HTMLMediaElement& mediaElement)
{
    return !mediaElement.fastHasAttribute(HTMLNames::disableremoteplaybackAttr) && mediaElement.hasRemoteRoutes();
}

static bool preferHiddenVolumeControls(const Document& document)
{
    return !document.settings() || document.settings()->preferHiddenVolumeControls();
}

class MediaControls::BatchedControlUpdate {
    WTF_MAKE_NONCOPYABLE(BatchedControlUpdate);
    STACK_ALLOCATED();
public:
    explicit BatchedControlUpdate(MediaControls* controls)
        : m_controls(controls)
    {
        DCHECK(isMainThread());
        DCHECK_GE(s_batchDepth, 0);
        ++s_batchDepth;
    }
    ~BatchedControlUpdate()
    {
        DCHECK(isMainThread());
        DCHECK_GT(s_batchDepth, 0);
        if (!(--s_batchDepth))
            m_controls->computeWhichControlsFit();
    }

private:
    Member<MediaControls> m_controls;
    static int s_batchDepth;
};

// Count of number open batches for controls visibility.
int MediaControls::BatchedControlUpdate::s_batchDepth = 0;

MediaControls::MediaControls(HTMLMediaElement& mediaElement)
    : HTMLDivElement(mediaElement.document())
    , m_mediaElement(&mediaElement)
    , m_overlayEnclosure(nullptr)
    , m_overlayPlayButton(nullptr)
    , m_overlayCastButton(nullptr)
    , m_enclosure(nullptr)
    , m_panel(nullptr)
    , m_playButton(nullptr)
    , m_timeline(nullptr)
    , m_currentTimeDisplay(nullptr)
    , m_durationDisplay(nullptr)
    , m_muteButton(nullptr)
    , m_volumeSlider(nullptr)
    , m_toggleClosedCaptionsButton(nullptr)
    , m_textTrackList(nullptr)
    , m_castButton(nullptr)
    , m_fullScreenButton(nullptr)
    , m_hideMediaControlsTimer(this, &MediaControls::hideMediaControlsTimerFired)
    , m_hideTimerBehaviorFlags(IgnoreNone)
    , m_isMouseOverControls(false)
    , m_isPausedForScrubbing(false)
    , m_panelWidthChangedTimer(this, &MediaControls::panelWidthChangedTimerFired)
    , m_panelWidth(0)
    , m_allowHiddenVolumeControls(RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
{
}

MediaControls* MediaControls::create(HTMLMediaElement& mediaElement)
{
    MediaControls* controls = new MediaControls(mediaElement);
    controls->setShadowPseudoId(AtomicString("-webkit-media-controls"));
    controls->initializeControls();
    return controls;
}

// The media controls DOM structure looks like:
//
// MediaControls                                       (-webkit-media-controls)
// +-MediaControlOverlayEnclosureElement               (-webkit-media-controls-overlay-enclosure)
// | +-MediaControlOverlayPlayButtonElement            (-webkit-media-controls-overlay-play-button)
// | | {if mediaControlsOverlayPlayButtonEnabled}
// | \-MediaControlCastButtonElement                   (-internal-media-controls-overlay-cast-button)
// \-MediaControlPanelEnclosureElement                 (-webkit-media-controls-enclosure)
//   \-MediaControlPanelElement                        (-webkit-media-controls-panel)
//     +-MediaControlPlayButtonElement                 (-webkit-media-controls-play-button)
//     | {if !RTE::newMediaPlaybackUi()}
//     +-MediaControlTimelineElement                   (-webkit-media-controls-timeline)
//     +-MediaControlCurrentTimeDisplayElement         (-webkit-media-controls-current-time-display)
//     +-MediaControlTimeRemainingDisplayElement       (-webkit-media-controls-time-remaining-display)
//     | {if RTE::newMediaPlaybackUi()}
//     +-MediaControlTimelineElement                   (-webkit-media-controls-timeline)
//     +-MediaControlMuteButtonElement                 (-webkit-media-controls-mute-button)
//     +-MediaControlVolumeSliderElement               (-webkit-media-controls-volume-slider)
//     +-MediaControlToggleClosedCaptionsButtonElement (-webkit-media-controls-toggle-closed-captions-button)
//     +-MediaControlCastButtonElement                 (-internal-media-controls-cast-button)
//     \-MediaControlFullscreenButtonElement           (-webkit-media-controls-fullscreen-button)
// +-MediaControlTextTrackListElement                (-internal-media-controls-text-track-list)
// | {for each renderable text track}
//  \-MediaControlTextTrackListItem                 (-internal-media-controls-text-track-list-item)
//  +-MediaControlTextTrackListItemInput            (-internal-media-controls-text-track-list-item-input)
//  +-MediaControlTextTrackListItemCaptions         (-internal-media-controls-text-track-list-kind-captions)
//  +-MediaControlTextTrackListItemSubtitles        (-internal-media-controls-text-track-list-kind-subtitles)
void MediaControls::initializeControls()
{
    const bool useNewUi = RuntimeEnabledFeatures::newMediaPlaybackUiEnabled();
    MediaControlOverlayEnclosureElement* overlayEnclosure = MediaControlOverlayEnclosureElement::create(*this);

    if (document().settings() && document().settings()->mediaControlsOverlayPlayButtonEnabled()) {
        MediaControlOverlayPlayButtonElement* overlayPlayButton = MediaControlOverlayPlayButtonElement::create(*this);
        m_overlayPlayButton = overlayPlayButton;
        overlayEnclosure->appendChild(overlayPlayButton);
    }

    MediaControlCastButtonElement* overlayCastButton = MediaControlCastButtonElement::create(*this, true);
    m_overlayCastButton = overlayCastButton;
    overlayEnclosure->appendChild(overlayCastButton);

    m_overlayEnclosure = overlayEnclosure;
    appendChild(overlayEnclosure);

    // Create an enclosing element for the panel so we can visually offset the controls correctly.
    MediaControlPanelEnclosureElement* enclosure = MediaControlPanelEnclosureElement::create(*this);

    MediaControlPanelElement* panel = MediaControlPanelElement::create(*this);

    MediaControlPlayButtonElement* playButton = MediaControlPlayButtonElement::create(*this);
    m_playButton = playButton;
    panel->appendChild(playButton);

    MediaControlTimelineElement* timeline = MediaControlTimelineElement::create(*this);
    m_timeline = timeline;
    // In old UX, timeline is before the time / duration text.
    if (!useNewUi)
        panel->appendChild(timeline);
    // else we will attach it later.

    MediaControlCurrentTimeDisplayElement* currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(*this);
    m_currentTimeDisplay = currentTimeDisplay;
    m_currentTimeDisplay->setIsWanted(useNewUi);
    panel->appendChild(currentTimeDisplay);

    MediaControlTimeRemainingDisplayElement* durationDisplay = MediaControlTimeRemainingDisplayElement::create(*this);
    m_durationDisplay = durationDisplay;
    panel->appendChild(durationDisplay);

    // Timeline is after the time / duration text if newMediaPlaybackUiEnabled.
    if (useNewUi)
        panel->appendChild(timeline);

    MediaControlMuteButtonElement* muteButton = MediaControlMuteButtonElement::create(*this);
    m_muteButton = muteButton;
    panel->appendChild(muteButton);

    MediaControlVolumeSliderElement* slider = MediaControlVolumeSliderElement::create(*this);
    m_volumeSlider = slider;
    panel->appendChild(slider);
    if (m_allowHiddenVolumeControls && preferHiddenVolumeControls(document()))
        m_volumeSlider->setIsWanted(false);

    MediaControlToggleClosedCaptionsButtonElement* toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(*this);
    m_toggleClosedCaptionsButton = toggleClosedCaptionsButton;
    panel->appendChild(toggleClosedCaptionsButton);

    MediaControlCastButtonElement* castButton = MediaControlCastButtonElement::create(*this, false);
    m_castButton = castButton;
    panel->appendChild(castButton);

    MediaControlFullscreenButtonElement* fullscreenButton = MediaControlFullscreenButtonElement::create(*this);
    m_fullScreenButton = fullscreenButton;
    panel->appendChild(fullscreenButton);

    m_panel = panel;
    enclosure->appendChild(panel);

    m_enclosure = enclosure;
    appendChild(enclosure);

    MediaControlTextTrackListElement* textTrackList = MediaControlTextTrackListElement::create(*this);
    m_textTrackList = textTrackList;
    appendChild(textTrackList);
}

void MediaControls::reset()
{
    EventDispatchForbiddenScope::AllowUserAgentEvents allowEventsInShadow;
    const bool useNewUi = RuntimeEnabledFeatures::newMediaPlaybackUiEnabled();
    BatchedControlUpdate batch(this);

    m_allowHiddenVolumeControls = useNewUi;

    const double duration = mediaElement().duration();
    m_durationDisplay->setTextContent(LayoutTheme::theme().formatMediaControlsTime(duration));
    m_durationDisplay->setCurrentValue(duration);

    if (useNewUi) {
        // Show everything that we might hide.
        // If we don't have a duration, then mark it to be hidden.  For the
        // old UI case, want / don't want is the same as show / hide since
        // it is never marked as not fitting.
        m_durationDisplay->setIsWanted(std::isfinite(duration));
        m_currentTimeDisplay->setIsWanted(true);
        m_timeline->setIsWanted(true);
    }

    // If the player has entered an error state, force it into the paused state.
    if (mediaElement().error())
        mediaElement().pause();

    updatePlayState();

    updateCurrentTimeDisplay();

    m_timeline->setDuration(duration);
    m_timeline->setPosition(mediaElement().currentTime());

    updateVolume();

    refreshClosedCaptionsButtonVisibility();

    m_fullScreenButton->setIsWanted(shouldShowFullscreenButton(mediaElement()));

    refreshCastButtonVisibilityWithoutUpdate();
}

LayoutObject* MediaControls::layoutObjectForTextTrackLayout()
{
    return m_panel->layoutObject();
}

void MediaControls::show()
{
    makeOpaque();
    m_panel->setIsWanted(true);
    m_panel->setIsDisplayed(true);
    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
}

void MediaControls::mediaElementFocused()
{
    if (mediaElement().shouldShowControls()) {
        show();
        resetHideMediaControlsTimer();
    }
}

void MediaControls::hide()
{
    m_panel->setIsWanted(false);
    m_panel->setIsDisplayed(false);
    if (m_overlayPlayButton)
        m_overlayPlayButton->setIsWanted(false);
}

void MediaControls::makeOpaque()
{
    m_panel->makeOpaque();
}

void MediaControls::makeTransparent()
{
    m_panel->makeTransparent();
}

bool MediaControls::shouldHideMediaControls(unsigned behaviorFlags) const
{
    // Never hide for a media element without visual representation.
    if (!mediaElement().isHTMLVideoElement() || !mediaElement().hasVideo() || mediaElement().isPlayingRemotely())
        return false;
    // Don't hide if the mouse is over the controls.
    const bool ignoreControlsHover = behaviorFlags & IgnoreControlsHover;
    if (!ignoreControlsHover && m_panel->hovered())
        return false;
    // Don't hide if the mouse is over the video area.
    const bool ignoreVideoHover = behaviorFlags & IgnoreVideoHover;
    if (!ignoreVideoHover && m_isMouseOverControls)
        return false;
    // Don't hide if focus is on the HTMLMediaElement or within the
    // controls/shadow tree. (Perform the checks separately to avoid going
    // through all the potential ancestor hosts for the focused element.)
    const bool ignoreFocus = behaviorFlags & IgnoreFocus;
    if (!ignoreFocus && (mediaElement().focused() || contains(document().focusedElement())))
        return false;
    // Don't hide the media controls when the text track list is showing.
    if (m_textTrackList->isWanted())
        return false;
    return true;
}

void MediaControls::playbackStarted()
{
    BatchedControlUpdate batch(this);

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled()) {
        m_currentTimeDisplay->setIsWanted(true);
        m_durationDisplay->setIsWanted(false);
    }

    updatePlayState();
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();

    startHideMediaControlsTimer();
}

void MediaControls::playbackProgressed()
{
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();

    if (shouldHideMediaControls())
        makeTransparent();
}

void MediaControls::playbackStopped()
{
    updatePlayState();
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();
    makeOpaque();

    stopHideMediaControlsTimer();
}

void MediaControls::updatePlayState()
{
    if (m_isPausedForScrubbing)
        return;

    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
    m_playButton->updateDisplayType();
}

void MediaControls::beginScrubbing()
{
    if (!mediaElement().paused()) {
        m_isPausedForScrubbing = true;
        mediaElement().pause();
    }
}

void MediaControls::endScrubbing()
{
    if (m_isPausedForScrubbing) {
        m_isPausedForScrubbing = false;
        if (mediaElement().paused())
            mediaElement().play();
    }
}

void MediaControls::updateCurrentTimeDisplay()
{
    double now = mediaElement().currentTime();
    double duration = mediaElement().duration();

    // After seek, hide duration display and show current time.
    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled() && now > 0) {
        BatchedControlUpdate batch(this);
        m_currentTimeDisplay->setIsWanted(true);
        m_durationDisplay->setIsWanted(false);
    }

    // Allow the theme to format the time.
    m_currentTimeDisplay->setInnerText(LayoutTheme::theme().formatMediaControlsCurrentTime(now, duration), IGNORE_EXCEPTION);
    m_currentTimeDisplay->setCurrentValue(now);
}

void MediaControls::updateVolume()
{
    m_muteButton->updateDisplayType();
    // Invalidate the mute button because it paints differently according to volume.
    invalidate(m_muteButton);

    if (mediaElement().muted())
        m_volumeSlider->setVolume(0);
    else
        m_volumeSlider->setVolume(mediaElement().volume());

    // Update the visibility of our audio elements.
    // We never want the volume slider if there's no audio.
    // If there is audio, then we want it unless hiding audio is enabled and
    // we prefer to hide it.
    BatchedControlUpdate batch(this);
    m_volumeSlider->setIsWanted(mediaElement().hasAudio()
        && !(m_allowHiddenVolumeControls && preferHiddenVolumeControls(document())));

    // The mute button is a little more complicated.  If enableNewMediaPlaybackUi
    // is true, then we choose to hide or show the mute button to save space.
    // If enableNew* is not set, then we never touch the mute button, and
    // instead leave it to the CSS.
    // Note that this is why m_allowHiddenVolumeControls isn't rolled into prefer...().
    if (m_allowHiddenVolumeControls) {
        // If there is no audio track, then hide the mute button.
        m_muteButton->setIsWanted(mediaElement().hasAudio());
    }

    // Invalidate the volume slider because it paints differently according to volume.
    invalidate(m_volumeSlider);
}

void MediaControls::changedClosedCaptionsVisibility()
{
    m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControls::refreshClosedCaptionsButtonVisibility()
{
    m_toggleClosedCaptionsButton->setIsWanted(mediaElement().hasClosedCaptions());
    BatchedControlUpdate batch(this);
}

void MediaControls::toggleTextTrackList()
{
    if (!mediaElement().hasClosedCaptions()) {
        m_textTrackList->setVisible(false);
        return;
    }

    m_textTrackList->setVisible(!m_textTrackList->isWanted());
}

void MediaControls::refreshCastButtonVisibility()
{
    refreshCastButtonVisibilityWithoutUpdate();
    BatchedControlUpdate batch(this);
}

void MediaControls::refreshCastButtonVisibilityWithoutUpdate()
{
    if (!shouldShowCastButton(mediaElement())) {
        m_castButton->setIsWanted(false);
        m_overlayCastButton->setIsWanted(false);
        return;
    }

    // The reason for the autoplay test is that some pages (e.g. vimeo.com) have an autoplay background video, which
    // doesn't autoplay on Chrome for Android (we prevent it) so starts paused. In such cases we don't want to automatically
    // show the cast button, since it looks strange and is unlikely to correspond with anything the user wants to do.
    // If a user does want to cast a paused autoplay video then they can still do so by touching or clicking on the
    // video, which will cause the cast button to appear.
    if (!mediaElement().shouldShowControls() && !mediaElement().autoplay() && mediaElement().paused()) {
        // Note that this is a case where we add the overlay cast button
        // without wanting the panel cast button.  We depend on the fact
        // that computeWhichControlsFit() won't change overlay cast button
        // visibility in the case where the cast button isn't wanted.
        // We don't call compute...() here, but it will be called as
        // non-cast changes (e.g., resize) occur.  If the panel button
        // is shown, however, compute...() will take control of the
        // overlay cast button if it needs to hide it from the panel.
        m_overlayCastButton->tryShowOverlay();
        m_castButton->setIsWanted(false);
    } else if (mediaElement().shouldShowControls()) {
        m_overlayCastButton->setIsWanted(false);
        m_castButton->setIsWanted(true);
        // Check that the cast button actually fits on the bar.  For the
        // newMediaPlaybackUiEnabled case, we let computeWhichControlsFit()
        // handle this.
        if ( !RuntimeEnabledFeatures::newMediaPlaybackUiEnabled()
            && m_fullScreenButton->getBoundingClientRect()->right() > m_panel->getBoundingClientRect()->right()) {
            m_castButton->setIsWanted(false);
            m_overlayCastButton->tryShowOverlay();
        }
    }
}

void MediaControls::showOverlayCastButtonIfNeeded()
{
    if (mediaElement().shouldShowControls() || !shouldShowCastButton(mediaElement()))
        return;

    m_overlayCastButton->tryShowOverlay();
    resetHideMediaControlsTimer();
}

void MediaControls::enteredFullscreen()
{
    m_fullScreenButton->setIsFullscreen(true);
    stopHideMediaControlsTimer();
    startHideMediaControlsTimer();
}

void MediaControls::exitedFullscreen()
{
    m_fullScreenButton->setIsFullscreen(false);
    stopHideMediaControlsTimer();
    startHideMediaControlsTimer();
}

void MediaControls::startedCasting()
{
    m_castButton->setIsPlayingRemotely(true);
    m_overlayCastButton->setIsPlayingRemotely(true);
}

void MediaControls::stoppedCasting()
{
    m_castButton->setIsPlayingRemotely(false);
    m_overlayCastButton->setIsPlayingRemotely(false);
}

void MediaControls::defaultEventHandler(Event* event)
{
    HTMLDivElement::defaultEventHandler(event);

    // Add IgnoreControlsHover to m_hideTimerBehaviorFlags when we see a touch event,
    // to allow the hide-timer to do the right thing when it fires.
    // FIXME: Preferably we would only do this when we're actually handling the event
    // here ourselves.
    bool wasLastEventTouch = event->isTouchEvent() || event->isGestureEvent()
        || (event->isMouseEvent() && toMouseEvent(event)->fromTouch());
    m_hideTimerBehaviorFlags |= wasLastEventTouch ? IgnoreControlsHover : IgnoreNone;

    if (event->type() == EventTypeNames::mouseover) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = true;
            if (!mediaElement().paused()) {
                makeOpaque();
                if (shouldHideMediaControls())
                    startHideMediaControlsTimer();
            }
        }
        return;
    }

    if (event->type() == EventTypeNames::mouseout) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = false;
            stopHideMediaControlsTimer();
        }
        return;
    }

    if (event->type() == EventTypeNames::mousemove) {
        // When we get a mouse move, show the media controls, and start a timer
        // that will hide the media controls after a 3 seconds without a mouse move.
        makeOpaque();
        refreshCastButtonVisibility();
        if (shouldHideMediaControls(IgnoreVideoHover))
            startHideMediaControlsTimer();
        return;
    }
}

void MediaControls::hideMediaControlsTimerFired(Timer<MediaControls>*)
{
    unsigned behaviorFlags = m_hideTimerBehaviorFlags | IgnoreFocus | IgnoreVideoHover;
    m_hideTimerBehaviorFlags = IgnoreNone;

    if (mediaElement().paused())
        return;

    if (!shouldHideMediaControls(behaviorFlags))
        return;

    makeTransparent();
    m_overlayCastButton->setIsWanted(false);
}

void MediaControls::startHideMediaControlsTimer()
{
    m_hideMediaControlsTimer.startOneShot(timeWithoutMouseMovementBeforeHidingMediaControls, BLINK_FROM_HERE);
}

void MediaControls::stopHideMediaControlsTimer()
{
    m_hideMediaControlsTimer.stop();
}

void MediaControls::resetHideMediaControlsTimer()
{
    stopHideMediaControlsTimer();
    if (!mediaElement().paused())
        startHideMediaControlsTimer();
}


bool MediaControls::containsRelatedTarget(Event* event)
{
    if (!event->isMouseEvent())
        return false;
    EventTarget* relatedTarget = toMouseEvent(event)->relatedTarget();
    if (!relatedTarget)
        return false;
    return contains(relatedTarget->toNode());
}

void MediaControls::notifyPanelWidthChanged(const LayoutUnit& newWidth)
{
    // Don't bother to do any work if this matches the most recent panel
    // width, since we're called after layout.
    // Note that this code permits a bad frame on resize, since it is
    // run after the relayout / paint happens.  It would be great to improve
    // this, but it would be even greater to move this code entirely to
    // JS and fix it there.
    const int panelWidth = newWidth.toInt();

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
        return;

    m_panelWidth = panelWidth;

    // Adjust for effective zoom.
    if (!m_panel->layoutObject() || !m_panel->layoutObject()->style())
        return;
    m_panelWidth = ceil(m_panelWidth / m_panel->layoutObject()->style()->effectiveZoom());

    m_panelWidthChangedTimer.startOneShot(0, BLINK_FROM_HERE);
}

void MediaControls::panelWidthChangedTimerFired(Timer<MediaControls>*)
{
    computeWhichControlsFit();
}

void MediaControls::computeWhichControlsFit()
{
    // Hide all controls that don't fit, and show the ones that do.
    // This might be better suited for a layout, but since JS media controls
    // won't benefit from that anwyay, we just do it here like JS will.

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
        return;

    // Controls that we'll hide / show, in order of decreasing priority.
    MediaControlElement* elements[] = {
        // Exclude m_playButton; we handle it specially.
        m_toggleClosedCaptionsButton.get(),
        m_fullScreenButton.get(),
        m_timeline.get(),
        m_currentTimeDisplay.get(),
        m_volumeSlider.get(),
        m_castButton.get(),
        m_muteButton.get(),
        m_durationDisplay.get(),
    };

    int usedWidth = 0;

    // Assume that all controls require 48px, unless we can get the computed
    // style for the play button.  Since the play button is always shown, it
    // should be available the first time we're called after layout.  This will
    // also be the first time we have m_panelWidth!=0, so it won't matter if
    // we get this wrong before that.
    int minimumWidth = 48;
    if (m_playButton->layoutObject() && m_playButton->layoutObject()->style()) {
        const ComputedStyle* style = m_playButton->layoutObject()->style();
        minimumWidth = ceil(style->width().pixels() / style->effectiveZoom());
    }

    // Special-case the play button; it always fits.
    if (m_playButton->isWanted()) {
        m_playButton->setDoesFit(true);
        usedWidth += minimumWidth;
    }

    if (!m_panelWidth) {
        // No layout yet -- hide everything, then make them show up later.
        // This prevents the wrong controls from being shown briefly
        // immediately after the first layout and paint, but before we have
        // a chance to revise them.
        for (MediaControlElement* element : elements) {
            if (element)
                element->setDoesFit(false);
        }
        return;
    }

    // For each control that fits, enable it in order of decreasing priority.
    bool droppedCastButton = false;
    for (MediaControlElement* element : elements) {
        if (!element)
            continue;

        if (element->isWanted()) {
            if (usedWidth + minimumWidth <= m_panelWidth) {
                element->setDoesFit(true);
                usedWidth += minimumWidth;
            } else {
                element->setDoesFit(false);
                if (element == m_castButton.get())
                    droppedCastButton = true;
            }
        }
    }

    // Special case for cast: if we want a cast button but dropped it, then
    // show the overlay cast button instead.
    if (m_castButton->isWanted()) {
        if (droppedCastButton)
            m_overlayCastButton->tryShowOverlay();
        else
            m_overlayCastButton->setIsWanted(false);
    }
}

void MediaControls::setAllowHiddenVolumeControls(bool allow)
{
    m_allowHiddenVolumeControls = allow;
    // Update the controls visibility.
    updateVolume();
}

void MediaControls::invalidate(Element* element)
{
    if (!element)
        return;

    if (LayoutObject* layoutObject = element->layoutObject())
        layoutObject->setShouldDoFullPaintInvalidation();
}

void MediaControls::networkStateChanged()
{
    invalidate(m_playButton);
    invalidate(m_overlayPlayButton);
    invalidate(m_muteButton);
    invalidate(m_fullScreenButton);
    invalidate(m_timeline);
    invalidate(m_volumeSlider);
}

DEFINE_TRACE(MediaControls)
{
    visitor->trace(m_mediaElement);
    visitor->trace(m_panel);
    visitor->trace(m_overlayPlayButton);
    visitor->trace(m_overlayEnclosure);
    visitor->trace(m_playButton);
    visitor->trace(m_currentTimeDisplay);
    visitor->trace(m_timeline);
    visitor->trace(m_muteButton);
    visitor->trace(m_volumeSlider);
    visitor->trace(m_toggleClosedCaptionsButton);
    visitor->trace(m_fullScreenButton);
    visitor->trace(m_durationDisplay);
    visitor->trace(m_enclosure);
    visitor->trace(m_textTrackList);
    visitor->trace(m_castButton);
    visitor->trace(m_overlayCastButton);
    HTMLDivElement::trace(visitor);
}

} // namespace blink
