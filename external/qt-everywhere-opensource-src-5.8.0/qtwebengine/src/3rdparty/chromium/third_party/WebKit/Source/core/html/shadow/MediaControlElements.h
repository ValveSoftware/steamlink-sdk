/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlElements_h
#define MediaControlElements_h

#include "core/html/shadow/MediaControlElementTypes.h"

namespace blink {

class TextTrack;

// ----------------------------

class MediaControlPanelElement final : public MediaControlDivElement {
public:
    static MediaControlPanelElement* create(MediaControls&);

    void setIsDisplayed(bool);

    void makeOpaque();
    void makeTransparent();

private:
    explicit MediaControlPanelElement(MediaControls&);

    void defaultEventHandler(Event*) override;
    bool keepEventInNode(Event*) override;

    void startTimer();
    void stopTimer();
    void transitionTimerFired(Timer<MediaControlPanelElement>*);
    void didBecomeVisible();

    bool m_isDisplayed;
    bool m_opaque;

    Timer<MediaControlPanelElement> m_transitionTimer;
};

// ----------------------------

class MediaControlPanelEnclosureElement final : public MediaControlDivElement {
public:
    static MediaControlPanelEnclosureElement* create(MediaControls&);

private:
    explicit MediaControlPanelEnclosureElement(MediaControls&);
};

// ----------------------------

class MediaControlOverlayEnclosureElement final : public MediaControlDivElement {
public:
    static MediaControlOverlayEnclosureElement* create(MediaControls&);

private:
    explicit MediaControlOverlayEnclosureElement(MediaControls&);
    EventDispatchHandlingState* preDispatchEventHandler(Event*) override;
};

// ----------------------------

class MediaControlMuteButtonElement final : public MediaControlInputElement {
public:
    static MediaControlMuteButtonElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override { return true; }
    void updateDisplayType() override;

private:
    explicit MediaControlMuteButtonElement(MediaControls&);

    void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlPlayButtonElement final : public MediaControlInputElement {
public:
    static MediaControlPlayButtonElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override { return true; }
    void updateDisplayType() override;

private:
    explicit MediaControlPlayButtonElement(MediaControls&);

    void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlOverlayPlayButtonElement final : public MediaControlInputElement {
public:
    static MediaControlOverlayPlayButtonElement* create(MediaControls&);

    void updateDisplayType() override;

private:
    explicit MediaControlOverlayPlayButtonElement(MediaControls&);

    void defaultEventHandler(Event*) override;
    bool keepEventInNode(Event*) override;
};

// ----------------------------

class MediaControlToggleClosedCaptionsButtonElement final : public MediaControlInputElement {
public:
    static MediaControlToggleClosedCaptionsButtonElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override { return true; }

    void updateDisplayType() override;

private:
    explicit MediaControlToggleClosedCaptionsButtonElement(MediaControls&);

    void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlTextTrackListElement final : public MediaControlDivElement {
public:
    static MediaControlTextTrackListElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override { return true; }

    void setVisible(bool);

private:
    explicit MediaControlTextTrackListElement(MediaControls&);

    void defaultEventHandler(Event*) override;

    void refreshTextTrackListMenu();

    // Returns the label for the track when a valid track is passed in and "Off" when the parameter is null.
    String getTextTrackLabel(TextTrack*);
    // Creates the track element in the list when a valid track is passed in and the "Off" item when the parameter is null.
    Element* createTextTrackListItem(TextTrack*);

    void showTextTrackAtIndex(unsigned);
    void disableShowingTextTracks();
};

// ----------------------------

class MediaControlTimelineElement final : public MediaControlInputElement {
public:
    static MediaControlTimelineElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override;

    // FIXME: An "earliest possible position" will be needed once that concept
    // is supported by HTMLMediaElement, see https://crbug.com/137275
    void setPosition(double);
    void setDuration(double);

private:
    explicit MediaControlTimelineElement(MediaControls&);

    void defaultEventHandler(Event*) override;
    bool keepEventInNode(Event*) override;
};

// ----------------------------

class MediaControlFullscreenButtonElement final : public MediaControlInputElement {
public:
    static MediaControlFullscreenButtonElement* create(MediaControls&);

    bool willRespondToMouseClickEvents() override { return true; }

    void setIsFullscreen(bool);

private:
    explicit MediaControlFullscreenButtonElement(MediaControls&);

    void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlCastButtonElement final : public MediaControlInputElement {
public:
    static MediaControlCastButtonElement* create(MediaControls&, bool isOverlayButton);

    bool willRespondToMouseClickEvents() override { return true; }

    void setIsPlayingRemotely(bool);

    // This will show a cast button if it is not covered by another element.
    // This MUST be called for cast button elements that are overlay elements.
    void tryShowOverlay();

private:
    explicit MediaControlCastButtonElement(MediaControls&, bool isOverlayButton);

    const AtomicString& shadowPseudoId() const override;
    void defaultEventHandler(Event*) override;
    bool keepEventInNode(Event*) override;

    bool m_isOverlayButton;

    // This is used for UMA histogram (Cast.Sender.Overlay). New values should
    // be appended only and must be added before |Count|.
    enum class CastOverlayMetrics {
        Created = 0,
        Shown,
        Clicked,
        Count // Keep last.
    };
    void recordMetrics(CastOverlayMetrics);

    // UMA related boolean. They are used to prevent counting something twice
    // for the same media element.
    bool m_clickUseCounted = false;
    bool m_showUseCounted = false;
};

// ----------------------------

class MediaControlVolumeSliderElement final : public MediaControlInputElement {
public:
    static MediaControlVolumeSliderElement* create(MediaControls&);

    bool willRespondToMouseMoveEvents() override;
    bool willRespondToMouseClickEvents() override;
    void setVolume(double);

private:
    explicit MediaControlVolumeSliderElement(MediaControls&);

    void defaultEventHandler(Event*) override;
    bool keepEventInNode(Event*) override;
};

// ----------------------------

class MediaControlTimeRemainingDisplayElement final : public MediaControlTimeDisplayElement {
public:
    static MediaControlTimeRemainingDisplayElement* create(MediaControls&);

private:
    explicit MediaControlTimeRemainingDisplayElement(MediaControls&);
};

// ----------------------------

class MediaControlCurrentTimeDisplayElement final : public MediaControlTimeDisplayElement {
public:
    static MediaControlCurrentTimeDisplayElement* create(MediaControls&);

private:
    explicit MediaControlCurrentTimeDisplayElement(MediaControls&);
};

} // namespace blink

#endif // MediaControlElements_h
