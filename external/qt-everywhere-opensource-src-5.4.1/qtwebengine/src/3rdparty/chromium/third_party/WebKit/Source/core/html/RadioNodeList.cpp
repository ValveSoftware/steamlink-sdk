/*
 * Copyright (c) 2012 Motorola Mobility, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA MOBILITY, INC. AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY, INC. OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/RadioNodeList.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/NodeRareData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLObjectElement.h"

namespace WebCore {

using namespace HTMLNames;

RadioNodeList::RadioNodeList(ContainerNode& rootNode, const AtomicString& name, CollectionType type)
    : LiveNodeList(rootNode, type, InvalidateForFormControls, isHTMLFormElement(rootNode) ? NodeListIsRootedAtDocument : NodeListIsRootedAtNode)
    , m_name(name)
    , m_onlyMatchImgElements(type == RadioImgNodeListType)
{
    ScriptWrappable::init(this);
}

RadioNodeList::~RadioNodeList()
{
#if !ENABLE(OILPAN)
    ownerNode().nodeLists()->removeCache(this, m_onlyMatchImgElements ? RadioImgNodeListType : RadioNodeListType, m_name);
#endif
}

static inline HTMLInputElement* toRadioButtonInputElement(Element& element)
{
    if (!isHTMLInputElement(element))
        return 0;
    HTMLInputElement& inputElement = toHTMLInputElement(element);
    if (!inputElement.isRadioButton() || inputElement.value().isEmpty())
        return 0;
    return &inputElement;
}

String RadioNodeList::value() const
{
    if (m_onlyMatchImgElements)
        return String();
    unsigned length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        const HTMLInputElement* inputElement = toRadioButtonInputElement(*item(i));
        if (!inputElement || !inputElement->checked())
            continue;
        return inputElement->value();
    }
    return String();
}

void RadioNodeList::setValue(const String& value)
{
    if (m_onlyMatchImgElements)
        return;
    unsigned length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        HTMLInputElement* inputElement = toRadioButtonInputElement(*item(i));
        if (!inputElement || inputElement->value() != value)
            continue;
        inputElement->setChecked(true);
        return;
    }
}

bool RadioNodeList::matchesByIdOrName(const Element& testElement) const
{
    return testElement.getIdAttribute() == m_name || testElement.getNameAttribute() == m_name;
}

bool RadioNodeList::checkElementMatchesRadioNodeListFilter(const Element& testElement) const
{
    ASSERT(!m_onlyMatchImgElements);
    ASSERT(isHTMLObjectElement(testElement) || testElement.isFormControlElement());
    if (isHTMLFormElement(ownerNode())) {
        HTMLFormElement* formElement = toHTMLElement(testElement).formOwner();
        if (!formElement || formElement != ownerNode())
            return false;
    }

    return matchesByIdOrName(testElement);
}

bool RadioNodeList::elementMatches(const Element& element) const
{
    if (m_onlyMatchImgElements) {
        if (!isHTMLImageElement(element))
            return false;

        if (toHTMLElement(element).formOwner() != ownerNode())
            return false;

        return matchesByIdOrName(element);
    }

    if (!isHTMLObjectElement(element) && !element.isFormControlElement())
        return false;

    if (isHTMLInputElement(element) && toHTMLInputElement(element).isImageButton())
        return false;

    return checkElementMatchesRadioNodeListFilter(element);
}

} // namespace
