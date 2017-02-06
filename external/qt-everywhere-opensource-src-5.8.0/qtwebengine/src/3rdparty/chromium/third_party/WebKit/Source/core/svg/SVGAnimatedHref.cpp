// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGAnimatedHref.h"

#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGElement.h"

namespace blink {

SVGAnimatedHref* SVGAnimatedHref::create(SVGElement* contextElement)
{
    return new SVGAnimatedHref(contextElement);
}

DEFINE_TRACE(SVGAnimatedHref)
{
    visitor->trace(m_xlinkHref);
    SVGAnimatedString::trace(visitor);
}

SVGAnimatedHref::SVGAnimatedHref(SVGElement* contextElement)
    : SVGAnimatedString(contextElement, SVGNames::hrefAttr, SVGString::create())
    , m_xlinkHref(SVGAnimatedString::create(contextElement, XLinkNames::hrefAttr, SVGString::create()))
{
}

void SVGAnimatedHref::addToPropertyMap(SVGElement* element)
{
    element->addToPropertyMap(this);
    element->addToPropertyMap(m_xlinkHref);
}

bool SVGAnimatedHref::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName.matches(SVGNames::hrefAttr) || attrName.matches(XLinkNames::hrefAttr);
}

SVGString* SVGAnimatedHref::currentValue()
{
    return backingString()->SVGAnimatedString::currentValue();
}

const SVGString* SVGAnimatedHref::currentValue() const
{
    return backingString()->SVGAnimatedString::currentValue();
}

String SVGAnimatedHref::baseVal()
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefBaseVal);
    return backingString()->SVGAnimatedString::baseVal();
}

void SVGAnimatedHref::setBaseVal(const String& value, ExceptionState& exceptionState)
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefBaseVal);
    return backingString()->SVGAnimatedString::setBaseVal(value, exceptionState);
}

String SVGAnimatedHref::animVal()
{
    UseCounter::count(contextElement()->document(), UseCounter::SVGHrefAnimVal);
    return backingString()->SVGAnimatedString::animVal();
}

SVGAnimatedString* SVGAnimatedHref::backingString()
{
    return useXLink() ? m_xlinkHref.get() : this;
}

const SVGAnimatedString* SVGAnimatedHref::backingString() const
{
    return useXLink() ? m_xlinkHref.get() : this;
}

bool SVGAnimatedHref::useXLink() const
{
    return !SVGAnimatedString::isSpecified() && m_xlinkHref->isSpecified();
}

} // namespace blink
