/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
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

#ifndef SVGTests_h
#define SVGTests_h

#include "core/svg/SVGStaticStringList.h"
#include "wtf/HashSet.h"

namespace WebCore {

class QualifiedName;
class SVGElement;

class SVGTests {
public:
    // JS API
    SVGStringListTearOff* requiredFeatures() { return m_requiredFeatures->tearOff(); }
    SVGStringListTearOff* requiredExtensions() { return m_requiredExtensions->tearOff(); }
    SVGStringListTearOff* systemLanguage() { return m_systemLanguage->tearOff(); }
    bool hasExtension(const String&);

    bool isValid() const;

    bool parseAttribute(const QualifiedName&, const AtomicString&);
    bool isKnownAttribute(const QualifiedName&);

    void addSupportedAttributes(HashSet<QualifiedName>&);

protected:
    SVGTests(SVGElement* contextElement);

private:
    RefPtr<SVGStaticStringList> m_requiredFeatures;
    RefPtr<SVGStaticStringList> m_requiredExtensions;
    RefPtr<SVGStaticStringList> m_systemLanguage;
};

} // namespace WebCore

#endif // SVGTests_h
