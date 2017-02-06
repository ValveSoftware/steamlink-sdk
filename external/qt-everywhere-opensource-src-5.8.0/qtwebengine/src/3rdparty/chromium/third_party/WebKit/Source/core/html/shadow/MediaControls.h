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

#ifndef MediaControls_h
#define MediaControls_h

#include "core/html/HTMLDivElement.h"
#include "core/html/shadow/MediaControlElements.h"

namespace blink {

class Event;
class TextTrackContainer;

class CORE_EXPORT MediaControls final : public HTMLDivElement {
public:
    static MediaControls* create(HTMLMediaElement&);

    HTMLMediaElement& mediaElement() const { return *m_mediaElement; }

    void reset();

    void show();
    void hide();

    void playbackStarted();
    void playbackProgressed();
    void playbackStopped();

    void beginScrubbing();
    void endScrubbing();

    void updateCurrentTimeDisplay();

    void updateVolume();

    void changedClosedCaptionsVisibility();
    void refreshClosedCaptionsButtonVisibility();
    void toggleTextTrackList();

    void enteredFullscreen();
    void exitedFullscreen();

    void startedCasting();
    void stoppedCasting();
    void refreshCastButtonVisibility();
    void showOverlayCastButtonIfNeeded();
    // Update cast button visibility, but don't try to update our panel
    // button visibility for space.
    void refreshCastButtonVisibilityWithoutUpdate();

    void setAllowHiddenVolumeControls(bool);

    void mediaElementFocused();

    // Returns the layout object for the part of the controls that should be
    // used for overlap checking during text track layout. May be null.
    LayoutObject* layoutObjectForTextTrackLayout();

    // Notify us that our controls enclosure has changed width.
    void notifyPanelWidthChanged(const LayoutUnit& newWidth);

    // Notify us that the media element's network state has changed.
    void networkStateChanged();

    DECLARE_VIRTUAL_TRACE();

private:
    void invalidate(Element*);

    class BatchedControlUpdate;

    explicit MediaControls(HTMLMediaElement&);

    void initializeControls();

    void makeOpaque();
    void makeTransparent();

    void updatePlayState();

    enum HideBehaviorFlags {
        IgnoreNone = 0,
        IgnoreVideoHover = 1 << 0,
        IgnoreFocus = 1 << 1,
        IgnoreControlsHover = 1 << 2
    };

    bool shouldHideMediaControls(unsigned behaviorFlags = 0) const;
    void hideMediaControlsTimerFired(Timer<MediaControls>*);
    void startHideMediaControlsTimer();
    void stopHideMediaControlsTimer();
    void resetHideMediaControlsTimer();

    void panelWidthChangedTimerFired(Timer<MediaControls>*);

    // Hide elements that don't fit, and show those things that we want which
    // do fit.  This requires that m_panelWidth is current.
    void computeWhichControlsFit();

    // Node
    bool isMediaControls() const override { return true; }
    bool willRespondToMouseMoveEvents() override { return true; }
    void defaultEventHandler(Event*) override;
    bool containsRelatedTarget(Event*);

    Member<HTMLMediaElement> m_mediaElement;

    // Media control elements.
    Member<MediaControlOverlayEnclosureElement> m_overlayEnclosure;
    Member<MediaControlOverlayPlayButtonElement> m_overlayPlayButton;
    Member<MediaControlCastButtonElement> m_overlayCastButton;
    Member<MediaControlPanelEnclosureElement> m_enclosure;
    Member<MediaControlPanelElement> m_panel;
    Member<MediaControlPlayButtonElement> m_playButton;
    Member<MediaControlTimelineElement> m_timeline;
    Member<MediaControlCurrentTimeDisplayElement> m_currentTimeDisplay;
    Member<MediaControlTimeRemainingDisplayElement> m_durationDisplay;
    Member<MediaControlMuteButtonElement> m_muteButton;
    Member<MediaControlVolumeSliderElement> m_volumeSlider;
    Member<MediaControlToggleClosedCaptionsButtonElement> m_toggleClosedCaptionsButton;
    Member<MediaControlTextTrackListElement> m_textTrackList;
    Member<MediaControlCastButtonElement> m_castButton;
    Member<MediaControlFullscreenButtonElement> m_fullScreenButton;

    Timer<MediaControls> m_hideMediaControlsTimer;
    unsigned m_hideTimerBehaviorFlags;
    bool m_isMouseOverControls : 1;
    bool m_isPausedForScrubbing : 1;

    Timer<MediaControls> m_panelWidthChangedTimer;
    int m_panelWidth;

    bool m_allowHiddenVolumeControls : 1;
};

DEFINE_ELEMENT_TYPE_CASTS(MediaControls, isMediaControls());

} // namespace blink

#endif
