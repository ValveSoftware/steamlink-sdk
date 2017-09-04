/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGTests.h"

#include "core/SVGNames.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGStaticStringList.h"
#include "platform/Language.h"

namespace blink {

SVGTests::SVGTests(SVGElement* contextElement)
    : m_requiredFeatures(
          SVGStaticStringList::create(contextElement,
                                      SVGNames::requiredFeaturesAttr)),
      m_requiredExtensions(
          SVGStaticStringList::create(contextElement,
                                      SVGNames::requiredExtensionsAttr)),
      m_systemLanguage(
          SVGStaticStringList::create(contextElement,
                                      SVGNames::systemLanguageAttr)) {
  ASSERT(contextElement);

  contextElement->addToPropertyMap(m_requiredFeatures);
  contextElement->addToPropertyMap(m_requiredExtensions);
  contextElement->addToPropertyMap(m_systemLanguage);
}

DEFINE_TRACE(SVGTests) {
  visitor->trace(m_requiredFeatures);
  visitor->trace(m_requiredExtensions);
  visitor->trace(m_systemLanguage);
}

SVGStringListTearOff* SVGTests::requiredFeatures() {
  return m_requiredFeatures->tearOff();
}

SVGStringListTearOff* SVGTests::requiredExtensions() {
  return m_requiredExtensions->tearOff();
}

SVGStringListTearOff* SVGTests::systemLanguage() {
  return m_systemLanguage->tearOff();
}

bool SVGTests::isValid() const {
  // No need to check requiredFeatures since hasFeature always returns true.

  if (m_systemLanguage->isSpecified()) {
    bool matchFound = false;
    for (const auto& value : m_systemLanguage->value()->values()) {
      if (value.length() == 2 && defaultLanguage().startsWith(value)) {
        matchFound = true;
        break;
      }
    }
    if (!matchFound)
      return false;
  }

  if (!m_requiredExtensions->value()->values().isEmpty())
    return false;

  return true;
}

bool SVGTests::isKnownAttribute(const QualifiedName& attrName) {
  return attrName == SVGNames::requiredFeaturesAttr ||
         attrName == SVGNames::requiredExtensionsAttr ||
         attrName == SVGNames::systemLanguageAttr;
}

}  // namespace blink
