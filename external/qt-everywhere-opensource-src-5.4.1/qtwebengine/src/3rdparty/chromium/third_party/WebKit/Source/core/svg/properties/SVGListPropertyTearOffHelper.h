/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGListPropertyTearOffHelper_h
#define SVGListPropertyTearOffHelper_h

#include "bindings/v8/ExceptionState.h"
#include "core/svg/properties/SVGPropertyTearOff.h"
#include "wtf/HashMap.h"
#include "wtf/TypeTraits.h"

namespace WebCore {

template<typename ItemProperty>
class ListItemPropertyTraits {
public:
    typedef ItemProperty ItemPropertyType;
    typedef typename ItemPropertyType::TearOffType ItemTearOffType;

    static PassRefPtr<ItemPropertyType> getValueForInsertionFromTearOff(PassRefPtr<ItemTearOffType> passNewItem, SVGElement* contextElement, const QualifiedName& attributeName)
    {
        RefPtr<ItemTearOffType> newItem = passNewItem;

        // |newItem| is immutable, OR
        // |newItem| belongs to a SVGElement, but it does not belong to an animated list
        // (for example: "textElement.x.baseVal.appendItem(rectElement.width.baseVal)")
        if (newItem->isImmutable()
            || (newItem->contextElement() && !newItem->target()->ownerList())) {
            // We have to copy the incoming |newItem|,
            // Otherwise we'll end up having two tearoffs that operate on the same SVGProperty. Consider the example below:
            // SVGRectElements SVGAnimatedLength 'width' property baseVal points to the same tear off object
            // that's inserted into SVGTextElements SVGAnimatedLengthList 'x'. textElement.x.baseVal.getItem(0).value += 150 would
            // mutate the rectElement width _and_ the textElement x list. That's obviously wrong, take care of that.
            return newItem->target()->clone();
        }

        newItem->attachToSVGElementAttribute(contextElement, attributeName);
        return newItem->target();
    }

    static PassRefPtr<ItemTearOffType> createTearOff(PassRefPtr<ItemPropertyType> value, SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName)
    {
        return ItemTearOffType::create(value, contextElement, propertyIsAnimVal, attributeName);
    }
};

template<typename Derived, typename ListProperty>
class SVGListPropertyTearOffHelper : public SVGPropertyTearOff<ListProperty> {
public:
    typedef ListProperty ListPropertyType;
    typedef typename ListPropertyType::ItemPropertyType ItemPropertyType;
    typedef typename ItemPropertyType::TearOffType ItemTearOffType;
    typedef ListItemPropertyTraits<ItemPropertyType> ItemTraits;

    // SVG*List DOM interface:

    // WebIDL requires "unsigned long" type instead of size_t.
    unsigned long length()
    {
        return toDerived()->target()->length();
    }

    void clear(ExceptionState& exceptionState)
    {
        if (toDerived()->isImmutable()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
            return;
        }

        toDerived()->target()->clear();
    }

    PassRefPtr<ItemTearOffType> initialize(PassRefPtr<ItemTearOffType> passItem, ExceptionState& exceptionState)
    {
        RefPtr<ItemTearOffType> item = passItem;

        if (toDerived()->isImmutable()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
            return nullptr;
        }

        if (!item) {
            exceptionState.throwTypeError("Lists must be initialized with a valid item.");
            return nullptr;
        }

        RefPtr<ItemPropertyType> value = toDerived()->target()->initialize(getValueForInsertionFromTearOff(item));
        toDerived()->commitChange();

        return createItemTearOff(value.release());
    }

    PassRefPtr<ItemTearOffType> getItem(unsigned long index, ExceptionState& exceptionState)
    {
        RefPtr<ItemPropertyType> value = toDerived()->target()->getItem(index, exceptionState);
        return createItemTearOff(value.release());
    }

    PassRefPtr<ItemTearOffType> insertItemBefore(PassRefPtr<ItemTearOffType> passItem, unsigned long index, ExceptionState& exceptionState)
    {
        RefPtr<ItemTearOffType> item = passItem;

        if (toDerived()->isImmutable()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
            return nullptr;
        }

        if (!item) {
            exceptionState.throwTypeError("An invalid item cannot be inserted to a list.");
            return nullptr;
        }

        RefPtr<ItemPropertyType> value = toDerived()->target()->insertItemBefore(getValueForInsertionFromTearOff(item), index);
        toDerived()->commitChange();

        return createItemTearOff(value.release());
    }

    PassRefPtr<ItemTearOffType> replaceItem(PassRefPtr<ItemTearOffType> passItem, unsigned long index, ExceptionState& exceptionState)
    {
        RefPtr<ItemTearOffType> item = passItem;

        if (toDerived()->isImmutable()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
            return nullptr;
        }

        if (!item) {
            exceptionState.throwTypeError("An invalid item cannot be replaced with an existing list item.");
            return nullptr;
        }

        RefPtr<ItemPropertyType> value = toDerived()->target()->replaceItem(getValueForInsertionFromTearOff(item), index, exceptionState);
        toDerived()->commitChange();

        return createItemTearOff(value.release());
    }

    bool anonymousIndexedSetter(unsigned index, PassRefPtr<ItemTearOffType> passItem, ExceptionState& exceptionState)
    {
        replaceItem(passItem, index, exceptionState);
        return true;
    }

    PassRefPtr<ItemTearOffType> removeItem(unsigned long index, ExceptionState& exceptionState)
    {
        RefPtr<ItemPropertyType> value = toDerived()->target()->removeItem(index, exceptionState);
        toDerived()->commitChange();

        return createItemTearOff(value.release());
    }

    PassRefPtr<ItemTearOffType> appendItem(PassRefPtr<ItemTearOffType> passItem, ExceptionState& exceptionState)
    {
        RefPtr<ItemTearOffType> item = passItem;

        if (toDerived()->isImmutable()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
            return nullptr;
        }

        if (!item) {
            exceptionState.throwTypeError("An invalid item cannot be appended to a list.");
            return nullptr;
        }

        RefPtr<ItemPropertyType> value = toDerived()->target()->appendItem(getValueForInsertionFromTearOff(item));
        toDerived()->commitChange();

        return createItemTearOff(value.release());
    }

protected:
    SVGListPropertyTearOffHelper(PassRefPtr<ListPropertyType> target, SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName = QualifiedName::null())
        : SVGPropertyTearOff<ListPropertyType>(target, contextElement, propertyIsAnimVal, attributeName)
    {
    }

    PassRefPtr<ItemPropertyType> getValueForInsertionFromTearOff(PassRefPtr<ItemTearOffType> passNewItem)
    {
        return ItemTraits::getValueForInsertionFromTearOff(passNewItem, toDerived()->contextElement(), toDerived()->attributeName());
    }

    PassRefPtr<ItemTearOffType> createItemTearOff(PassRefPtr<ItemPropertyType> value)
    {
        if (!value)
            return nullptr;

        if (value->ownerList() == toDerived()->target())
            return ItemTraits::createTearOff(value, toDerived()->contextElement(), toDerived()->propertyIsAnimVal(), toDerived()->attributeName());

        return ItemTraits::createTearOff(value, 0, PropertyIsNotAnimVal, QualifiedName::null());
    }

private:
    Derived* toDerived() { return static_cast<Derived*>(this); }
};

}

#endif // SVGListPropertyTearOffHelper_h
