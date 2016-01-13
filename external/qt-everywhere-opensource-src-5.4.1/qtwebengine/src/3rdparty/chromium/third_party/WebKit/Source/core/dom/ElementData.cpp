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

#include "config.h"
#include "core/dom/ElementData.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Attr.h"
#include "core/dom/QualifiedName.h"
#include "wtf/Vector.h"

namespace WebCore {

struct SameSizeAsElementData : public RefCounted<SameSizeAsElementData> {
    unsigned bitfield;
    void* refPtrs[3];
};

COMPILE_ASSERT(sizeof(ElementData) == sizeof(SameSizeAsElementData), element_attribute_data_should_stay_small);

static size_t sizeForShareableElementDataWithAttributeCount(unsigned count)
{
    return sizeof(ShareableElementData) + sizeof(Attribute) * count;
}

ElementData::ElementData()
    : m_isUnique(true)
    , m_arraySize(0)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

ElementData::ElementData(unsigned arraySize)
    : m_isUnique(false)
    , m_arraySize(arraySize)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

ElementData::ElementData(const ElementData& other, bool isUnique)
    : m_isUnique(isUnique)
    , m_arraySize(isUnique ? 0 : other.attributeCount())
    , m_presentationAttributeStyleIsDirty(other.m_presentationAttributeStyleIsDirty)
    , m_styleAttributeIsDirty(other.m_styleAttributeIsDirty)
    , m_animatedSVGAttributesAreDirty(other.m_animatedSVGAttributesAreDirty)
    , m_classNames(other.m_classNames)
    , m_idForStyleResolution(other.m_idForStyleResolution)
{
    // NOTE: The inline style is copied by the subclass copy constructor since we don't know what to do with it here.
}

void ElementData::destroy()
{
    if (m_isUnique)
        delete static_cast<UniqueElementData*>(this);
    else
        delete static_cast<ShareableElementData*>(this);
}

PassRefPtr<UniqueElementData> ElementData::makeUniqueCopy() const
{
    if (isUnique())
        return adoptRef(new UniqueElementData(static_cast<const UniqueElementData&>(*this)));
    return adoptRef(new UniqueElementData(static_cast<const ShareableElementData&>(*this)));
}

bool ElementData::isEquivalent(const ElementData* other) const
{
    if (!other)
        return !hasAttributes();

    AttributeCollection attributes = this->attributes();
    if (attributes.size() != other->attributeCount())
        return false;

    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
        const Attribute* otherAttr = other->findAttributeByName(it->name());
        if (!otherAttr || it->value() != otherAttr->value())
            return false;
    }
    return true;
}

size_t ElementData::findAttrNodeIndex(Attr* attr) const
{
    // This relies on the fact that Attr's QualifiedName == the Attribute's name.
    AttributeCollection attributes = this->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    unsigned index = 0;
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it, ++index) {
        if (it->name() == attr->qualifiedName())
            return index;
    }
    return kNotFound;
}

size_t ElementData::findAttributeIndexByNameSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    // Continue to checking case-insensitively and/or full namespaced names if necessary:
    AttributeCollection attributes = this->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    unsigned index = 0;
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it, ++index) {
        // FIXME: Why check the prefix? Namespace is all that should matter
        // and all HTML/SVG attributes have a null namespace!
        if (!it->name().hasPrefix()) {
            if (shouldIgnoreAttributeCase && equalIgnoringCase(name, it->localName()))
                return index;
        } else {
            // FIXME: Would be faster to do this comparison without calling toString, which
            // generates a temporary string by concatenation. But this branch is only reached
            // if the attribute name has a prefix, which is rare in HTML.
            if (equalPossiblyIgnoringCase(name, it->name().toString(), shouldIgnoreAttributeCase))
                return index;
        }
    }
    return kNotFound;
}

ShareableElementData::ShareableElementData(const Vector<Attribute>& attributes)
    : ElementData(attributes.size())
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(attributes[i]);
}

ShareableElementData::~ShareableElementData()
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        m_attributeArray[i].~Attribute();
}

ShareableElementData::ShareableElementData(const UniqueElementData& other)
    : ElementData(other, false)
{
    ASSERT(!other.m_presentationAttributeStyle);

    if (other.m_inlineStyle) {
        m_inlineStyle = other.m_inlineStyle->immutableCopyIfNeeded();
    }

    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(other.m_attributeVector.at(i));
}

PassRefPtr<ShareableElementData> ShareableElementData::createWithAttributes(const Vector<Attribute>& attributes)
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(attributes.size()));
    return adoptRef(new (slot) ShareableElementData(attributes));
}

UniqueElementData::UniqueElementData()
{
}

UniqueElementData::UniqueElementData(const UniqueElementData& other)
    : ElementData(other, true)
    , m_presentationAttributeStyle(other.m_presentationAttributeStyle)
    , m_attributeVector(other.m_attributeVector)
{
    m_inlineStyle = other.m_inlineStyle ? other.m_inlineStyle->mutableCopy() : nullptr;
}

UniqueElementData::UniqueElementData(const ShareableElementData& other)
    : ElementData(other, true)
{
    // An ShareableElementData should never have a mutable inline StylePropertySet attached.
    ASSERT(!other.m_inlineStyle || !other.m_inlineStyle->isMutable());
    m_inlineStyle = other.m_inlineStyle;

    unsigned length = other.attributeCount();
    m_attributeVector.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_attributeVector.uncheckedAppend(other.m_attributeArray[i]);
}

PassRefPtr<UniqueElementData> UniqueElementData::create()
{
    return adoptRef(new UniqueElementData);
}

PassRefPtr<ShareableElementData> UniqueElementData::makeShareableCopy() const
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(m_attributeVector.size()));
    return adoptRef(new (slot) ShareableElementData(*this));
}

Attribute* UniqueElementData::findAttributeByName(const QualifiedName& name)
{
    unsigned length = m_attributeVector.size();
    for (unsigned i = 0; i < length; ++i) {
        if (m_attributeVector.at(i).name().matches(name))
            return &m_attributeVector.at(i);
    }
    return 0;
}

} // namespace WebCore
