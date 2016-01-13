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

namespace WebCore {

class Document;
class Event;

class MediaControls FINAL : public HTMLDivElement {
public:
    static PassRefPtrWillBeRawPtr<MediaControls> create(HTMLMediaElement&);

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
    void closedCaptionTracksChanged();

    void enteredFullscreen();
    void exitedFullscreen();

    void updateTextTrackDisplay();

    void mediaElementFocused();

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit MediaControls(HTMLMediaElement&);

    bool initializeControls();

    void makeOpaque();
    void makeTransparent();

    void updatePlayState();

    enum HideBehaviorFlags {
        IgnoreVideoHover = 1 << 0,
        IgnoreFocus = 1 << 1
    };

    bool shouldHideMediaControls(unsigned behaviorFlags = 0) const;
    void hideMediaControlsTimerFired(Timer<MediaControls>*);
    void startHideMediaControlsTimer();
    void stopHideMediaControlsTimer();

    void createTextTrackDisplay();
    void showTextTrackDisplay();
    void hideTextTrackDisplay();

    // Node
    virtual bool isMediaControls() const OVERRIDE { return true; }
    virtual bool willRespondToMouseMoveEvents() OVERRIDE { return true; }
    virtual void defaultEventHandler(Event*) OVERRIDE;
    bool containsRelatedTarget(Event*);

    // Element
    virtual const AtomicString& shadowPseudoId() const OVERRIDE;

    RawPtrWillBeMember<HTMLMediaElement> m_mediaElement;

    // Container for the media control elements.
    RawPtrWillBeMember<MediaControlPanelElement> m_panel;

    // Container for the text track cues.
    RawPtrWillBeMember<MediaControlTextTrackContainerElement> m_textDisplayContainer;

    // Media control elements.
    RawPtrWillBeMember<MediaControlOverlayPlayButtonElement> m_overlayPlayButton;
    RawPtrWillBeMember<MediaControlOverlayEnclosureElement> m_overlayEnclosure;
    RawPtrWillBeMember<MediaControlPlayButtonElement> m_playButton;
    RawPtrWillBeMember<MediaControlCurrentTimeDisplayElement> m_currentTimeDisplay;
    RawPtrWillBeMember<MediaControlTimelineElement> m_timeline;
    RawPtrWillBeMember<MediaControlMuteButtonElement> m_muteButton;
    RawPtrWillBeMember<MediaControlVolumeSliderElement> m_volumeSlider;
    RawPtrWillBeMember<MediaControlToggleClosedCaptionsButtonElement> m_toggleClosedCaptionsButton;
    RawPtrWillBeMember<MediaControlFullscreenButtonElement> m_fullScreenButton;
    RawPtrWillBeMember<MediaControlTimeRemainingDisplayElement> m_durationDisplay;
    RawPtrWillBeMember<MediaControlPanelEnclosureElement> m_enclosure;

    Timer<MediaControls> m_hideMediaControlsTimer;
    bool m_isMouseOverControls : 1;
    bool m_isPausedForScrubbing : 1;
};

DEFINE_ELEMENT_TYPE_CASTS(MediaControls, isMediaControls());

}

#endif
