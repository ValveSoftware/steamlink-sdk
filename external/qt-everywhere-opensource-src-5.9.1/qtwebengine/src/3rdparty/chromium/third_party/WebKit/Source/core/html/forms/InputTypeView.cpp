/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/InputTypeView.h"

#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"
#include "core/layout/LayoutObject.h"

namespace blink {

InputTypeView::~InputTypeView() {}

DEFINE_TRACE(InputTypeView) {
  visitor->trace(m_element);
}

bool InputTypeView::sizeShouldIncludeDecoration(int, int& preferredSize) const {
  preferredSize = element().size();
  return false;
}

void InputTypeView::handleClickEvent(MouseEvent*) {}

void InputTypeView::handleMouseDownEvent(MouseEvent*) {}

void InputTypeView::handleKeydownEvent(KeyboardEvent*) {}

void InputTypeView::handleKeypressEvent(KeyboardEvent*) {}

void InputTypeView::handleKeyupEvent(KeyboardEvent*) {}

void InputTypeView::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) {}

void InputTypeView::handleDOMActivateEvent(Event*) {}

void InputTypeView::forwardEvent(Event*) {}

void InputTypeView::dispatchSimulatedClickIfActive(KeyboardEvent* event) const {
  if (element().isActive())
    element().dispatchSimulatedClick(event);
  event->setDefaultHandled();
}

void InputTypeView::accessKeyAction(bool) {
  element().focus(
      FocusParams(SelectionBehaviorOnFocus::Reset, WebFocusTypeNone, nullptr));
}

bool InputTypeView::shouldSubmitImplicitly(Event* event) {
  return event->isKeyboardEvent() &&
         event->type() == EventTypeNames::keypress &&
         toKeyboardEvent(event)->charCode() == '\r';
}

HTMLFormElement* InputTypeView::formForSubmission() const {
  return element().form();
}

LayoutObject* InputTypeView::createLayoutObject(
    const ComputedStyle& style) const {
  return LayoutObject::createObject(&element(), style);
}

PassRefPtr<ComputedStyle> InputTypeView::customStyleForLayoutObject(
    PassRefPtr<ComputedStyle> originalStyle) {
  return originalStyle;
}

TextDirection InputTypeView::computedTextDirection() {
  return element().ensureComputedStyle()->direction();
}

void InputTypeView::blur() {
  element().defaultBlur();
}

bool InputTypeView::hasCustomFocusLogic() const {
  return true;
}

void InputTypeView::handleFocusEvent(Element*, WebFocusType) {}

void InputTypeView::handleBlurEvent() {}

void InputTypeView::handleFocusInEvent(Element*, WebFocusType) {}

void InputTypeView::startResourceLoading() {}

void InputTypeView::closePopupView() {}

void InputTypeView::createShadowSubtree() {}

void InputTypeView::destroyShadowSubtree() {
  if (ShadowRoot* root = element().userAgentShadowRoot())
    root->removeChildren();
}

void InputTypeView::altAttributeChanged() {}

void InputTypeView::srcAttributeChanged() {}

void InputTypeView::minOrMaxAttributeChanged() {}

void InputTypeView::stepAttributeChanged() {}

ClickHandlingState* InputTypeView::willDispatchClick() {
  return nullptr;
}

void InputTypeView::didDispatchClick(Event*, const ClickHandlingState&) {}

void InputTypeView::updateView() {}

void InputTypeView::attributeChanged() {}

void InputTypeView::multipleAttributeChanged() {}

void InputTypeView::disabledAttributeChanged() {}

void InputTypeView::readonlyAttributeChanged() {}

void InputTypeView::requiredAttributeChanged() {}

void InputTypeView::valueAttributeChanged() {}

void InputTypeView::didSetValue(const String&, bool) {}

void InputTypeView::subtreeHasChanged() {
  NOTREACHED();
}

void InputTypeView::listAttributeTargetChanged() {}

void InputTypeView::updateClearButtonVisibility() {}

void InputTypeView::updatePlaceholderText() {}

AXObject* InputTypeView::popupRootAXObject() {
  return nullptr;
}

FormControlState InputTypeView::saveFormControlState() const {
  String currentValue = element().value();
  if (currentValue == element().defaultValue())
    return FormControlState();
  return FormControlState(currentValue);
}

void InputTypeView::restoreFormControlState(const FormControlState& state) {
  element().setValue(state[0]);
}

bool InputTypeView::hasBadInput() const {
  return false;
}

DEFINE_TRACE(ClickHandlingState) {
  visitor->trace(checkedRadioButton);
  EventDispatchHandlingState::trace(visitor);
}

}  // namespace blink
