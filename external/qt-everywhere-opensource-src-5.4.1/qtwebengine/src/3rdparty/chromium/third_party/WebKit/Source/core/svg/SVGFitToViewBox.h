/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef SVGFitToViewBox_h
#define SVGFitToViewBox_h

#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGRect.h"
#include "wtf/HashSet.h"

namespace WebCore {

class AffineTransform;
class Document;

class SVGFitToViewBox {
public:
    enum PropertyMapPolicy {
        PropertyMapPolicyAdd,
        PropertyMapPolicySkip,
    };

    static AffineTransform viewBoxToViewTransform(const FloatRect& viewBoxRect, PassRefPtr<SVGPreserveAspectRatio>, float viewWidth, float viewHeight);

    static bool isKnownAttribute(const QualifiedName&);
    static void addSupportedAttributes(HashSet<QualifiedName>&);

    bool parseAttribute(const QualifiedName& name, const AtomicString& value, Document& document, SVGParsingError& parseError)
    {
        if (name == SVGNames::viewBoxAttr) {
            m_viewBox->setBaseValueAsString(value, parseError);
            if (m_viewBox->baseValue()->width() < 0.0f) {
                document.accessSVGExtensions().reportError("A negative value for ViewBox width is not allowed");
                m_viewBox->baseValue()->setInvalid();
            }
            if (m_viewBox->baseValue()->height() < 0.0f) {
                document.accessSVGExtensions().reportError("A negative value for ViewBox height is not allowed");
                m_viewBox->baseValue()->setInvalid();
            }
            return true;
        }
        if (name == SVGNames::preserveAspectRatioAttr) {
            m_preserveAspectRatio->setBaseValueAsString(value, parseError);
            return true;
        }
        return false;
    }

    bool hasEmptyViewBox() const { return m_viewBox->currentValue()->isValid() && m_viewBox->currentValue()->value().isEmpty(); }

    // JS API
    SVGAnimatedRect* viewBox() const { return m_viewBox.get(); }
    SVGAnimatedPreserveAspectRatio* preserveAspectRatio() const { return m_preserveAspectRatio.get(); }

protected:
    explicit SVGFitToViewBox(SVGElement*, PropertyMapPolicy = PropertyMapPolicyAdd);
    void updateViewBox(const FloatRect&);
    void clearViewBox() { m_viewBox = nullptr; }
    void clearPreserveAspectRatio() { m_preserveAspectRatio = nullptr; }

private:
    RefPtr<SVGAnimatedRect> m_viewBox;
    RefPtr<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;
};

} // namespace WebCore

#endif // SVGFitToViewBox_h
