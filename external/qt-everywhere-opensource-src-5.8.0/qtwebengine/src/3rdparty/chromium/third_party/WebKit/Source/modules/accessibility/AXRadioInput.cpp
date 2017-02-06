// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "modules/accessibility/AXRadioInput.h"

#include "core/InputTypeNames.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/RadioInputType.h"
#include "core/layout/LayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXRadioInput::AXRadioInput(LayoutObject* layoutObject, AXObjectCacheImpl& axObjectCache)
    : AXLayoutObject(layoutObject, axObjectCache)
{
    // Updates posInSet and setSize for the current object and the next objects.
    if (!calculatePosInSet())
        return;
    // When a new object is inserted, it needs to update setSize for the previous objects.
    requestUpdateToNextNode(false);
}

AXRadioInput* AXRadioInput::create(LayoutObject* layoutObject, AXObjectCacheImpl& axObjectCache)
{
    return new AXRadioInput(layoutObject, axObjectCache);
}

void AXRadioInput::updatePosAndSetSize(int position)
{
    if (position)
        m_posInSet = position;
    m_setSize = sizeOfRadioGroup();
}

void AXRadioInput::requestUpdateToNextNode(bool forward)
{
    HTMLInputElement* nextElement = RadioInputType::nextRadioButtonInGroup(element(), forward);
    AXObject* nextAXobject = axObjectCache().get(nextElement);
    if (!nextAXobject || !nextAXobject->isAXRadioInput())
        return;

    int position = 0;
    if (forward)
        position = posInSet() + 1;
    // If it is backward, it keeps position as positions are already assigned for previous objects.
    // updatePosAndSetSize() is called with '0' and it doesn't modify m_posInSet and updates m_setSize as size is increased.

    toAXRadioInput(nextAXobject)->updatePosAndSetSize(position);
    axObjectCache().postNotification(nextAXobject, AXObjectCacheImpl::AXAriaAttributeChanged);
    toAXRadioInput(nextAXobject)->requestUpdateToNextNode(forward);
}

HTMLInputElement* AXRadioInput::findFirstRadioButtonInGroup(HTMLInputElement* current) const
{
    while (HTMLInputElement* prevElement = RadioInputType::nextRadioButtonInGroup(current, false))
        current = prevElement;
    return current;
}

int AXRadioInput::posInSet() const
{
    if (hasAttribute(aria_posinsetAttr))
        return getAttribute(aria_posinsetAttr).toInt();
    return m_posInSet;
}

int AXRadioInput::setSize() const
{
    if (hasAttribute(aria_setsizeAttr))
        return getAttribute(aria_setsizeAttr).toInt();
    return m_setSize;
}

bool AXRadioInput::calculatePosInSet()
{
    // Calculate 'posInSet' attribute when AXRadioInputs need to be updated
    // as a new AXRadioInput Object is added or one of objects from RadioGroup is removed.
    bool needToUpdatePrev = false;
    int position = 1;
    HTMLInputElement* prevElement = RadioInputType::nextRadioButtonInGroup(element(), false);
    if (prevElement) {
        AXObject* object = axObjectCache().get(prevElement);
        // If the previous element doesn't have AXObject yet, caculate position from the first element.
        // Otherwise, get position from the previous AXObject.
        if (!object || !object->isAXRadioInput()) {
            position = countFromFirstElement();
        } else {
            position = object->posInSet() + 1;
            // It returns true if previous objects need to be updated.
            // When AX tree exists already and a new node is inserted,
            // as updating is started from the inserted node,
            // we need to update setSize for previous nodes.
            if (setSize() != object->setSize())
                needToUpdatePrev = true;
        }
    }
    updatePosAndSetSize(position);

    // If it is not the last element, request update to the next node.
    if (position != setSize())
        requestUpdateToNextNode(true);
    return needToUpdatePrev;
}

int AXRadioInput::countFromFirstElement() const
{
    int count = 1;
    HTMLInputElement* current = element();
    while (HTMLInputElement* prevElement = RadioInputType::nextRadioButtonInGroup(current, false)) {
        current = prevElement;
        count++;
    }
    return count;
}

HTMLInputElement* AXRadioInput::element() const
{
    return toHTMLInputElement(m_layoutObject->node());
}

int AXRadioInput::sizeOfRadioGroup() const
{
    int size = element()->sizeOfRadioGroup();
    // If it has no size in Group, it means that there is only itself.
    if (!size)
        return 1;
    return size;
}

} // namespace blink
