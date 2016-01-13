/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef SVGRect_h
#define SVGRect_h

#include "core/svg/properties/SVGProperty.h"
#include "platform/geometry/FloatRect.h"

namespace WebCore {

class SVGRectTearOff;

class SVGRect : public SVGPropertyBase {
public:
    typedef SVGRectTearOff TearOffType;

    struct InvalidSVGRectTag { };

    static PassRefPtr<SVGRect> create()
    {
        return adoptRef(new SVGRect());
    }

    static PassRefPtr<SVGRect> create(InvalidSVGRectTag)
    {
        return adoptRef(new SVGRect(InvalidSVGRectTag()));
    }

    static PassRefPtr<SVGRect> create(const FloatRect& rect)
    {
        return adoptRef(new SVGRect(rect));
    }

    PassRefPtr<SVGRect> clone() const;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    const FloatRect& value() const { return m_value; }
    void setValue(const FloatRect& v) { m_value = v; }

    float x() const { return m_value.x(); }
    float y() const { return m_value.y(); }
    float width() const { return m_value.width(); }
    float height() const { return m_value.height(); }
    void setX(float f) { m_value.setX(f); }
    void setY(float f) { m_value.setY(f); }
    void setWidth(float f) { m_value.setWidth(f); }
    void setHeight(float f) { m_value.setHeight(f); }

    bool operator==(const SVGRect&) const;
    bool operator!=(const SVGRect& other) const { return !operator==(other); }

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement* contextElement) OVERRIDE;

    bool isValid() const { return m_isValid; }
    void setInvalid();

    static AnimatedPropertyType classType() { return AnimatedRect; }

private:
    SVGRect();
    SVGRect(InvalidSVGRectTag);
    SVGRect(const FloatRect&);

    template<typename CharType>
    void parse(const CharType*& ptr, const CharType* end, ExceptionState&);

    bool m_isValid;
    FloatRect m_value;
};

inline PassRefPtr<SVGRect> toSVGRect(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGRect::classType());
    return static_pointer_cast<SVGRect>(base.release());
}

} // namespace WebCore

#endif // SVGRect_h
