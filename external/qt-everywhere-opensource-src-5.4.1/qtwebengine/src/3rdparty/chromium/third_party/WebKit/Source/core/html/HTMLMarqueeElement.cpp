/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2007, 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/html/HTMLMarqueeElement.h"

#include "bindings/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/rendering/RenderMarquee.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLMarqueeElement::HTMLMarqueeElement(Document& document)
    : HTMLElement(marqueeTag, document)
    , ActiveDOMObject(&document)
{
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<HTMLMarqueeElement> HTMLMarqueeElement::create(Document& document)
{
    RefPtrWillBeRawPtr<HTMLMarqueeElement> marqueeElement(adoptRefWillBeNoop(new HTMLMarqueeElement(document)));
    marqueeElement->suspendIfNeeded();
    return marqueeElement.release();
}

int HTMLMarqueeElement::minimumDelay() const
{
    if (fastGetAttribute(truespeedAttr).isEmpty()) {
        // WinIE uses 60ms as the minimum delay by default.
        return 60;
    }
    return 0;
}

void HTMLMarqueeElement::didMoveToNewDocument(Document& oldDocument)
{
    ActiveDOMObject::didMoveToNewExecutionContext(&document());
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLMarqueeElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == bgcolorAttr || name == vspaceAttr || name == hspaceAttr || name == scrollamountAttr || name == scrolldelayAttr || name == loopAttr || name == behaviorAttr || name == directionAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLMarqueeElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else if (name == bgcolorAttr) {
        if (!value.isEmpty())
            addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    } else if (name == vspaceAttr) {
        if (!value.isEmpty()) {
            addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
            addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        }
    } else if (name == hspaceAttr) {
        if (!value.isEmpty()) {
            addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
            addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        }
    } else if (name == scrollamountAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyInternalMarqueeIncrement, value);
    } else if (name == scrolldelayAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyInternalMarqueeSpeed, value);
    } else if (name == loopAttr) {
        if (!value.isEmpty()) {
            if (value == "-1" || equalIgnoringCase(value, "infinite"))
                addPropertyToPresentationAttributeStyle(style, CSSPropertyInternalMarqueeRepetition, CSSValueInfinite);
            else
                addHTMLLengthToStyle(style, CSSPropertyInternalMarqueeRepetition, value);
        }
    } else if (name == behaviorAttr) {
        if (!value.isEmpty())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyInternalMarqueeStyle, value);
    } else if (name == directionAttr) {
        if (!value.isEmpty())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyInternalMarqueeDirection, value);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLMarqueeElement::start()
{
    if (RenderMarquee* marqueeRenderer = renderMarquee())
        marqueeRenderer->start();
}

void HTMLMarqueeElement::stop()
{
    if (RenderMarquee* marqueeRenderer = renderMarquee())
        marqueeRenderer->stop();
}

int HTMLMarqueeElement::scrollAmount() const
{
    bool ok;
    int scrollAmount = fastGetAttribute(scrollamountAttr).toInt(&ok);
    return ok && scrollAmount >= 0 ? scrollAmount : RenderStyle::initialMarqueeIncrement().intValue();
}

void HTMLMarqueeElement::setScrollAmount(int scrollAmount, ExceptionState& exceptionState)
{
    if (scrollAmount < 0)
        exceptionState.throwDOMException(IndexSizeError, "The provided value (" + String::number(scrollAmount) + ") is negative.");
    else
        setIntegralAttribute(scrollamountAttr, scrollAmount);
}

int HTMLMarqueeElement::scrollDelay() const
{
    bool ok;
    int scrollDelay = fastGetAttribute(scrolldelayAttr).toInt(&ok);
    return ok && scrollDelay >= 0 ? scrollDelay : RenderStyle::initialMarqueeSpeed();
}

void HTMLMarqueeElement::setScrollDelay(int scrollDelay, ExceptionState& exceptionState)
{
    if (scrollDelay < 0)
        exceptionState.throwDOMException(IndexSizeError, "The provided value (" + String::number(scrollDelay) + ") is negative.");
    else
        setIntegralAttribute(scrolldelayAttr, scrollDelay);
}

int HTMLMarqueeElement::loop() const
{
    bool ok;
    int loopValue = fastGetAttribute(loopAttr).toInt(&ok);
    return ok && loopValue > 0 ? loopValue : -1;
}

void HTMLMarqueeElement::setLoop(int loop, ExceptionState& exceptionState)
{
    if (loop <= 0 && loop != -1)
        exceptionState.throwDOMException(IndexSizeError, "The provided value (" + String::number(loop) + ") is neither positive nor -1.");
    else
        setIntegralAttribute(loopAttr, loop);
}

void HTMLMarqueeElement::suspend()
{
    if (RenderMarquee* marqueeRenderer = renderMarquee())
        marqueeRenderer->suspend();
}

void HTMLMarqueeElement::resume()
{
    if (RenderMarquee* marqueeRenderer = renderMarquee())
        marqueeRenderer->updateMarqueePosition();
}

RenderMarquee* HTMLMarqueeElement::renderMarquee() const
{
    if (renderer() && renderer()->isMarquee())
        return toRenderMarquee(renderer());
    return 0;
}

RenderObject* HTMLMarqueeElement::createRenderer(RenderStyle*)
{
    return new RenderMarquee(this);
}

void HTMLMarqueeElement::timerFired(Timer<HTMLMarqueeElement>*)
{
    if (!renderer())
        return;

    document().updateLayout();

    // The updateLayout() could have destroyed renderer(), so this re-check is very important.
    if (renderer())
        toRenderMarquee(renderer())->timerFired();
}

} // namespace WebCore
