/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
#include "core/html/HTMLElement.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/XMLNames.h"
#include "core/css/CSSMarkup.h"
#include "core/css/CSSValuePool.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/markup.h"
#include "core/events/EventListener.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderObject.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/BidiTextRun.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

namespace WebCore {

using namespace HTMLNames;
using namespace WTF;

using std::min;
using std::max;

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLElement);

String HTMLElement::nodeName() const
{
    // FIXME: Would be nice to have an atomicstring lookup based off uppercase
    // chars that does not have to copy the string on a hit in the hash.
    // FIXME: We should have a way to detect XHTML elements and replace the hasPrefix() check with it.
    if (document().isHTMLDocument()) {
        if (!tagQName().hasPrefix())
            return tagQName().localNameUpper();
        return Element::nodeName().upper();
    }
    return Element::nodeName();
}

bool HTMLElement::ieForbidsInsertHTML() const
{
    // FIXME: Supposedly IE disallows settting innerHTML, outerHTML
    // and createContextualFragment on these tags.  We have no tests to
    // verify this however, so this list could be totally wrong.
    // This list was moved from the previous endTagRequirement() implementation.
    // This is also called from editing and assumed to be the list of tags
    // for which no end tag should be serialized. It's unclear if the list for
    // IE compat and the list for serialization sanity are the same.
    if (hasLocalName(areaTag)
        || hasLocalName(baseTag)
        || hasLocalName(basefontTag)
        || hasLocalName(brTag)
        || hasLocalName(colTag)
        || hasLocalName(embedTag)
        || hasLocalName(frameTag)
        || hasLocalName(hrTag)
        || hasLocalName(imageTag)
        || hasLocalName(imgTag)
        || hasLocalName(inputTag)
        || hasLocalName(linkTag)
        || hasLocalName(metaTag)
        || hasLocalName(paramTag)
        || hasLocalName(sourceTag)
        || hasLocalName(wbrTag))
        return true;
    return false;
}

static inline CSSValueID unicodeBidiAttributeForDirAuto(HTMLElement* element)
{
    if (element->hasLocalName(preTag) || element->hasLocalName(textareaTag))
        return CSSValueWebkitPlaintext;
    // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate" but we don't support having multiple values in unicode-bidi yet.
    // See https://bugs.webkit.org/show_bug.cgi?id=73164.
    return CSSValueWebkitIsolate;
}

unsigned HTMLElement::parseBorderWidthAttribute(const AtomicString& value) const
{
    unsigned borderWidth = 0;
    if (value.isEmpty() || !parseHTMLNonNegativeInteger(value, borderWidth))
        return hasLocalName(tableTag) ? 1 : borderWidth;
    return borderWidth;
}

void HTMLElement::applyBorderAttributeToStyle(const AtomicString& value, MutableStylePropertySet* style)
{
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSPrimitiveValue::CSS_PX);
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
}

void HTMLElement::mapLanguageAttributeToLocale(const AtomicString& value, MutableStylePropertySet* style)
{
    if (!value.isEmpty()) {
        // Have to quote so the locale id is treated as a string instead of as a CSS keyword.
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale, quoteCSSString(value));
    } else {
        // The empty string means the language is explicitly unknown.
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale, CSSValueAuto);
    }
}

bool HTMLElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr || name == contenteditableAttr || name == hiddenAttr || name == langAttr || name.matches(XMLNames::langAttr) || name == draggableAttr || name == dirAttr)
        return true;
    return Element::isPresentationAttribute(name);
}

static inline bool isValidDirAttribute(const AtomicString& value)
{
    return equalIgnoringCase(value, "auto") || equalIgnoringCase(value, "ltr") || equalIgnoringCase(value, "rtl");
}

void HTMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == alignAttr) {
        if (equalIgnoringCase(value, "middle"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, value);
    } else if (name == contenteditableAttr) {
        if (value.isEmpty() || equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadWrite);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak, CSSValueAfterWhiteSpace);
        } else if (equalIgnoringCase(value, "plaintext-only")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadWritePlaintextOnly);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak, CSSValueAfterWhiteSpace);
        } else if (equalIgnoringCase(value, "false"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadOnly);
    } else if (name == hiddenAttr) {
        addPropertyToPresentationAttributeStyle(style, CSSPropertyDisplay, CSSValueNone);
    } else if (name == draggableAttr) {
        if (equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueElement);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserSelect, CSSValueNone);
        } else if (equalIgnoringCase(value, "false"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueNone);
    } else if (name == dirAttr) {
        if (equalIgnoringCase(value, "auto"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyUnicodeBidi, unicodeBidiAttributeForDirAuto(this));
        else {
            if (isValidDirAttribute(value))
                addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, value);
            else
                addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, "ltr");
            if (!hasTagName(bdiTag) && !hasTagName(bdoTag) && !hasTagName(outputTag))
                addPropertyToPresentationAttributeStyle(style, CSSPropertyUnicodeBidi, CSSValueEmbed);
        }
    } else if (name.matches(XMLNames::langAttr))
        mapLanguageAttributeToLocale(value, style);
    else if (name == langAttr) {
        // xml:lang has a higher priority than lang.
        if (!fastHasAttribute(XMLNames::langAttr))
            mapLanguageAttributeToLocale(value, style);
    } else
        Element::collectStyleForPresentationAttribute(name, value, style);
}

const AtomicString& HTMLElement::eventNameForAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return nullAtom;

    if (!attrName.localName().startsWith("on", false))
        return nullAtom;

    typedef HashMap<AtomicString, AtomicString> StringToStringMap;
    DEFINE_STATIC_LOCAL(StringToStringMap, attributeNameToEventNameMap, ());
    if (!attributeNameToEventNameMap.size()) {
        attributeNameToEventNameMap.set(onabortAttr.localName(), EventTypeNames::abort);
        attributeNameToEventNameMap.set(onanimationendAttr.localName(), EventTypeNames::animationend);
        attributeNameToEventNameMap.set(onanimationiterationAttr.localName(), EventTypeNames::animationiteration);
        attributeNameToEventNameMap.set(onanimationstartAttr.localName(), EventTypeNames::animationstart);
        attributeNameToEventNameMap.set(onautocompleteAttr.localName(), EventTypeNames::autocomplete);
        attributeNameToEventNameMap.set(onautocompleteerrorAttr.localName(), EventTypeNames::autocompleteerror);
        attributeNameToEventNameMap.set(onbeforecopyAttr.localName(), EventTypeNames::beforecopy);
        attributeNameToEventNameMap.set(onbeforecutAttr.localName(), EventTypeNames::beforecut);
        attributeNameToEventNameMap.set(onbeforepasteAttr.localName(), EventTypeNames::beforepaste);
        attributeNameToEventNameMap.set(onblurAttr.localName(), EventTypeNames::blur);
        attributeNameToEventNameMap.set(oncancelAttr.localName(), EventTypeNames::cancel);
        attributeNameToEventNameMap.set(oncanplayAttr.localName(), EventTypeNames::canplay);
        attributeNameToEventNameMap.set(oncanplaythroughAttr.localName(), EventTypeNames::canplaythrough);
        attributeNameToEventNameMap.set(onchangeAttr.localName(), EventTypeNames::change);
        attributeNameToEventNameMap.set(onclickAttr.localName(), EventTypeNames::click);
        attributeNameToEventNameMap.set(oncloseAttr.localName(), EventTypeNames::close);
        attributeNameToEventNameMap.set(oncontextmenuAttr.localName(), EventTypeNames::contextmenu);
        attributeNameToEventNameMap.set(oncopyAttr.localName(), EventTypeNames::copy);
        attributeNameToEventNameMap.set(oncuechangeAttr.localName(), EventTypeNames::cuechange);
        attributeNameToEventNameMap.set(oncutAttr.localName(), EventTypeNames::cut);
        attributeNameToEventNameMap.set(ondblclickAttr.localName(), EventTypeNames::dblclick);
        attributeNameToEventNameMap.set(ondragAttr.localName(), EventTypeNames::drag);
        attributeNameToEventNameMap.set(ondragendAttr.localName(), EventTypeNames::dragend);
        attributeNameToEventNameMap.set(ondragenterAttr.localName(), EventTypeNames::dragenter);
        attributeNameToEventNameMap.set(ondragleaveAttr.localName(), EventTypeNames::dragleave);
        attributeNameToEventNameMap.set(ondragoverAttr.localName(), EventTypeNames::dragover);
        attributeNameToEventNameMap.set(ondragstartAttr.localName(), EventTypeNames::dragstart);
        attributeNameToEventNameMap.set(ondropAttr.localName(), EventTypeNames::drop);
        attributeNameToEventNameMap.set(ondurationchangeAttr.localName(), EventTypeNames::durationchange);
        attributeNameToEventNameMap.set(onemptiedAttr.localName(), EventTypeNames::emptied);
        attributeNameToEventNameMap.set(onendedAttr.localName(), EventTypeNames::ended);
        attributeNameToEventNameMap.set(onerrorAttr.localName(), EventTypeNames::error);
        attributeNameToEventNameMap.set(onfocusAttr.localName(), EventTypeNames::focus);
        attributeNameToEventNameMap.set(onfocusinAttr.localName(), EventTypeNames::focusin);
        attributeNameToEventNameMap.set(onfocusoutAttr.localName(), EventTypeNames::focusout);
        attributeNameToEventNameMap.set(oninputAttr.localName(), EventTypeNames::input);
        attributeNameToEventNameMap.set(oninvalidAttr.localName(), EventTypeNames::invalid);
        attributeNameToEventNameMap.set(onkeydownAttr.localName(), EventTypeNames::keydown);
        attributeNameToEventNameMap.set(onkeypressAttr.localName(), EventTypeNames::keypress);
        attributeNameToEventNameMap.set(onkeyupAttr.localName(), EventTypeNames::keyup);
        attributeNameToEventNameMap.set(onloadAttr.localName(), EventTypeNames::load);
        attributeNameToEventNameMap.set(onloadeddataAttr.localName(), EventTypeNames::loadeddata);
        attributeNameToEventNameMap.set(onloadedmetadataAttr.localName(), EventTypeNames::loadedmetadata);
        attributeNameToEventNameMap.set(onloadstartAttr.localName(), EventTypeNames::loadstart);
        attributeNameToEventNameMap.set(onmousedownAttr.localName(), EventTypeNames::mousedown);
        attributeNameToEventNameMap.set(onmouseenterAttr.localName(), EventTypeNames::mouseenter);
        attributeNameToEventNameMap.set(onmouseleaveAttr.localName(), EventTypeNames::mouseleave);
        attributeNameToEventNameMap.set(onmousemoveAttr.localName(), EventTypeNames::mousemove);
        attributeNameToEventNameMap.set(onmouseoutAttr.localName(), EventTypeNames::mouseout);
        attributeNameToEventNameMap.set(onmouseoverAttr.localName(), EventTypeNames::mouseover);
        attributeNameToEventNameMap.set(onmouseupAttr.localName(), EventTypeNames::mouseup);
        attributeNameToEventNameMap.set(onmousewheelAttr.localName(), EventTypeNames::mousewheel);
        attributeNameToEventNameMap.set(onpasteAttr.localName(), EventTypeNames::paste);
        attributeNameToEventNameMap.set(onpauseAttr.localName(), EventTypeNames::pause);
        attributeNameToEventNameMap.set(onplayAttr.localName(), EventTypeNames::play);
        attributeNameToEventNameMap.set(onplayingAttr.localName(), EventTypeNames::playing);
        attributeNameToEventNameMap.set(onprogressAttr.localName(), EventTypeNames::progress);
        attributeNameToEventNameMap.set(onratechangeAttr.localName(), EventTypeNames::ratechange);
        attributeNameToEventNameMap.set(onresetAttr.localName(), EventTypeNames::reset);
        attributeNameToEventNameMap.set(onresizeAttr.localName(), EventTypeNames::resize);
        attributeNameToEventNameMap.set(onscrollAttr.localName(), EventTypeNames::scroll);
        attributeNameToEventNameMap.set(onseekedAttr.localName(), EventTypeNames::seeked);
        attributeNameToEventNameMap.set(onseekingAttr.localName(), EventTypeNames::seeking);
        attributeNameToEventNameMap.set(onselectAttr.localName(), EventTypeNames::select);
        attributeNameToEventNameMap.set(onselectstartAttr.localName(), EventTypeNames::selectstart);
        attributeNameToEventNameMap.set(onshowAttr.localName(), EventTypeNames::show);
        attributeNameToEventNameMap.set(onstalledAttr.localName(), EventTypeNames::stalled);
        attributeNameToEventNameMap.set(onsubmitAttr.localName(), EventTypeNames::submit);
        attributeNameToEventNameMap.set(onsuspendAttr.localName(), EventTypeNames::suspend);
        attributeNameToEventNameMap.set(ontimeupdateAttr.localName(), EventTypeNames::timeupdate);
        attributeNameToEventNameMap.set(ontoggleAttr.localName(), EventTypeNames::toggle);
        attributeNameToEventNameMap.set(ontouchcancelAttr.localName(), EventTypeNames::touchcancel);
        attributeNameToEventNameMap.set(ontouchendAttr.localName(), EventTypeNames::touchend);
        attributeNameToEventNameMap.set(ontouchmoveAttr.localName(), EventTypeNames::touchmove);
        attributeNameToEventNameMap.set(ontouchstartAttr.localName(), EventTypeNames::touchstart);
        attributeNameToEventNameMap.set(ontransitionendAttr.localName(), EventTypeNames::webkitTransitionEnd);
        attributeNameToEventNameMap.set(onvolumechangeAttr.localName(), EventTypeNames::volumechange);
        attributeNameToEventNameMap.set(onwaitingAttr.localName(), EventTypeNames::waiting);
        attributeNameToEventNameMap.set(onwebkitanimationendAttr.localName(), EventTypeNames::webkitAnimationEnd);
        attributeNameToEventNameMap.set(onwebkitanimationiterationAttr.localName(), EventTypeNames::webkitAnimationIteration);
        attributeNameToEventNameMap.set(onwebkitanimationstartAttr.localName(), EventTypeNames::webkitAnimationStart);
        attributeNameToEventNameMap.set(onwebkitfullscreenchangeAttr.localName(), EventTypeNames::webkitfullscreenchange);
        attributeNameToEventNameMap.set(onwebkitfullscreenerrorAttr.localName(), EventTypeNames::webkitfullscreenerror);
        attributeNameToEventNameMap.set(onwebkittransitionendAttr.localName(), EventTypeNames::webkitTransitionEnd);
        attributeNameToEventNameMap.set(onwheelAttr.localName(), EventTypeNames::wheel);
    }

    return attributeNameToEventNameMap.get(attrName.localName());
}

void HTMLElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == tabindexAttr)
        return Element::parseAttribute(name, value);

    if (name == dirAttr) {
        dirAttributeChanged(value);
    } else {
        const AtomicString& eventName = eventNameForAttributeName(name);
        if (!eventName.isNull())
            setAttributeEventListener(eventName, createAttributeEventListener(this, name, value, eventParameterName()));
    }
}

PassRefPtrWillBeRawPtr<DocumentFragment> HTMLElement::textToFragment(const String& text, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<DocumentFragment> fragment = DocumentFragment::create(document());
    unsigned i, length = text.length();
    UChar c = 0;
    for (unsigned start = 0; start < length; ) {

        // Find next line break.
        for (i = start; i < length; i++) {
          c = text[i];
          if (c == '\r' || c == '\n')
              break;
        }

        fragment->appendChild(Text::create(document(), text.substring(start, i - start)), exceptionState);
        if (exceptionState.hadException())
            return nullptr;

        if (c == '\r' || c == '\n') {
            fragment->appendChild(HTMLBRElement::create(document()), exceptionState);
            if (exceptionState.hadException())
                return nullptr;
            // Make sure \r\n doesn't result in two line breaks.
            if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
                i++;
        }

        start = i + 1; // Character after line break.
    }

    return fragment;
}

void HTMLElement::setInnerText(const String& text, ExceptionState& exceptionState)
{
    if (ieForbidsInsertHTML()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The '" + localName() + "' element does not support text insertion.");
        return;
    }
    if (hasLocalName(colTag) || hasLocalName(colgroupTag) || hasLocalName(framesetTag) ||
        hasLocalName(headTag) || hasLocalName(htmlTag) || hasLocalName(tableTag) ||
        hasLocalName(tbodyTag) || hasLocalName(tfootTag) || hasLocalName(theadTag) ||
        hasLocalName(trTag)) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The '" + localName() + "' element does not support text insertion.");
        return;
    }

    // FIXME: This doesn't take whitespace collapsing into account at all.

    if (!text.contains('\n') && !text.contains('\r')) {
        if (text.isEmpty()) {
            removeChildren();
            return;
        }
        replaceChildrenWithText(this, text, exceptionState);
        return;
    }

    // FIXME: Do we need to be able to detect preserveNewline style even when there's no renderer?
    // FIXME: Can the renderer be out of date here? Do we need to call updateStyleIfNeeded?
    // For example, for the contents of textarea elements that are display:none?
    RenderObject* r = renderer();
    if (r && r->style()->preserveNewline()) {
        if (!text.contains('\r')) {
            replaceChildrenWithText(this, text, exceptionState);
            return;
        }
        String textWithConsistentLineBreaks = text;
        textWithConsistentLineBreaks.replace("\r\n", "\n");
        textWithConsistentLineBreaks.replace('\r', '\n');
        replaceChildrenWithText(this, textWithConsistentLineBreaks, exceptionState);
        return;
    }

    // Add text nodes and <br> elements.
    RefPtrWillBeRawPtr<DocumentFragment> fragment = textToFragment(text, exceptionState);
    if (!exceptionState.hadException())
        replaceChildrenWithFragment(this, fragment.release(), exceptionState);
}

void HTMLElement::setOuterText(const String &text, ExceptionState& exceptionState)
{
    if (ieForbidsInsertHTML()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The '" + localName() + "' element does not support text insertion.");
        return;
    }
    if (hasLocalName(colTag) || hasLocalName(colgroupTag) || hasLocalName(framesetTag) ||
        hasLocalName(headTag) || hasLocalName(htmlTag) || hasLocalName(tableTag) ||
        hasLocalName(tbodyTag) || hasLocalName(tfootTag) || hasLocalName(theadTag) ||
        hasLocalName(trTag)) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The '" + localName() + "' element does not support text insertion.");
        return;
    }

    ContainerNode* parent = parentNode();
    if (!parent) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The element has no parent.");
        return;
    }

    RefPtrWillBeRawPtr<Node> prev = previousSibling();
    RefPtrWillBeRawPtr<Node> next = nextSibling();
    RefPtrWillBeRawPtr<Node> newChild = nullptr;

    // Convert text to fragment with <br> tags instead of linebreaks if needed.
    if (text.contains('\r') || text.contains('\n'))
        newChild = textToFragment(text, exceptionState);
    else
        newChild = Text::create(document(), text);

    // textToFragment might cause mutation events.
    if (!parentNode())
        exceptionState.throwDOMException(HierarchyRequestError, "The element has no parent.");

    if (exceptionState.hadException())
        return;

    parent->replaceChild(newChild.release(), this, exceptionState);

    RefPtrWillBeRawPtr<Node> node = next ? next->previousSibling() : nullptr;
    if (!exceptionState.hadException() && node && node->isTextNode())
        mergeWithNextTextNode(node.release(), exceptionState);

    if (!exceptionState.hadException() && prev && prev->isTextNode())
        mergeWithNextTextNode(prev.release(), exceptionState);
}

void HTMLElement::applyAlignmentAttributeToStyle(const AtomicString& alignment, MutableStylePropertySet* style)
{
    // Vertical alignment with respect to the current baseline of the text
    // right or left means floating images.
    CSSValueID floatValue = CSSValueInvalid;
    CSSValueID verticalAlignValue = CSSValueInvalid;

    if (equalIgnoringCase(alignment, "absmiddle"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalIgnoringCase(alignment, "absbottom"))
        verticalAlignValue = CSSValueBottom;
    else if (equalIgnoringCase(alignment, "left")) {
        floatValue = CSSValueLeft;
        verticalAlignValue = CSSValueTop;
    } else if (equalIgnoringCase(alignment, "right")) {
        floatValue = CSSValueRight;
        verticalAlignValue = CSSValueTop;
    } else if (equalIgnoringCase(alignment, "top"))
        verticalAlignValue = CSSValueTop;
    else if (equalIgnoringCase(alignment, "middle"))
        verticalAlignValue = CSSValueWebkitBaselineMiddle;
    else if (equalIgnoringCase(alignment, "center"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalIgnoringCase(alignment, "bottom"))
        verticalAlignValue = CSSValueBaseline;
    else if (equalIgnoringCase(alignment, "texttop"))
        verticalAlignValue = CSSValueTextTop;

    if (floatValue != CSSValueInvalid)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFloat, floatValue);

    if (verticalAlignValue != CSSValueInvalid)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, verticalAlignValue);
}

bool HTMLElement::hasCustomFocusLogic() const
{
    return false;
}

String HTMLElement::contentEditable() const
{
    const AtomicString& value = fastGetAttribute(contenteditableAttr);

    if (value.isNull())
        return "inherit";
    if (value.isEmpty() || equalIgnoringCase(value, "true"))
        return "true";
    if (equalIgnoringCase(value, "false"))
         return "false";
    if (equalIgnoringCase(value, "plaintext-only"))
        return "plaintext-only";

    return "inherit";
}

void HTMLElement::setContentEditable(const String& enabled, ExceptionState& exceptionState)
{
    if (equalIgnoringCase(enabled, "true"))
        setAttribute(contenteditableAttr, "true");
    else if (equalIgnoringCase(enabled, "false"))
        setAttribute(contenteditableAttr, "false");
    else if (equalIgnoringCase(enabled, "plaintext-only"))
        setAttribute(contenteditableAttr, "plaintext-only");
    else if (equalIgnoringCase(enabled, "inherit"))
        removeAttribute(contenteditableAttr);
    else
        exceptionState.throwDOMException(SyntaxError, "The value provided ('" + enabled + "') is not one of 'true', 'false', 'plaintext-only', or 'inherit'.");
}

bool HTMLElement::draggable() const
{
    return equalIgnoringCase(getAttribute(draggableAttr), "true");
}

void HTMLElement::setDraggable(bool value)
{
    setAttribute(draggableAttr, value ? "true" : "false");
}

bool HTMLElement::spellcheck() const
{
    return isSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable)
{
    setAttribute(spellcheckAttr, enable ? "true" : "false");
}


void HTMLElement::click()
{
    dispatchSimulatedClick(0, SendNoEvents);
}

void HTMLElement::accessKeyAction(bool sendMouseEvents)
{
    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

String HTMLElement::title() const
{
    return fastGetAttribute(titleAttr);
}

short HTMLElement::tabIndex() const
{
    if (supportsFocus())
        return Element::tabIndex();
    return -1;
}

TranslateAttributeMode HTMLElement::translateAttributeMode() const
{
    const AtomicString& value = getAttribute(translateAttr);

    if (value == nullAtom)
        return TranslateAttributeInherit;
    if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, ""))
        return TranslateAttributeYes;
    if (equalIgnoringCase(value, "no"))
        return TranslateAttributeNo;

    return TranslateAttributeInherit;
}

bool HTMLElement::translate() const
{
    for (const HTMLElement* element = this; element; element = Traversal<HTMLElement>::firstAncestor(*element)) {
        TranslateAttributeMode mode = element->translateAttributeMode();
        if (mode != TranslateAttributeInherit) {
            ASSERT(mode == TranslateAttributeYes || mode == TranslateAttributeNo);
            return mode == TranslateAttributeYes;
        }
    }

    // Default on the root element is translate=yes.
    return true;
}

void HTMLElement::setTranslate(bool enable)
{
    setAttribute(translateAttr, enable ? "yes" : "no");
}

// Returns the conforming 'dir' value associated with the state the attribute is in (in its canonical case), if any,
// or the empty string if the attribute is in a state that has no associated keyword value or if the attribute is
// not in a defined state (e.g. the attribute is missing and there is no missing value default).
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#limited-to-only-known-values
static inline const AtomicString& toValidDirValue(const AtomicString& value)
{
    DEFINE_STATIC_LOCAL(const AtomicString, ltrValue, ("ltr", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, rtlValue, ("rtl", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, autoValue, ("auto", AtomicString::ConstructFromLiteral));

    if (equalIgnoringCase(value, ltrValue))
        return ltrValue;
    if (equalIgnoringCase(value, rtlValue))
        return rtlValue;
    if (equalIgnoringCase(value, autoValue))
        return autoValue;
    return nullAtom;
}

const AtomicString& HTMLElement::dir()
{
    return toValidDirValue(fastGetAttribute(dirAttr));
}

void HTMLElement::setDir(const AtomicString& value)
{
    setAttribute(dirAttr, value);
}

HTMLFormElement* HTMLElement::findFormAncestor() const
{
    return Traversal<HTMLFormElement>::firstAncestor(*this);
}

static inline bool elementAffectsDirectionality(const Node* node)
{
    return node->isHTMLElement() && (isHTMLBDIElement(*node) || toHTMLElement(node)->hasAttribute(dirAttr));
}

static void setHasDirAutoFlagRecursively(Node* firstNode, bool flag, Node* lastNode = 0)
{
    firstNode->setSelfOrAncestorHasDirAutoAttribute(flag);

    Node* node = firstNode->firstChild();

    while (node) {
        if (elementAffectsDirectionality(node)) {
            if (node == lastNode)
                return;
            node = NodeTraversal::nextSkippingChildren(*node, firstNode);
            continue;
        }
        node->setSelfOrAncestorHasDirAutoAttribute(flag);
        if (node == lastNode)
            return;
        node = NodeTraversal::next(*node, firstNode);
    }
}

void HTMLElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    Element::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    adjustDirectionalityIfNeededAfterChildrenChanged(beforeChange, childCountDelta);
}

bool HTMLElement::hasDirectionAuto() const
{
    const AtomicString& direction = fastGetAttribute(dirAttr);
    return (isHTMLBDIElement(*this) && direction == nullAtom) || equalIgnoringCase(direction, "auto");
}

TextDirection HTMLElement::directionalityIfhasDirAutoAttribute(bool& isAuto) const
{
    if (!(selfOrAncestorHasDirAutoAttribute() && hasDirectionAuto())) {
        isAuto = false;
        return LTR;
    }

    isAuto = true;
    return directionality();
}

TextDirection HTMLElement::directionality(Node** strongDirectionalityTextNode) const
{
    if (isHTMLInputElement(*this)) {
        HTMLInputElement* inputElement = toHTMLInputElement(const_cast<HTMLElement*>(this));
        bool hasStrongDirectionality;
        TextDirection textDirection = determineDirectionality(inputElement->value(), hasStrongDirectionality);
        if (strongDirectionalityTextNode)
            *strongDirectionalityTextNode = hasStrongDirectionality ? inputElement : 0;
        return textDirection;
    }

    Node* node = firstChild();
    while (node) {
        // Skip bdi, script, style and text form controls.
        if (equalIgnoringCase(node->nodeName(), "bdi") || isHTMLScriptElement(*node) || isHTMLStyleElement(*node)
            || (node->isElementNode() && toElement(node)->isTextFormControl())) {
            node = NodeTraversal::nextSkippingChildren(*node, this);
            continue;
        }

        // Skip elements with valid dir attribute
        if (node->isElementNode()) {
            AtomicString dirAttributeValue = toElement(node)->fastGetAttribute(dirAttr);
            if (isValidDirAttribute(dirAttributeValue)) {
                node = NodeTraversal::nextSkippingChildren(*node, this);
                continue;
            }
        }

        if (node->isTextNode()) {
            bool hasStrongDirectionality;
            TextDirection textDirection = determineDirectionality(node->textContent(true), hasStrongDirectionality);
            if (hasStrongDirectionality) {
                if (strongDirectionalityTextNode)
                    *strongDirectionalityTextNode = node;
                return textDirection;
            }
        }
        node = NodeTraversal::next(*node, this);
    }
    if (strongDirectionalityTextNode)
        *strongDirectionalityTextNode = 0;
    return LTR;
}

void HTMLElement::dirAttributeChanged(const AtomicString& value)
{
    Element* parent = parentElement();

    if (parent && parent->isHTMLElement() && parent->selfOrAncestorHasDirAutoAttribute())
        toHTMLElement(parent)->adjustDirectionalityIfNeededAfterChildAttributeChanged(this);

    if (equalIgnoringCase(value, "auto"))
        calculateAndAdjustDirectionality();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child)
{
    ASSERT(selfOrAncestorHasDirAutoAttribute());
    Node* strongDirectionalityTextNode;
    TextDirection textDirection = directionality(&strongDirectionalityTextNode);
    setHasDirAutoFlagRecursively(child, false);
    if (renderer() && renderer()->style() && renderer()->style()->direction() != textDirection) {
        Element* elementToAdjust = this;
        for (; elementToAdjust; elementToAdjust = elementToAdjust->parentElement()) {
            if (elementAffectsDirectionality(elementToAdjust)) {
                elementToAdjust->setNeedsStyleRecalc(SubtreeStyleChange);
                return;
            }
        }
    }
}

void HTMLElement::calculateAndAdjustDirectionality()
{
    Node* strongDirectionalityTextNode;
    TextDirection textDirection = directionality(&strongDirectionalityTextNode);
    setHasDirAutoFlagRecursively(this, hasDirectionAuto(), strongDirectionalityTextNode);
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        setHasDirAutoFlagRecursively(root, hasDirectionAuto());
    if (renderer() && renderer()->style() && renderer()->style()->direction() != textDirection)
        setNeedsStyleRecalc(SubtreeStyleChange);
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildrenChanged(Node* beforeChange, int childCountDelta)
{
    if (document().renderView() && childCountDelta < 0) {
        Node* node = beforeChange ? NodeTraversal::nextSkippingChildren(*beforeChange) : 0;
        for (int counter = 0; node && counter < childCountDelta; counter++, node = NodeTraversal::nextSkippingChildren(*node)) {
            if (elementAffectsDirectionality(node))
                continue;

            setHasDirAutoFlagRecursively(node, false);
        }
    }

    if (!selfOrAncestorHasDirAutoAttribute())
        return;

    Node* oldMarkedNode = beforeChange ? NodeTraversal::nextSkippingChildren(*beforeChange) : 0;
    while (oldMarkedNode && elementAffectsDirectionality(oldMarkedNode))
        oldMarkedNode = NodeTraversal::nextSkippingChildren(*oldMarkedNode, this);
    if (oldMarkedNode)
        setHasDirAutoFlagRecursively(oldMarkedNode, false);

    for (Element* elementToAdjust = this; elementToAdjust; elementToAdjust = elementToAdjust->parentElement()) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            toHTMLElement(elementToAdjust)->calculateAndAdjustDirectionality();
            return;
        }
    }
}

void HTMLElement::addHTMLLengthToStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, const String& value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.

    // strip attribute garbage..
    StringImpl* v = value.impl();
    if (v) {
        unsigned length = 0;

        while (length < v->length() && (*v)[length] <= ' ')
            length++;

        for (; length < v->length(); length++) {
            UChar cc = (*v)[length];
            if (cc > '9')
                break;
            if (cc < '0') {
                if (cc == '%' || cc == '*')
                    length++;
                if (cc != '.')
                    break;
            }
        }

        if (length != v->length()) {
            addPropertyToPresentationAttributeStyle(style, propertyID, v->substring(0, length));
            return;
        }
    }

    addPropertyToPresentationAttributeStyle(style, propertyID, value);
}

static RGBA32 parseColorStringWithCrazyLegacyRules(const String& colorString)
{
    // Per spec, only look at the first 128 digits of the string.
    const size_t maxColorLength = 128;
    // We'll pad the buffer with two extra 0s later, so reserve two more than the max.
    Vector<char, maxColorLength+2> digitBuffer;

    size_t i = 0;
    // Skip a leading #.
    if (colorString[0] == '#')
        i = 1;

    // Grab the first 128 characters, replacing non-hex characters with 0.
    // Non-BMP characters are replaced with "00" due to them appearing as two "characters" in the String.
    for (; i < colorString.length() && digitBuffer.size() < maxColorLength; i++) {
        if (!isASCIIHexDigit(colorString[i]))
            digitBuffer.append('0');
        else
            digitBuffer.append(colorString[i]);
    }

    if (!digitBuffer.size())
        return Color::black;

    // Pad the buffer out to at least the next multiple of three in size.
    digitBuffer.append('0');
    digitBuffer.append('0');

    if (digitBuffer.size() < 6)
        return makeRGB(toASCIIHexValue(digitBuffer[0]), toASCIIHexValue(digitBuffer[1]), toASCIIHexValue(digitBuffer[2]));

    // Split the digits into three components, then search the last 8 digits of each component.
    ASSERT(digitBuffer.size() >= 6);
    size_t componentLength = digitBuffer.size() / 3;
    size_t componentSearchWindowLength = min<size_t>(componentLength, 8);
    size_t redIndex = componentLength - componentSearchWindowLength;
    size_t greenIndex = componentLength * 2 - componentSearchWindowLength;
    size_t blueIndex = componentLength * 3 - componentSearchWindowLength;
    // Skip digits until one of them is non-zero, or we've only got two digits left in the component.
    while (digitBuffer[redIndex] == '0' && digitBuffer[greenIndex] == '0' && digitBuffer[blueIndex] == '0' && (componentLength - redIndex) > 2) {
        redIndex++;
        greenIndex++;
        blueIndex++;
    }
    ASSERT(redIndex + 1 < componentLength);
    ASSERT(greenIndex >= componentLength);
    ASSERT(greenIndex + 1 < componentLength * 2);
    ASSERT(blueIndex >= componentLength * 2);
    ASSERT_WITH_SECURITY_IMPLICATION(blueIndex + 1 < digitBuffer.size());

    int redValue = toASCIIHexValue(digitBuffer[redIndex], digitBuffer[redIndex + 1]);
    int greenValue = toASCIIHexValue(digitBuffer[greenIndex], digitBuffer[greenIndex + 1]);
    int blueValue = toASCIIHexValue(digitBuffer[blueIndex], digitBuffer[blueIndex + 1]);
    return makeRGB(redValue, greenValue, blueValue);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
void HTMLElement::addHTMLColorToStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, const String& attributeValue)
{
    // An empty string doesn't apply a color. (One containing only whitespace does, which is why this check occurs before stripping.)
    if (attributeValue.isEmpty())
        return;

    String colorString = attributeValue.stripWhiteSpace();

    // "transparent" doesn't apply a color either.
    if (equalIgnoringCase(colorString, "transparent"))
        return;

    // If the string is a named CSS color or a 3/6-digit hex color, use that.
    Color parsedColor;
    if (!parsedColor.setFromString(colorString))
        parsedColor.setRGB(parseColorStringWithCrazyLegacyRules(colorString));

    style->setProperty(propertyID, cssValuePool().createColorValue(parsedColor.rgb()));
}

bool HTMLElement::isInteractiveContent() const
{
    return false;
}

void HTMLElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::keypress && event->isKeyboardEvent()) {
        handleKeypressEvent(toKeyboardEvent(event));
        if (event->defaultHandled())
            return;
    }

    Element::defaultEventHandler(event);
}

bool HTMLElement::matchesReadOnlyPseudoClass() const
{
    return !matchesReadWritePseudoClass();
}

bool HTMLElement::matchesReadWritePseudoClass() const
{
    if (fastHasAttribute(contenteditableAttr)) {
        const AtomicString& value = fastGetAttribute(contenteditableAttr);

        if (value.isEmpty() || equalIgnoringCase(value, "true") || equalIgnoringCase(value, "plaintext-only"))
            return true;
        if (equalIgnoringCase(value, "false"))
            return false;
        // All other values should be treated as "inherit".
    }

    return parentElement() && parentElement()->rendererIsEditable();
}

void HTMLElement::handleKeypressEvent(KeyboardEvent* event)
{
    if (!document().settings() || !document().settings()->spatialNavigationEnabled() || !supportsFocus())
        return;
    // if the element is a text form control (like <input type=text> or <textarea>)
    // or has contentEditable attribute on, we should enter a space or newline
    // even in spatial navigation mode instead of handling it as a "click" action.
    if (isTextFormControl() || isContentEditable())
        return;
    int charCode = event->charCode();
    if (charCode == '\r' || charCode == ' ') {
        dispatchSimulatedClick(event);
        event->setDefaultHandled();
    }
}

const AtomicString& HTMLElement::eventParameterName()
{
    DEFINE_STATIC_LOCAL(const AtomicString, eventString, ("event", AtomicString::ConstructFromLiteral));
    return eventString;
}

} // namespace WebCore

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(WebCore::HTMLElement*);

void dumpInnerHTML(WebCore::HTMLElement* element)
{
    printf("%s\n", element->innerHTML().ascii().data());
}
#endif
