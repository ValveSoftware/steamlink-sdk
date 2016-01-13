/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGPathSegList_h
#define SVGPathSegList_h

#include "core/svg/SVGPathByteStream.h"
#include "core/svg/SVGPathSeg.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGListPropertyHelper.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class SVGPathElement;
class SVGPathSegListTearOff;

class SVGPathSegList : public SVGListPropertyHelper<SVGPathSegList, SVGPathSeg> {
public:
    typedef void PrimitiveType;
    typedef SVGPathSeg ItemPropertyType;
    typedef SVGPathSegListTearOff TearOffType;
    typedef SVGListPropertyHelper<SVGPathSegList, SVGPathSeg> Base;

    static PassRefPtr<SVGPathSegList> create(SVGPathElement* contextElement, SVGPathSegRole role = PathSegUndefinedRole)
    {
        return adoptRef(new SVGPathSegList(contextElement, role));
    }

    virtual ~SVGPathSegList();

    const SVGPathByteStream* byteStream() const;
    void clearByteStream() { m_byteStream.clear(); }

    // SVGListPropertyHelper methods with |m_byteStream| sync:

    ItemPropertyType* at(size_t index)
    {
        updateListFromByteStream();
        return Base::at(index);
    }

    size_t length()
    {
        updateListFromByteStream();
        return Base::length();
    }

    bool isEmpty() const
    {
        if (m_listSyncedToByteStream)
            return Base::isEmpty();

        return !m_byteStream || m_byteStream->isEmpty();
    }

    void clear()
    {
        clearByteStream();
        Base::clear();
    }

    void append(PassRefPtr<ItemPropertyType> passNewItem)
    {
        updateListFromByteStream();
        clearByteStream();
        Base::append(passNewItem);
    }

    PassRefPtr<ItemPropertyType> initialize(PassRefPtr<ItemPropertyType> passItem)
    {
        clearByteStream();
        return Base::initialize(passItem);
    }

    PassRefPtr<ItemPropertyType> getItem(size_t index, ExceptionState& exceptionState)
    {
        updateListFromByteStream();
        return Base::getItem(index, exceptionState);
    }

    PassRefPtr<ItemPropertyType> insertItemBefore(PassRefPtr<ItemPropertyType> passItem, size_t index)
    {
        updateListFromByteStream();
        clearByteStream();
        return Base::insertItemBefore(passItem, index);
    }

    PassRefPtr<ItemPropertyType> replaceItem(PassRefPtr<ItemPropertyType> passItem, size_t index, ExceptionState& exceptionState)
    {
        updateListFromByteStream();
        clearByteStream();
        return Base::replaceItem(passItem, index, exceptionState);
    }

    PassRefPtr<ItemPropertyType> removeItem(size_t index, ExceptionState& exceptionState)
    {
        updateListFromByteStream();
        clearByteStream();
        return Base::removeItem(index, exceptionState);
    }

    PassRefPtr<ItemPropertyType> appendItem(PassRefPtr<ItemPropertyType> passItem);

    // SVGPropertyBase:
    PassRefPtr<SVGPathSegList> clone();
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;
    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> fromValue, PassRefPtr<SVGPropertyBase> toValue, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedPath; }

private:
    SVGPathSegList(SVGPathElement*, SVGPathSegRole);
    SVGPathSegList(SVGPathElement*, SVGPathSegRole, PassOwnPtr<SVGPathByteStream>);

    friend class SVGPathSegListBuilder;
    // This is only to be called from SVGPathSegListBuilder.
    void appendWithoutByteStreamSync(PassRefPtr<ItemPropertyType> passNewItem)
    {
        Base::append(passNewItem);
    }

    void updateListFromByteStream();
    void invalidateList();

    // FIXME: This pointer should be removed after SVGPathSeg has a tear-off.
    // FIXME: oilpan: This is raw-ptr to avoid reference cycles.
    //        SVGPathSegList is either owned by SVGAnimatedPath or
    //        SVGPathSegListTearOff. Both keep |contextElement| alive,
    //        so this ptr is always valid.
    SVGPathElement* m_contextElement;

    SVGPathSegRole m_role;
    mutable OwnPtr<SVGPathByteStream> m_byteStream;
    bool m_listSyncedToByteStream;
};

inline PassRefPtr<SVGPathSegList> toSVGPathSegList(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGPathSegList::classType());
    return static_pointer_cast<SVGPathSegList>(base.release());
}

} // namespace WebCore

#endif
