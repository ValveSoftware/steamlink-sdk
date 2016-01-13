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

#ifndef SVGEnumeration_h
#define SVGEnumeration_h

#include "core/svg/properties/SVGProperty.h"

namespace WebCore {

class SVGEnumerationBase : public SVGPropertyBase {
public:
    typedef std::pair<unsigned short, String> StringEntry;
    typedef Vector<StringEntry> StringEntries;

    // SVGEnumeration does not have a tear-off type.
    typedef void TearOffType;
    typedef unsigned short PrimitiveType;

    virtual ~SVGEnumerationBase();

    unsigned short value() const { return m_value; }
    void setValue(unsigned short, ExceptionState&);

    // This assumes that |m_entries| are sorted.
    unsigned short maxEnumValue() const { return m_entries.last().first; }

    // SVGPropertyBase:
    virtual PassRefPtr<SVGEnumerationBase> clone() const = 0;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedEnumeration; }

    // Ensure that |SVGAnimatedEnumerationBase::setBaseVal| is used instead of |SVGAnimatedProperty<SVGEnumerationBase>::setBaseVal|.
    void setValue(unsigned short) { ASSERT_NOT_REACHED(); }

protected:
    SVGEnumerationBase(unsigned short value, const StringEntries& entries)
        : SVGPropertyBase(classType())
        , m_value(value)
        , m_entries(entries)
    {
    }

    // Used by SVGMarkerOrientEnumeration.
    virtual void notifyChange() { }

    unsigned short m_value;
    const StringEntries& m_entries;
};
typedef SVGEnumerationBase::StringEntries SVGEnumerationStringEntries;

template<typename Enum> const SVGEnumerationStringEntries& getStaticStringEntries();

template<typename Enum>
class SVGEnumeration : public SVGEnumerationBase {
public:
    static PassRefPtr<SVGEnumeration<Enum> > create(Enum newValue)
    {
        return adoptRef(new SVGEnumeration<Enum>(newValue));
    }

    virtual ~SVGEnumeration()
    {
    }

    virtual PassRefPtr<SVGEnumerationBase> clone() const OVERRIDE
    {
        return create(enumValue());
    }

    Enum enumValue() const
    {
        ASSERT(m_value <= maxEnumValue());
        return static_cast<Enum>(m_value);
    }

    void setEnumValue(Enum value)
    {
        m_value = value;
        notifyChange();
    }

protected:
    explicit SVGEnumeration(Enum newValue)
        : SVGEnumerationBase(newValue, getStaticStringEntries<Enum>())
    {
    }
};

} // namespace WebCore

#endif // SVGEnumeration_h
