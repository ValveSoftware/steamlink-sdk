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

#include "core/html/shadow/MediaControlElementTypes.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Text.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/layout/LayoutObject.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

class Event;

const HTMLMediaElement* toParentMediaElement(const Node* node) {
  if (!node)
    return nullptr;
  const Node* mediaNode = node->ownerShadowHost();
  if (!mediaNode)
    return nullptr;
  if (!isHTMLMediaElement(mediaNode))
    return nullptr;

  return toHTMLMediaElement(mediaNode);
}

const HTMLMediaElement* toParentMediaElement(const LayoutObject& layoutObject) {
  return toParentMediaElement(layoutObject.node());
}

MediaControlElementType mediaControlElementType(const Node* node) {
  SECURITY_DCHECK(node->isMediaControlElement());
  const HTMLElement* element = toHTMLElement(node);
  if (isHTMLInputElement(*element))
    return static_cast<const MediaControlInputElement*>(element)->displayType();
  return static_cast<const MediaControlDivElement*>(element)->displayType();
}

MediaControlElement::MediaControlElement(MediaControls& mediaControls,
                                         MediaControlElementType displayType,
                                         HTMLElement* element)
    : m_mediaControls(&mediaControls),
      m_displayType(displayType),
      m_element(element),
      m_isWanted(true),
      m_doesFit(true) {}

HTMLMediaElement& MediaControlElement::mediaElement() const {
  return mediaControls().mediaElement();
}

void MediaControlElement::updateShownState() {
  if (m_isWanted && m_doesFit)
    m_element->removeInlineStyleProperty(CSSPropertyDisplay);
  else
    m_element->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

void MediaControlElement::setDoesFit(bool fits) {
  m_doesFit = fits;
  updateShownState();
}

void MediaControlElement::setIsWanted(bool wanted) {
  if (m_isWanted == wanted)
    return;
  m_isWanted = wanted;
  updateShownState();
}

bool MediaControlElement::isWanted() {
  return m_isWanted;
}

void MediaControlElement::setDisplayType(MediaControlElementType displayType) {
  if (displayType == m_displayType)
    return;

  m_displayType = displayType;
  if (LayoutObject* object = m_element->layoutObject())
    object->setShouldDoFullPaintInvalidation();
}

void MediaControlElement::shouldShowButtonInOverflowMenu(bool shouldShow) {
  if (!hasOverflowButton())
    return;
  if (shouldShow) {
    m_overflowMenuElement->removeInlineStyleProperty(CSSPropertyDisplay);
  } else {
    m_overflowMenuElement->setInlineStyleProperty(CSSPropertyDisplay,
                                                  CSSValueNone);
  }
}

String MediaControlElement::getOverflowMenuString() {
  return mediaElement().locale().queryString(getOverflowStringName());
}

void MediaControlElement::updateOverflowString() {
  if (m_overflowMenuElement && m_overflowMenuText)
    m_overflowMenuText->replaceWholeText(getOverflowMenuString());
}

DEFINE_TRACE(MediaControlElement) {
  visitor->trace(m_mediaControls);
  visitor->trace(m_element);
  visitor->trace(m_overflowMenuElement);
  visitor->trace(m_overflowMenuText);
}

// ----------------------------

MediaControlDivElement::MediaControlDivElement(
    MediaControls& mediaControls,
    MediaControlElementType displayType)
    : HTMLDivElement(mediaControls.document()),
      MediaControlElement(mediaControls, displayType, this) {}

DEFINE_TRACE(MediaControlDivElement) {
  MediaControlElement::trace(visitor);
  HTMLDivElement::trace(visitor);
}

// ----------------------------

MediaControlInputElement::MediaControlInputElement(
    MediaControls& mediaControls,
    MediaControlElementType displayType)
    : HTMLInputElement(mediaControls.document(), 0, false),
      MediaControlElement(mediaControls, displayType, this) {}

bool MediaControlInputElement::isMouseFocusable() const {
  return false;
}

HTMLElement* MediaControlInputElement::createOverflowElement(
    MediaControls& mediaControls,
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->setIsWanted(false);

  m_overflowMenuText =
      Text::create(mediaControls.document(), button->getOverflowMenuString());

  HTMLLabelElement* element =
      HTMLLabelElement::create(mediaControls.document());
  element->setShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list-item"));
  // Appending a button to a label element ensures that clicks on the label
  // are passed down to the button, performing the action we'd expect.
  element->appendChild(button);
  element->appendChild(m_overflowMenuText);
  m_overflowMenuElement = element;
  return element;
}

DEFINE_TRACE(MediaControlInputElement) {
  MediaControlElement::trace(visitor);
  HTMLInputElement::trace(visitor);
}

// ----------------------------

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(
    MediaControls& mediaControls,
    MediaControlElementType displayType)
    : MediaControlDivElement(mediaControls, displayType), m_currentValue(0) {}

void MediaControlTimeDisplayElement::setCurrentValue(double time) {
  m_currentValue = time;
}

}  // namespace blink
