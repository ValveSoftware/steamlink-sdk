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

#include "config.h"

#include "core/svg/SVGLengthTearOff.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace WebCore {

namespace {

inline SVGLengthType toSVGLengthType(unsigned short type)
{
    ASSERT(type >= LengthTypeUnknown && type <= LengthTypePC);
    return static_cast<SVGLengthType>(type);
}

} // namespace

SVGLengthType SVGLengthTearOff::unitType()
{
    return target()->unitType();
}

SVGLengthMode SVGLengthTearOff::unitMode()
{
    return target()->unitMode();
}

float SVGLengthTearOff::value(ExceptionState& es)
{
    SVGLengthContext lengthContext(contextElement());
    return target()->value(lengthContext, es);
}

void SVGLengthTearOff::setValue(float value, ExceptionState& es)
{
    if (isImmutable()) {
        es.throwDOMException(NoModificationAllowedError, "The attribute is read-only.");
        return;
    }

    SVGLengthContext lengthContext(contextElement());
    target()->setValue(value, lengthContext, es);
    commitChange();
}

float SVGLengthTearOff::valueInSpecifiedUnits()
{
    return target()->valueInSpecifiedUnits();
}

void SVGLengthTearOff::setValueInSpecifiedUnits(float value, ExceptionState& es)
{
    if (isImmutable()) {
        es.throwDOMException(NoModificationAllowedError, "The attribute is read-only.");
        return;
    }
    target()->setValueInSpecifiedUnits(value);
    commitChange();
}

String SVGLengthTearOff::valueAsString()
{
    return target()->valueAsString();
}

void SVGLengthTearOff::setValueAsString(const String& str, ExceptionState& es)
{
    if (isImmutable()) {
        es.throwDOMException(NoModificationAllowedError, "The attribute is read-only.");
        return;
    }

    target()->setValueAsString(str, es);
    commitChange();
}

void SVGLengthTearOff::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits, ExceptionState& exceptionState)
{
    if (isImmutable()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
        return;
    }

    if (unitType == LengthTypeUnknown || unitType > LengthTypePC) {
        exceptionState.throwDOMException(NotSupportedError, "Cannot set value with unknown or invalid units (" + String::number(unitType) + ").");
        return;
    }

    target()->newValueSpecifiedUnits(toSVGLengthType(unitType), valueInSpecifiedUnits);
    commitChange();
}

void SVGLengthTearOff::convertToSpecifiedUnits(unsigned short unitType, ExceptionState& exceptionState)
{
    if (isImmutable()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The object is read-only.");
        return;
    }

    if (unitType == LengthTypeUnknown || unitType > LengthTypePC) {
        exceptionState.throwDOMException(NotSupportedError, "Cannot convert to unknown or invalid units (" + String::number(unitType) + ").");
        return;
    }

    SVGLengthContext lengthContext(contextElement());
    target()->convertToSpecifiedUnits(toSVGLengthType(unitType), lengthContext, exceptionState);
    commitChange();
}

SVGLengthTearOff::SVGLengthTearOff(PassRefPtr<SVGLength> target, SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName)
    : SVGPropertyTearOff<SVGLength>(target, contextElement, propertyIsAnimVal, attributeName)
{
    ScriptWrappable::init(this);
}

}
