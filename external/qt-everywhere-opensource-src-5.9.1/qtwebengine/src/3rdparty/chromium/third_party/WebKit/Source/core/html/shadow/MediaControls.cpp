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
#include "core/html/shadow/MediaControlsMediaEventListener.h"
#include "core/html/shadow/MediaControlsWindowEventListener.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/html/track/TextTrackList.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "platform/EventDispatchForbiddenScope.h"

namespace blink {

// If you change this value, then also update the corresponding value in
// LayoutTests/media/media-controls.js.
static const double timeWithoutMouseMovementBeforeHidingMediaControls = 3;

static bool shouldShowFullscreenButton(const HTMLMediaElement& mediaElement) {
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

static bool shouldShowCastButton(HTMLMediaElement& mediaElement) {
  return !mediaElement.fastHasAttribute(HTMLNames::disableremoteplaybackAttr) &&
         mediaElement.hasRemoteRoutes();
}

static bool preferHiddenVolumeControls(const Document& document) {
  return !document.settings() ||
         document.settings()->preferHiddenVolumeControls();
}

class MediaControls::BatchedControlUpdate {
  WTF_MAKE_NONCOPYABLE(BatchedControlUpdate);
  STACK_ALLOCATED();

 public:
  explicit BatchedControlUpdate(MediaControls* controls)
      : m_controls(controls) {
    DCHECK(isMainThread());
    DCHECK_GE(s_batchDepth, 0);
    ++s_batchDepth;
  }
  ~BatchedControlUpdate() {
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
    : HTMLDivElement(mediaElement.document()),
      m_mediaElement(&mediaElement),
      m_overlayEnclosure(nullptr),
      m_overlayPlayButton(nullptr),
      m_overlayCastButton(nullptr),
      m_enclosure(nullptr),
      m_panel(nullptr),
      m_playButton(nullptr),
      m_timeline(nullptr),
      m_currentTimeDisplay(nullptr),
      m_durationDisplay(nullptr),
      m_muteButton(nullptr),
      m_volumeSlider(nullptr),
      m_toggleClosedCaptionsButton(nullptr),
      m_textTrackList(nullptr),
      m_overflowList(nullptr),
      m_castButton(nullptr),
      m_fullscreenButton(nullptr),
      m_downloadButton(nullptr),
      m_mediaEventListener(new MediaControlsMediaEventListener(this)),
      m_windowEventListener(MediaControlsWindowEventListener::create(
          this,
          WTF::bind(&MediaControls::hideAllMenus, wrapWeakPersistent(this)))),
      m_hideMediaControlsTimer(this,
                               &MediaControls::hideMediaControlsTimerFired),
      m_hideTimerBehaviorFlags(IgnoreNone),
      m_isMouseOverControls(false),
      m_isPausedForScrubbing(false),
      m_panelWidthChangedTimer(this,
                               &MediaControls::panelWidthChangedTimerFired),
      m_panelWidth(0),
      m_keepShowingUntilTimerFires(false) {}

MediaControls* MediaControls::create(HTMLMediaElement& mediaElement) {
  MediaControls* controls = new MediaControls(mediaElement);
  controls->setShadowPseudoId(AtomicString("-webkit-media-controls"));
  controls->initializeControls();
  return controls;
}

// The media controls DOM structure looks like:
//
// MediaControls
//     (-webkit-media-controls)
// +-MediaControlOverlayEnclosureElement
// |    (-webkit-media-controls-overlay-enclosure)
// | +-MediaControlOverlayPlayButtonElement
// | |    (-webkit-media-controls-overlay-play-button)
// | | {if mediaControlsOverlayPlayButtonEnabled}
// | \-MediaControlCastButtonElement
// |     (-internal-media-controls-overlay-cast-button)
// \-MediaControlPanelEnclosureElement
//   |    (-webkit-media-controls-enclosure)
//   \-MediaControlPanelElement
//     |    (-webkit-media-controls-panel)
//     +-MediaControlPlayButtonElement
//     |    (-webkit-media-controls-play-button)
//     +-MediaControlCurrentTimeDisplayElement
//     |    (-webkit-media-controls-current-time-display)
//     +-MediaControlTimeRemainingDisplayElement
//     |    (-webkit-media-controls-time-remaining-display)
//     +-MediaControlTimelineElement
//     |    (-webkit-media-controls-timeline)
//     +-MediaControlMuteButtonElement
//     |    (-webkit-media-controls-mute-button)
//     +-MediaControlVolumeSliderElement
//     |    (-webkit-media-controls-volume-slider)
//     +-MediaControlFullscreenButtonElement
//     |    (-webkit-media-controls-fullscreen-button)
//     +-MediaControlDownloadButtonElement
//     |    (-internal-media-controls-download-button)
//     +-MediaControlToggleClosedCaptionsButtonElement
//     |    (-webkit-media-controls-toggle-closed-captions-button)
//     \-MediaControlCastButtonElement
//         (-internal-media-controls-cast-button)
// +-MediaControlTextTrackListElement
// |    (-internal-media-controls-text-track-list)
// | {for each renderable text track}
//  \-MediaControlTextTrackListItem
//  |   (-internal-media-controls-text-track-list-item)
//  +-MediaControlTextTrackListItemInput
//  |    (-internal-media-controls-text-track-list-item-input)
//  +-MediaControlTextTrackListItemCaptions
//  |    (-internal-media-controls-text-track-list-kind-captions)
//  +-MediaControlTextTrackListItemSubtitles
//       (-internal-media-controls-text-track-list-kind-subtitles)
void MediaControls::initializeControls() {
  MediaControlOverlayEnclosureElement* overlayEnclosure =
      MediaControlOverlayEnclosureElement::create(*this);

  if (document().settings() &&
      document().settings()->mediaControlsOverlayPlayButtonEnabled()) {
    MediaControlOverlayPlayButtonElement* overlayPlayButton =
        MediaControlOverlayPlayButtonElement::create(*this);
    m_overlayPlayButton = overlayPlayButton;
    overlayEnclosure->appendChild(overlayPlayButton);
  }

  MediaControlCastButtonElement* overlayCastButton =
      MediaControlCastButtonElement::create(*this, true);
  m_overlayCastButton = overlayCastButton;
  overlayEnclosure->appendChild(overlayCastButton);

  m_overlayEnclosure = overlayEnclosure;
  appendChild(overlayEnclosure);

  // Create an enclosing element for the panel so we can visually offset the
  // controls correctly.
  MediaControlPanelEnclosureElement* enclosure =
      MediaControlPanelEnclosureElement::create(*this);

  MediaControlPanelElement* panel = MediaControlPanelElement::create(*this);

  MediaControlPlayButtonElement* playButton =
      MediaControlPlayButtonElement::create(*this);
  m_playButton = playButton;
  panel->appendChild(playButton);

  MediaControlCurrentTimeDisplayElement* currentTimeDisplay =
      MediaControlCurrentTimeDisplayElement::create(*this);
  m_currentTimeDisplay = currentTimeDisplay;
  m_currentTimeDisplay->setIsWanted(true);
  panel->appendChild(currentTimeDisplay);

  MediaControlTimeRemainingDisplayElement* durationDisplay =
      MediaControlTimeRemainingDisplayElement::create(*this);
  m_durationDisplay = durationDisplay;
  panel->appendChild(durationDisplay);

  MediaControlTimelineElement* timeline =
      MediaControlTimelineElement::create(*this);
  m_timeline = timeline;
  panel->appendChild(timeline);

  MediaControlMuteButtonElement* muteButton =
      MediaControlMuteButtonElement::create(*this);
  m_muteButton = muteButton;
  panel->appendChild(muteButton);

  MediaControlVolumeSliderElement* slider =
      MediaControlVolumeSliderElement::create(*this);
  m_volumeSlider = slider;
  panel->appendChild(slider);
  if (preferHiddenVolumeControls(document()))
    m_volumeSlider->setIsWanted(false);

  MediaControlFullscreenButtonElement* fullscreenButton =
      MediaControlFullscreenButtonElement::create(*this);
  m_fullscreenButton = fullscreenButton;
  panel->appendChild(fullscreenButton);

  MediaControlDownloadButtonElement* downloadButton =
      MediaControlDownloadButtonElement::create(*this);
  m_downloadButton = downloadButton;
  panel->appendChild(downloadButton);

  MediaControlCastButtonElement* castButton =
      MediaControlCastButtonElement::create(*this, false);
  m_castButton = castButton;
  panel->appendChild(castButton);

  MediaControlToggleClosedCaptionsButtonElement* toggleClosedCaptionsButton =
      MediaControlToggleClosedCaptionsButtonElement::create(*this);
  m_toggleClosedCaptionsButton = toggleClosedCaptionsButton;
  panel->appendChild(toggleClosedCaptionsButton);

  m_panel = panel;
  enclosure->appendChild(panel);

  m_enclosure = enclosure;
  appendChild(enclosure);

  MediaControlTextTrackListElement* textTrackList =
      MediaControlTextTrackListElement::create(*this);
  m_textTrackList = textTrackList;
  appendChild(textTrackList);

  MediaControlOverflowMenuButtonElement* overflowMenu =
      MediaControlOverflowMenuButtonElement::create(*this);
  m_overflowMenu = overflowMenu;
  panel->appendChild(overflowMenu);

  MediaControlOverflowMenuListElement* overflowList =
      MediaControlOverflowMenuListElement::create(*this);
  m_overflowList = overflowList;
  appendChild(overflowList);

  // The order in which we append elements to the overflow list is significant
  // because it determines how the elements show up in the overflow menu
  // relative to each other.  The first item appended appears at the top of the
  // overflow menu.
  m_overflowList->appendChild(m_playButton->createOverflowElement(
      *this, MediaControlPlayButtonElement::create(*this)));
  m_overflowList->appendChild(m_fullscreenButton->createOverflowElement(
      *this, MediaControlFullscreenButtonElement::create(*this)));
  m_overflowList->appendChild(m_downloadButton->createOverflowElement(
      *this, MediaControlDownloadButtonElement::create(*this)));
  m_overflowList->appendChild(m_muteButton->createOverflowElement(
      *this, MediaControlMuteButtonElement::create(*this)));
  m_overflowList->appendChild(m_castButton->createOverflowElement(
      *this, MediaControlCastButtonElement::create(*this, false)));
  m_overflowList->appendChild(
      m_toggleClosedCaptionsButton->createOverflowElement(
          *this, MediaControlToggleClosedCaptionsButtonElement::create(*this)));
}

void MediaControls::reset() {
  EventDispatchForbiddenScope::AllowUserAgentEvents allowEventsInShadow;
  BatchedControlUpdate batch(this);

  const double duration = mediaElement().duration();
  m_durationDisplay->setTextContent(
      LayoutTheme::theme().formatMediaControlsTime(duration));
  m_durationDisplay->setCurrentValue(duration);

  // Show everything that we might hide.
  // If we don't have a duration, then mark it to be hidden.  For the
  // old UI case, want / don't want is the same as show / hide since
  // it is never marked as not fitting.
  m_durationDisplay->setIsWanted(std::isfinite(duration));
  m_currentTimeDisplay->setIsWanted(true);
  m_timeline->setIsWanted(true);

  // If the player has entered an error state, force it into the paused state.
  if (mediaElement().error())
    mediaElement().pause();

  updatePlayState();

  updateCurrentTimeDisplay();

  m_timeline->setDuration(duration);
  m_timeline->setPosition(mediaElement().currentTime());

  onVolumeChange();

  refreshClosedCaptionsButtonVisibility();

  m_fullscreenButton->setIsWanted(shouldShowFullscreenButton(mediaElement()));

  refreshCastButtonVisibilityWithoutUpdate();

  m_downloadButton->setIsWanted(
      m_downloadButton->shouldDisplayDownloadButton());
}

LayoutObject* MediaControls::layoutObjectForTextTrackLayout() {
  return m_panel->layoutObject();
}

void MediaControls::show() {
  makeOpaque();
  m_panel->setIsWanted(true);
  m_panel->setIsDisplayed(true);
  if (m_overlayPlayButton)
    m_overlayPlayButton->updateDisplayType();
}

void MediaControls::hide() {
  m_panel->setIsWanted(false);
  m_panel->setIsDisplayed(false);
  if (m_overlayPlayButton)
    m_overlayPlayButton->setIsWanted(false);
}

bool MediaControls::isVisible() const {
  return m_panel->isOpaque();
}

void MediaControls::makeOpaque() {
  m_panel->makeOpaque();
}

void MediaControls::makeTransparent() {
  m_panel->makeTransparent();
}

bool MediaControls::shouldHideMediaControls(unsigned behaviorFlags) const {
  // Never hide for a media element without visual representation.
  if (!mediaElement().isHTMLVideoElement() || !mediaElement().hasVideo() ||
      mediaElement().isPlayingRemotely()) {
    return false;
  }

  // Keep the controls visible as long as the timer is running.
  const bool ignoreWaitForTimer = behaviorFlags & IgnoreWaitForTimer;
  if (!ignoreWaitForTimer && m_keepShowingUntilTimerFires)
    return false;

  // Don't hide if the mouse is over the controls.
  const bool ignoreControlsHover = behaviorFlags & IgnoreControlsHover;
  if (!ignoreControlsHover && m_panel->isHovered())
    return false;

  // Don't hide if the mouse is over the video area.
  const bool ignoreVideoHover = behaviorFlags & IgnoreVideoHover;
  if (!ignoreVideoHover && m_isMouseOverControls)
    return false;

  // Don't hide if focus is on the HTMLMediaElement or within the
  // controls/shadow tree. (Perform the checks separately to avoid going
  // through all the potential ancestor hosts for the focused element.)
  const bool ignoreFocus = behaviorFlags & IgnoreFocus;
  if (!ignoreFocus &&
      (mediaElement().isFocused() || contains(document().focusedElement()))) {
    return false;
  }

  // Don't hide the media controls when a panel is showing.
  if (m_textTrackList->isWanted() || m_overflowList->isWanted())
    return false;

  return true;
}

void MediaControls::playbackStarted() {
  BatchedControlUpdate batch(this);
  updatePlayState();
  m_timeline->setPosition(mediaElement().currentTime());
  updateCurrentTimeDisplay();

  startHideMediaControlsTimer();
}

void MediaControls::playbackProgressed() {
  m_timeline->setPosition(mediaElement().currentTime());
  updateCurrentTimeDisplay();

  if (isVisible() && shouldHideMediaControls())
    makeTransparent();
}

void MediaControls::playbackStopped() {
  updatePlayState();
  m_timeline->setPosition(mediaElement().currentTime());
  updateCurrentTimeDisplay();
  makeOpaque();

  stopHideMediaControlsTimer();
}

void MediaControls::updatePlayState() {
  if (m_isPausedForScrubbing)
    return;

  if (m_overlayPlayButton)
    m_overlayPlayButton->updateDisplayType();
  m_playButton->updateDisplayType();
}

void MediaControls::beginScrubbing() {
  if (!mediaElement().paused()) {
    m_isPausedForScrubbing = true;
    mediaElement().pause();
  }
}

void MediaControls::endScrubbing() {
  if (m_isPausedForScrubbing) {
    m_isPausedForScrubbing = false;
    if (mediaElement().paused())
      mediaElement().play();
  }
}

void MediaControls::updateCurrentTimeDisplay() {
  double now = mediaElement().currentTime();
  double duration = mediaElement().duration();

  // Allow the theme to format the time.
  m_currentTimeDisplay->setInnerText(
      LayoutTheme::theme().formatMediaControlsCurrentTime(now, duration),
      IGNORE_EXCEPTION);
  m_currentTimeDisplay->setCurrentValue(now);
}

void MediaControls::changedClosedCaptionsVisibility() {
  m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControls::refreshClosedCaptionsButtonVisibility() {
  m_toggleClosedCaptionsButton->setIsWanted(mediaElement().hasClosedCaptions());
  BatchedControlUpdate batch(this);
}

void MediaControls::toggleTextTrackList() {
  if (!mediaElement().hasClosedCaptions()) {
    m_textTrackList->setVisible(false);
    return;
  }

  if (!m_textTrackList->isWanted())
    m_windowEventListener->start();

  m_textTrackList->setVisible(!m_textTrackList->isWanted());
}

void MediaControls::showTextTrackAtIndex(unsigned indexToEnable) {
  TextTrackList* trackList = mediaElement().textTracks();
  if (indexToEnable >= trackList->length())
    return;
  TextTrack* track = trackList->anonymousIndexedGetter(indexToEnable);
  if (track && track->canBeRendered())
    track->setMode(TextTrack::showingKeyword());
}

void MediaControls::disableShowingTextTracks() {
  TextTrackList* trackList = mediaElement().textTracks();
  for (unsigned i = 0; i < trackList->length(); ++i) {
    TextTrack* track = trackList->anonymousIndexedGetter(i);
    if (track->mode() == TextTrack::showingKeyword())
      track->setMode(TextTrack::disabledKeyword());
  }
}

void MediaControls::refreshCastButtonVisibility() {
  refreshCastButtonVisibilityWithoutUpdate();
  BatchedControlUpdate batch(this);
}

void MediaControls::refreshCastButtonVisibilityWithoutUpdate() {
  if (!shouldShowCastButton(mediaElement())) {
    m_castButton->setIsWanted(false);
    m_overlayCastButton->setIsWanted(false);
    return;
  }

  // The reason for the autoplay test is that some pages (e.g. vimeo.com) have
  // an autoplay background video, which doesn't autoplay on Chrome for Android
  // (we prevent it) so starts paused. In such cases we don't want to
  // automatically show the cast button, since it looks strange and is unlikely
  // to correspond with anything the user wants to do.  If a user does want to
  // cast a paused autoplay video then they can still do so by touching or
  // clicking on the video, which will cause the cast button to appear.
  if (!mediaElement().shouldShowControls() && !mediaElement().autoplay() &&
      mediaElement().paused()) {
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
  }
}

void MediaControls::showOverlayCastButtonIfNeeded() {
  if (mediaElement().shouldShowControls() ||
      !shouldShowCastButton(mediaElement()))
    return;

  m_overlayCastButton->tryShowOverlay();
  resetHideMediaControlsTimer();
}

void MediaControls::enteredFullscreen() {
  m_fullscreenButton->setIsFullscreen(true);
  stopHideMediaControlsTimer();
  startHideMediaControlsTimer();
}

void MediaControls::exitedFullscreen() {
  m_fullscreenButton->setIsFullscreen(false);
  stopHideMediaControlsTimer();
  startHideMediaControlsTimer();
}

void MediaControls::startedCasting() {
  m_castButton->setIsPlayingRemotely(true);
  m_overlayCastButton->setIsPlayingRemotely(true);
}

void MediaControls::stoppedCasting() {
  m_castButton->setIsPlayingRemotely(false);
  m_overlayCastButton->setIsPlayingRemotely(false);
}

void MediaControls::defaultEventHandler(Event* event) {
  HTMLDivElement::defaultEventHandler(event);

  // Do not handle events to not interfere with the rest of the page if no
  // controls should be visible.
  if (!mediaElement().shouldShowControls())
    return;

  // Add IgnoreControlsHover to m_hideTimerBehaviorFlags when we see a touch
  // event, to allow the hide-timer to do the right thing when it fires.
  // FIXME: Preferably we would only do this when we're actually handling the
  // event here ourselves.
  bool isTouchEvent =
      event->isTouchEvent() || event->isGestureEvent() ||
      (event->isMouseEvent() && toMouseEvent(event)->fromTouch());
  m_hideTimerBehaviorFlags |= isTouchEvent ? IgnoreControlsHover : IgnoreNone;

  // Touch events are treated differently to avoid fake mouse events to trigger
  // random behavior. The expect behaviour for touch is that a tap will show the
  // controls and they will hide when the timer to hide fires.
  if (isTouchEvent) {
    if (event->type() != EventTypeNames::gesturetap)
      return;

    if (!containsRelatedTarget(event)) {
      if (!mediaElement().paused()) {
        if (!isVisible()) {
          makeOpaque();
          // When the panel switches from invisible to visible, we need to mark
          // the event handled to avoid buttons below the tap to be activated.
          event->setDefaultHandled();
        }
        if (shouldHideMediaControls(IgnoreWaitForTimer)) {
          m_keepShowingUntilTimerFires = true;
          startHideMediaControlsTimer();
        }
      }
    }

    return;
  }

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

void MediaControls::hideMediaControlsTimerFired(TimerBase*) {
  unsigned behaviorFlags =
      m_hideTimerBehaviorFlags | IgnoreFocus | IgnoreVideoHover;
  m_hideTimerBehaviorFlags = IgnoreNone;
  m_keepShowingUntilTimerFires = false;

  if (mediaElement().paused())
    return;

  if (!shouldHideMediaControls(behaviorFlags))
    return;

  makeTransparent();
  m_overlayCastButton->setIsWanted(false);
}

void MediaControls::startHideMediaControlsTimer() {
  m_hideMediaControlsTimer.startOneShot(
      timeWithoutMouseMovementBeforeHidingMediaControls, BLINK_FROM_HERE);
}

void MediaControls::stopHideMediaControlsTimer() {
  m_keepShowingUntilTimerFires = false;
  m_hideMediaControlsTimer.stop();
}

void MediaControls::resetHideMediaControlsTimer() {
  stopHideMediaControlsTimer();
  if (!mediaElement().paused())
    startHideMediaControlsTimer();
}

bool MediaControls::containsRelatedTarget(Event* event) {
  if (!event->isMouseEvent())
    return false;
  EventTarget* relatedTarget = toMouseEvent(event)->relatedTarget();
  if (!relatedTarget)
    return false;
  return contains(relatedTarget->toNode());
}

void MediaControls::onVolumeChange() {
  m_muteButton->updateDisplayType();
  m_volumeSlider->setVolume(mediaElement().muted() ? 0
                                                   : mediaElement().volume());

  // Update visibility of volume controls.
  // TODO(mlamouri): it should not be part of the volumechange handling because
  // it is using audio availability as input.
  BatchedControlUpdate batch(this);
  m_volumeSlider->setIsWanted(mediaElement().hasAudio() &&
                              !preferHiddenVolumeControls(document()));
  m_muteButton->setIsWanted(mediaElement().hasAudio());
}

void MediaControls::onFocusIn() {
  if (!mediaElement().shouldShowControls())
    return;

  show();
  resetHideMediaControlsTimer();
}

void MediaControls::notifyPanelWidthChanged(const LayoutUnit& newWidth) {
  // Don't bother to do any work if this matches the most recent panel
  // width, since we're called after layout.
  // Note that this code permits a bad frame on resize, since it is
  // run after the relayout / paint happens.  It would be great to improve
  // this, but it would be even greater to move this code entirely to
  // JS and fix it there.
  m_panelWidth = newWidth.toInt();

  // Adjust for effective zoom.
  if (!m_panel->layoutObject() || !m_panel->layoutObject()->style())
    return;
  m_panelWidth =
      ceil(m_panelWidth / m_panel->layoutObject()->style()->effectiveZoom());

  m_panelWidthChangedTimer.startOneShot(0, BLINK_FROM_HERE);
}

void MediaControls::panelWidthChangedTimerFired(TimerBase*) {
  computeWhichControlsFit();
}

void MediaControls::computeWhichControlsFit() {
  // Hide all controls that don't fit, and show the ones that do.
  // This might be better suited for a layout, but since JS media controls
  // won't benefit from that anwyay, we just do it here like JS will.

  // Controls that we'll hide / show, in order of decreasing priority.
  MediaControlElement* elements[] = {
      // Exclude m_overflowMenu; we handle it specially.
      m_playButton.get(),
      m_fullscreenButton.get(),
      m_downloadButton.get(),
      m_timeline.get(),
      m_muteButton.get(),
      m_volumeSlider.get(),
      m_toggleClosedCaptionsButton.get(),
      m_castButton.get(),
      m_currentTimeDisplay.get(),
      m_durationDisplay.get(),
  };

  int usedWidth = 0;

  // TODO(mlamouri): we need a more dynamic way to find out the width of an
  // element.
  const int sliderMargin = 36;  // Sliders have 18px margin on each side.

  // Assume that all controls require 48px, unless we can get the computed
  // style for the play button.  Since the play button or overflow is always
  // shown, one of the two buttons should be available the first time we're
  // called after layout.  This will
  // also be the first time we have m_panelWidth!=0, so it won't matter if
  // we get this wrong before that.
  int minimumWidth = 48;
  if (m_playButton->layoutObject() && m_playButton->layoutObject()->style()) {
    const ComputedStyle* style = m_playButton->layoutObject()->style();
    minimumWidth = ceil(style->width().pixels() / style->effectiveZoom());
  } else if (m_overflowMenu->layoutObject() &&
             m_overflowMenu->layoutObject()->style()) {
    const ComputedStyle* style = m_overflowMenu->layoutObject()->style();
    minimumWidth = ceil(style->width().pixels() / style->effectiveZoom());
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

  // Insert an overflow menu. However, if we see that the overflow menu
  // doesn't end up containing at least two elements, we will not display it
  // but instead make place for the first element that was dropped.
  m_overflowMenu->setDoesFit(true);
  m_overflowMenu->setIsWanted(true);
  usedWidth = minimumWidth;

  std::list<MediaControlElement*> overflowElements;
  MediaControlElement* firstDisplacedElement = nullptr;
  // For each control that fits, enable it in order of decreasing priority.
  for (MediaControlElement* element : elements) {
    if (!element)
      continue;
    int width = minimumWidth;
    if ((element == m_timeline.get()) || (element == m_volumeSlider.get()))
      width += sliderMargin;
    element->shouldShowButtonInOverflowMenu(false);
    if (element->isWanted()) {
      if (usedWidth + width <= m_panelWidth) {
        element->setDoesFit(true);
        usedWidth += width;
      } else {
        element->setDoesFit(false);
        element->shouldShowButtonInOverflowMenu(true);
        if (element->hasOverflowButton())
          overflowElements.push_front(element);
        // We want a way to access the first media element that was
        // removed. If we don't end up needing an overflow menu, we can
        // use the space the overflow menu would have taken up to
        // instead display that media element.
        if (!element->hasOverflowButton() && !firstDisplacedElement)
          firstDisplacedElement = element;
      }
    }
  }

  // If we don't have at least two overflow elements, we will not show the
  // overflow menu.
  if (overflowElements.empty()) {
    m_overflowMenu->setIsWanted(false);
    usedWidth -= minimumWidth;
    if (firstDisplacedElement) {
      int width = minimumWidth;
      if ((firstDisplacedElement == m_timeline.get()) ||
          (firstDisplacedElement == m_volumeSlider.get()))
        width += sliderMargin;
      if (usedWidth + width <= m_panelWidth)
        firstDisplacedElement->setDoesFit(true);
    }
  } else if (overflowElements.size() == 1) {
    m_overflowMenu->setIsWanted(false);
    overflowElements.front()->setDoesFit(true);
  }
}

void MediaControls::invalidate(Element* element) {
  if (!element)
    return;

  if (LayoutObject* layoutObject = element->layoutObject())
    layoutObject
        ->setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
}

void MediaControls::networkStateChanged() {
  invalidate(m_playButton);
  invalidate(m_overlayPlayButton);
  invalidate(m_muteButton);
  invalidate(m_fullscreenButton);
  invalidate(m_downloadButton);
  invalidate(m_timeline);
  invalidate(m_volumeSlider);
}

bool MediaControls::overflowMenuVisible() {
  return m_overflowList ? m_overflowList->isWanted() : false;
}

void MediaControls::toggleOverflowMenu() {
  DCHECK(m_overflowList);

  if (!m_overflowList->isWanted())
    m_windowEventListener->start();
  m_overflowList->setIsWanted(!m_overflowList->isWanted());
}

void MediaControls::hideAllMenus() {
  m_windowEventListener->stop();

  if (m_overflowList->isWanted())
    m_overflowList->setIsWanted(false);
  if (m_textTrackList->isWanted())
    m_textTrackList->setVisible(false);
}

DEFINE_TRACE(MediaControls) {
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
  visitor->trace(m_fullscreenButton);
  visitor->trace(m_downloadButton);
  visitor->trace(m_durationDisplay);
  visitor->trace(m_enclosure);
  visitor->trace(m_textTrackList);
  visitor->trace(m_overflowMenu);
  visitor->trace(m_overflowList);
  visitor->trace(m_castButton);
  visitor->trace(m_overlayCastButton);
  visitor->trace(m_mediaEventListener);
  visitor->trace(m_windowEventListener);
  HTMLDivElement::trace(visitor);
}

}  // namespace blink
