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

#ifndef MediaControlElementTypes_h
#define MediaControlElementTypes_h

#include "core/CoreExport.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLInputElement.h"
#include "public/platform/WebLocalizedString.h"

namespace blink {

class HTMLMediaElement;
class MediaControls;

enum MediaControlElementType {
  MediaEnterFullscreenButton = 0,
  MediaMuteButton,
  MediaPlayButton,
  MediaSlider,
  MediaSliderThumb,
  MediaShowClosedCaptionsButton,
  MediaHideClosedCaptionsButton,
  MediaTextTrackList,
  MediaUnMuteButton,
  MediaPauseButton,
  MediaTimelineContainer,
  MediaCurrentTimeDisplay,
  MediaTimeRemainingDisplay,
  MediaTrackSelectionCheckmark,
  MediaControlsPanel,
  MediaVolumeSliderContainer,
  MediaVolumeSlider,
  MediaVolumeSliderThumb,
  MediaFullscreenVolumeSlider,
  MediaFullscreenVolumeSliderThumb,
  MediaExitFullscreenButton,
  MediaOverlayPlayButton,
  MediaCastOffButton,
  MediaCastOnButton,
  MediaOverlayCastOffButton,
  MediaOverlayCastOnButton,
  MediaOverflowButton,
  MediaOverflowList,
  MediaDownloadButton,
};

CORE_EXPORT const HTMLMediaElement* toParentMediaElement(const Node*);
CORE_EXPORT const HTMLMediaElement* toParentMediaElement(const LayoutObject&);

CORE_EXPORT MediaControlElementType mediaControlElementType(const Node*);

// ----------------------------

class MediaControlElement : public GarbageCollectedMixin {
 public:
  // These hold the state about whether this control should be shown if
  // space permits.  These will also show / hide as needed.
  void setIsWanted(bool);
  bool isWanted();

  // Tell us whether we fit or not.  This will hide / show the control as
  // needed, also.
  void setDoesFit(bool);

  MediaControlElementType displayType() const { return m_displayType; }

  // By default, media controls elements are not added to the overflow menu.
  // Controls that can be added to the overflow menu should override this
  // function and return true.
  virtual bool hasOverflowButton() { return false; }

  // If true, shows the overflow menu item if it exists. Hides it if false.
  void shouldShowButtonInOverflowMenu(bool);

  // Returns a string representation of the media control element. Used for
  // the overflow menu.
  String getOverflowMenuString();

  // Updates the value of the Text string shown in the overflow menu.
  void updateOverflowString();

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlElement(MediaControls&, MediaControlElementType, HTMLElement*);

  MediaControls& mediaControls() const {
    DCHECK(m_mediaControls);
    return *m_mediaControls;
  }
  HTMLMediaElement& mediaElement() const;

  void setDisplayType(MediaControlElementType);

  // Represents the overflow menu element for this media control.
  // The Element contains the button that the user can click on, but having
  // the button within an Element enables us to style the overflow menu.
  // Setting this pointer is optional so it may be null.
  Member<Element> m_overflowMenuElement;

  // The text representation of the button within the overflow menu.
  Member<Text> m_overflowMenuText;

 private:
  // Hide or show based on our fits / wanted state.  We want to show
  // if and only if we're wanted and we fit.
  void updateShownState();

  // Returns a string representation of the media control element.
  // Subclasses should override this method to return the string representation
  // of the overflow button.
  virtual WebLocalizedString::Name getOverflowStringName() {
    NOTREACHED();
    return WebLocalizedString::AXAMPMFieldText;
  }

  Member<MediaControls> m_mediaControls;
  MediaControlElementType m_displayType;
  Member<HTMLElement> m_element;
  bool m_isWanted : 1;
  bool m_doesFit : 1;
};

// ----------------------------

class MediaControlDivElement : public HTMLDivElement,
                               public MediaControlElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlDivElement);

 public:
  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlDivElement(MediaControls&, MediaControlElementType);

 private:
  bool isMediaControlElement() const final { return true; }
};

// ----------------------------

class MediaControlInputElement : public HTMLInputElement,
                                 public MediaControlElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlInputElement);

 public:
  DECLARE_VIRTUAL_TRACE();

  // Creates an overflow menu element with the given button as a child.
  HTMLElement* createOverflowElement(MediaControls&, MediaControlInputElement*);

 protected:
  MediaControlInputElement(MediaControls&, MediaControlElementType);

 private:
  virtual void updateDisplayType() {}
  bool isMediaControlElement() const final { return true; }
  bool isMouseFocusable() const override;

  // Creates an overflow menu HTML element.
  virtual MediaControlInputElement* createOverflowButton(MediaControls&) {
    return nullptr;
  }
};

// ----------------------------

class MediaControlTimeDisplayElement : public MediaControlDivElement {
 public:
  void setCurrentValue(double);
  double currentValue() const { return m_currentValue; }

 protected:
  MediaControlTimeDisplayElement(MediaControls&, MediaControlElementType);

 private:
  double m_currentValue;
};

}  // namespace blink

#endif  // MediaControlElementTypes_h
