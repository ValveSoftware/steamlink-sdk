/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/css/CSSSelector.h"

#include "core/HTMLNames.h"
#include "core/css/CSSOMUtils.h"
#include "core/css/CSSSelectorList.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsCSSSelector {
    unsigned bitfields;
    void *pointers[1];
};

COMPILE_ASSERT(sizeof(CSSSelector) == sizeof(SameSizeAsCSSSelector), CSSSelectorShouldStaySmall);

void CSSSelector::createRareData()
{
    ASSERT(m_match != Tag);
    if (m_hasRareData)
        return;
    AtomicString value(m_data.m_value);
    if (m_data.m_value)
        m_data.m_value->deref();
    m_data.m_rareData = RareData::create(value).leakRef();
    m_hasRareData = true;
}

unsigned CSSSelector::specificity() const
{
    // make sure the result doesn't overflow
    static const unsigned maxValueMask = 0xffffff;
    static const unsigned idMask = 0xff0000;
    static const unsigned classMask = 0xff00;
    static const unsigned elementMask = 0xff;

    if (isForPage())
        return specificityForPage() & maxValueMask;

    unsigned total = 0;
    unsigned temp = 0;

    for (const CSSSelector* selector = this; selector; selector = selector->tagHistory()) {
        temp = total + selector->specificityForOneSelector();
        // Clamp each component to its max in the case of overflow.
        if ((temp & idMask) < (total & idMask))
            total |= idMask;
        else if ((temp & classMask) < (total & classMask))
            total |= classMask;
        else if ((temp & elementMask) < (total & elementMask))
            total |= elementMask;
        else
            total = temp;
    }
    return total;
}

inline unsigned CSSSelector::specificityForOneSelector() const
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    switch (m_match) {
    case Id:
        return 0x10000;
    case PseudoClass:
        if (pseudoType() == PseudoHost || pseudoType() == PseudoHostContext)
            return 0;
        // fall through.
    case Exact:
    case Class:
    case Set:
    case List:
    case Hyphen:
    case PseudoElement:
    case Contain:
    case Begin:
    case End:
        // FIXME: PseudoAny should base the specificity on the sub-selectors.
        // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0530.html
        if (pseudoType() == PseudoNot) {
            ASSERT(selectorList());
            return selectorList()->first()->specificityForOneSelector();
        }
        return 0x100;
    case Tag:
        return (tagQName().localName() != starAtom) ? 1 : 0;
    case Unknown:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned CSSSelector::specificityForPage() const
{
    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = 0;

    for (const CSSSelector* component = this; component; component = component->tagHistory()) {
        switch (component->m_match) {
        case Tag:
            s += tagQName().localName() == starAtom ? 0 : 4;
            break;
        case PagePseudoClass:
            switch (component->pseudoType()) {
            case PseudoFirstPage:
                s += 2;
                break;
            case PseudoLeftPage:
            case PseudoRightPage:
                s += 1;
                break;
            case PseudoNotParsed:
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        default:
            break;
        }
    }
    return s;
}

PseudoId CSSSelector::pseudoId(PseudoType type)
{
    switch (type) {
    case PseudoFirstLine:
        return FIRST_LINE;
    case PseudoFirstLetter:
        return FIRST_LETTER;
    case PseudoSelection:
        return SELECTION;
    case PseudoBefore:
        return BEFORE;
    case PseudoAfter:
        return AFTER;
    case PseudoBackdrop:
        return BACKDROP;
    case PseudoScrollbar:
        return SCROLLBAR;
    case PseudoScrollbarButton:
        return SCROLLBAR_BUTTON;
    case PseudoScrollbarCorner:
        return SCROLLBAR_CORNER;
    case PseudoScrollbarThumb:
        return SCROLLBAR_THUMB;
    case PseudoScrollbarTrack:
        return SCROLLBAR_TRACK;
    case PseudoScrollbarTrackPiece:
        return SCROLLBAR_TRACK_PIECE;
    case PseudoResizer:
        return RESIZER;
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAny:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScope:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
    case PseudoCue:
    case PseudoFutureCue:
    case PseudoPastCue:
    case PseudoUnresolved:
    case PseudoContent:
    case PseudoHost:
    case PseudoHostContext:
    case PseudoShadow:
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
    case PseudoFullScreenAncestor:
        return NOPSEUDO;
    case PseudoNotParsed:
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

// Could be made smaller and faster by replacing pointer with an
// offset into a string buffer and making the bit fields smaller but
// that could not be maintained by hand.
struct NameToPseudoStruct {
    const char* string;
    unsigned type:8;
};

// This table should be kept sorted.
const static NameToPseudoStruct pseudoTypeMap[] = {
{"-webkit-any(",                  CSSSelector::PseudoAny},
{"-webkit-any-link",              CSSSelector::PseudoAnyLink},
{"-webkit-autofill",              CSSSelector::PseudoAutofill},
{"-webkit-drag",                  CSSSelector::PseudoDrag},
{"-webkit-full-page-media",       CSSSelector::PseudoFullPageMedia},
{"-webkit-full-screen",           CSSSelector::PseudoFullScreen},
{"-webkit-full-screen-ancestor",  CSSSelector::PseudoFullScreenAncestor},
{"-webkit-full-screen-document",  CSSSelector::PseudoFullScreenDocument},
{"-webkit-resizer",               CSSSelector::PseudoResizer},
{"-webkit-scrollbar",             CSSSelector::PseudoScrollbar},
{"-webkit-scrollbar-button",      CSSSelector::PseudoScrollbarButton},
{"-webkit-scrollbar-corner",      CSSSelector::PseudoScrollbarCorner},
{"-webkit-scrollbar-thumb",       CSSSelector::PseudoScrollbarThumb},
{"-webkit-scrollbar-track",       CSSSelector::PseudoScrollbarTrack},
{"-webkit-scrollbar-track-piece", CSSSelector::PseudoScrollbarTrackPiece},
{"active",                        CSSSelector::PseudoActive},
{"after",                         CSSSelector::PseudoAfter},
{"backdrop",                      CSSSelector::PseudoBackdrop},
{"before",                        CSSSelector::PseudoBefore},
{"checked",                       CSSSelector::PseudoChecked},
{"content",                       CSSSelector::PseudoContent},
{"corner-present",                CSSSelector::PseudoCornerPresent},
{"cue",                           CSSSelector::PseudoWebKitCustomElement},
{"cue(",                          CSSSelector::PseudoCue},
{"decrement",                     CSSSelector::PseudoDecrement},
{"default",                       CSSSelector::PseudoDefault},
{"disabled",                      CSSSelector::PseudoDisabled},
{"double-button",                 CSSSelector::PseudoDoubleButton},
{"empty",                         CSSSelector::PseudoEmpty},
{"enabled",                       CSSSelector::PseudoEnabled},
{"end",                           CSSSelector::PseudoEnd},
{"first",                         CSSSelector::PseudoFirstPage},
{"first-child",                   CSSSelector::PseudoFirstChild},
{"first-letter",                  CSSSelector::PseudoFirstLetter},
{"first-line",                    CSSSelector::PseudoFirstLine},
{"first-of-type",                 CSSSelector::PseudoFirstOfType},
{"focus",                         CSSSelector::PseudoFocus},
{"future",                        CSSSelector::PseudoFutureCue},
{"horizontal",                    CSSSelector::PseudoHorizontal},
{"host",                          CSSSelector::PseudoHost},
{"host(",                         CSSSelector::PseudoHost},
{"host-context(",                 CSSSelector::PseudoHostContext},
{"hover",                         CSSSelector::PseudoHover},
{"in-range",                      CSSSelector::PseudoInRange},
{"increment",                     CSSSelector::PseudoIncrement},
{"indeterminate",                 CSSSelector::PseudoIndeterminate},
{"invalid",                       CSSSelector::PseudoInvalid},
{"lang(",                         CSSSelector::PseudoLang},
{"last-child",                    CSSSelector::PseudoLastChild},
{"last-of-type",                  CSSSelector::PseudoLastOfType},
{"left",                          CSSSelector::PseudoLeftPage},
{"link",                          CSSSelector::PseudoLink},
{"no-button",                     CSSSelector::PseudoNoButton},
{"not(",                          CSSSelector::PseudoNot},
{"nth-child(",                    CSSSelector::PseudoNthChild},
{"nth-last-child(",               CSSSelector::PseudoNthLastChild},
{"nth-last-of-type(",             CSSSelector::PseudoNthLastOfType},
{"nth-of-type(",                  CSSSelector::PseudoNthOfType},
{"only-child",                    CSSSelector::PseudoOnlyChild},
{"only-of-type",                  CSSSelector::PseudoOnlyOfType},
{"optional",                      CSSSelector::PseudoOptional},
{"out-of-range",                  CSSSelector::PseudoOutOfRange},
{"past",                          CSSSelector::PseudoPastCue},
{"read-only",                     CSSSelector::PseudoReadOnly},
{"read-write",                    CSSSelector::PseudoReadWrite},
{"required",                      CSSSelector::PseudoRequired},
{"right",                         CSSSelector::PseudoRightPage},
{"root",                          CSSSelector::PseudoRoot},
{"scope",                         CSSSelector::PseudoScope},
{"selection",                     CSSSelector::PseudoSelection},
{"shadow",                        CSSSelector::PseudoShadow},
{"single-button",                 CSSSelector::PseudoSingleButton},
{"start",                         CSSSelector::PseudoStart},
{"target",                        CSSSelector::PseudoTarget},
{"unresolved",                    CSSSelector::PseudoUnresolved},
{"valid",                         CSSSelector::PseudoValid},
{"vertical",                      CSSSelector::PseudoVertical},
{"visited",                       CSSSelector::PseudoVisited},
{"window-inactive",               CSSSelector::PseudoWindowInactive},
};

class NameToPseudoCompare {
public:
    NameToPseudoCompare(const AtomicString& key) : m_key(key) { ASSERT(m_key.is8Bit()); }

    bool operator()(const NameToPseudoStruct& entry, const NameToPseudoStruct&)
    {
        ASSERT(entry.string);
        const char* key = reinterpret_cast<const char*>(m_key.characters8());
        // If strncmp returns 0, then either the keys are equal, or |m_key| sorts before |entry|.
        return strncmp(entry.string, key, m_key.length()) < 0;
    }

private:
    const AtomicString& m_key;
};

static CSSSelector::PseudoType nameToPseudoType(const AtomicString& name)
{
    if (name.isNull() || !name.is8Bit())
        return CSSSelector::PseudoUnknown;

    const NameToPseudoStruct* pseudoTypeMapEnd = pseudoTypeMap + WTF_ARRAY_LENGTH(pseudoTypeMap);
    NameToPseudoStruct dummyKey = { 0, CSSSelector::PseudoUnknown };
    const NameToPseudoStruct* match = std::lower_bound(pseudoTypeMap, pseudoTypeMapEnd, dummyKey, NameToPseudoCompare(name));
    if (match == pseudoTypeMapEnd || match->string != name.string())
        return CSSSelector::PseudoUnknown;

    return static_cast<CSSSelector::PseudoType>(match->type);
}

#ifndef NDEBUG
void CSSSelector::show(int indent) const
{
    printf("%*sselectorText(): %s\n", indent, "", selectorText().ascii().data());
    printf("%*sm_match: %d\n", indent, "", m_match);
    printf("%*sisCustomPseudoElement(): %d\n", indent, "", isCustomPseudoElement());
    if (m_match != Tag)
        printf("%*svalue(): %s\n", indent, "", value().ascii().data());
    printf("%*spseudoType(): %d\n", indent, "", pseudoType());
    if (m_match == Tag)
        printf("%*stagQName().localName: %s\n", indent, "", tagQName().localName().ascii().data());
    printf("%*sisAttributeSelector(): %d\n", indent, "", isAttributeSelector());
    if (isAttributeSelector())
        printf("%*sattribute(): %s\n", indent, "", attribute().localName().ascii().data());
    printf("%*sargument(): %s\n", indent, "", argument().ascii().data());
    printf("%*sspecificity(): %u\n", indent, "", specificity());
    if (tagHistory()) {
        printf("\n%*s--> (relation == %d)\n", indent, "", relation());
        tagHistory()->show(indent + 2);
    } else {
        printf("\n%*s--> (relation == %d)\n", indent, "", relation());
    }
}

void CSSSelector::show() const
{
    printf("\n******* CSSSelector::show(\"%s\") *******\n", selectorText().ascii().data());
    show(2);
    printf("******* end *******\n");
}
#endif

CSSSelector::PseudoType CSSSelector::parsePseudoType(const AtomicString& name)
{
    CSSSelector::PseudoType pseudoType = nameToPseudoType(name);
    if (pseudoType != PseudoUnknown)
        return pseudoType;

    if (name.startsWith("-webkit-"))
        return PseudoWebKitCustomElement;
    if (name.startsWith("cue"))
        return PseudoUserAgentCustomElement;

    return PseudoUnknown;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement && m_match != PagePseudoClass)
        return;

    m_pseudoType = parsePseudoType(value());

    bool element = false; // pseudo-element
    bool compat = false; // single colon compatbility mode
    bool isPagePseudoClass = false; // Page pseudo-class

    switch (m_pseudoType) {
    case PseudoAfter:
    case PseudoBefore:
    case PseudoCue:
    case PseudoFirstLetter:
    case PseudoFirstLine:
        compat = true;
    case PseudoBackdrop:
    case PseudoResizer:
    case PseudoScrollbar:
    case PseudoScrollbarCorner:
    case PseudoScrollbarButton:
    case PseudoScrollbarThumb:
    case PseudoScrollbarTrack:
    case PseudoScrollbarTrackPiece:
    case PseudoSelection:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
    case PseudoContent:
    case PseudoShadow:
        element = true;
        break;
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAny:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoScope:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoNotParsed:
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
    case PseudoFullScreenAncestor:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoFutureCue:
    case PseudoPastCue:
    case PseudoHost:
    case PseudoHostContext:
    case PseudoUnresolved:
        break;
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
        isPagePseudoClass = true;
        break;
    }

    bool matchPagePseudoClass = (m_match == PagePseudoClass);
    if (matchPagePseudoClass != isPagePseudoClass)
        m_pseudoType = PseudoUnknown;
    else if (m_match == PseudoClass && element) {
        if (!compat)
            m_pseudoType = PseudoUnknown;
        else
            m_match = PseudoElement;
    } else if (m_match == PseudoElement && !element)
        m_pseudoType = PseudoUnknown;
}

bool CSSSelector::operator==(const CSSSelector& other) const
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->attribute() != sel2->attribute()
            || sel1->relation() != sel2->relation()
            || sel1->m_match != sel2->m_match
            || sel1->value() != sel2->value()
            || sel1->pseudoType() != sel2->pseudoType()
            || sel1->argument() != sel2->argument()) {
            return false;
        }
        if (sel1->m_match == Tag) {
            if (sel1->tagQName() != sel2->tagQName())
                return false;
        }
        sel1 = sel1->tagHistory();
        sel2 = sel2->tagHistory();
    }

    if (sel1 || sel2)
        return false;

    return true;
}

String CSSSelector::selectorText(const String& rightSide) const
{
    StringBuilder str;

    if (m_match == CSSSelector::Tag && !m_tagIsForNamespaceRule) {
        if (tagQName().prefix().isNull())
            str.append(tagQName().localName());
        else {
            str.append(tagQName().prefix().string());
            str.append('|');
            str.append(tagQName().localName());
        }
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->m_match == CSSSelector::Id) {
            str.append('#');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::Class) {
            str.append('.');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::PseudoClass || cs->m_match == CSSSelector::PagePseudoClass) {
            str.append(':');
            str.append(cs->value());

            switch (cs->pseudoType()) {
            case PseudoNot:
                ASSERT(cs->selectorList());
                str.append(cs->selectorList()->first()->selectorText());
                str.append(')');
                break;
            case PseudoLang:
            case PseudoNthChild:
            case PseudoNthLastChild:
            case PseudoNthOfType:
            case PseudoNthLastOfType:
                str.append(cs->argument());
                str.append(')');
                break;
            case PseudoAny: {
                const CSSSelector* firstSubSelector = cs->selectorList()->first();
                for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
                    if (subSelector != firstSubSelector)
                        str.append(',');
                    str.append(subSelector->selectorText());
                }
                str.append(')');
                break;
            }
            case PseudoHost:
            case PseudoHostContext: {
                if (cs->selectorList()) {
                    const CSSSelector* firstSubSelector = cs->selectorList()->first();
                    for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
                        if (subSelector != firstSubSelector)
                            str.append(',');
                        str.append(subSelector->selectorText());
                    }
                    str.append(')');
                }
                break;
            }
            default:
                break;
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str.appendLiteral("::");
            str.append(cs->value());

            if (cs->pseudoType() == PseudoContent) {
                if (cs->relation() == CSSSelector::SubSelector && cs->tagHistory())
                    return cs->tagHistory()->selectorText() + str.toString() + rightSide;
            }
        } else if (cs->isAttributeSelector()) {
            str.append('[');
            const AtomicString& prefix = cs->attribute().prefix();
            if (!prefix.isNull()) {
                str.append(prefix);
                str.append("|");
            }
            str.append(cs->attribute().localName());
            switch (cs->m_match) {
                case CSSSelector::Exact:
                    str.append('=');
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    str.append(']');
                    break;
                case CSSSelector::List:
                    str.appendLiteral("~=");
                    break;
                case CSSSelector::Hyphen:
                    str.appendLiteral("|=");
                    break;
                case CSSSelector::Begin:
                    str.appendLiteral("^=");
                    break;
                case CSSSelector::End:
                    str.appendLiteral("$=");
                    break;
                case CSSSelector::Contain:
                    str.appendLiteral("*=");
                    break;
                default:
                    break;
            }
            if (cs->m_match != CSSSelector::Set) {
                serializeString(cs->value(), str);
                str.append(']');
            }
        }
        if (cs->relation() != CSSSelector::SubSelector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (const CSSSelector* tagHistory = cs->tagHistory()) {
        switch (cs->relation()) {
        case CSSSelector::Descendant:
            return tagHistory->selectorText(" " + str.toString() + rightSide);
        case CSSSelector::Child:
            return tagHistory->selectorText(" > " + str.toString() + rightSide);
        case CSSSelector::ShadowDeep:
            return tagHistory->selectorText(" /deep/ " + str.toString() + rightSide);
        case CSSSelector::DirectAdjacent:
            return tagHistory->selectorText(" + " + str.toString() + rightSide);
        case CSSSelector::IndirectAdjacent:
            return tagHistory->selectorText(" ~ " + str.toString() + rightSide);
        case CSSSelector::SubSelector:
            ASSERT_NOT_REACHED();
        case CSSSelector::ShadowPseudo:
            return tagHistory->selectorText(str.toString() + rightSide);
        }
    }
    return str.toString() + rightSide;
}

void CSSSelector::setAttribute(const QualifiedName& value)
{
    createRareData();
    m_data.m_rareData->m_attribute = value;
}

void CSSSelector::setArgument(const AtomicString& value)
{
    createRareData();
    m_data.m_rareData->m_argument = value;
}

void CSSSelector::setSelectorList(PassOwnPtr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.m_rareData->m_selectorList = selectorList;
}

static bool validateSubSelector(const CSSSelector* selector)
{
    switch (selector->match()) {
    case CSSSelector::Tag:
    case CSSSelector::Id:
    case CSSSelector::Class:
    case CSSSelector::Exact:
    case CSSSelector::Set:
    case CSSSelector::List:
    case CSSSelector::Hyphen:
    case CSSSelector::Contain:
    case CSSSelector::Begin:
    case CSSSelector::End:
        return true;
    case CSSSelector::PseudoElement:
    case CSSSelector::Unknown:
        return false;
    case CSSSelector::PagePseudoClass:
    case CSSSelector::PseudoClass:
        break;
    }

    switch (selector->pseudoType()) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyOfType:
    case CSSSelector::PseudoHost:
    case CSSSelector::PseudoHostContext:
    case CSSSelector::PseudoNot:
        return true;
    default:
        return false;
    }
}

bool CSSSelector::isCompound() const
{
    if (!validateSubSelector(this))
        return false;

    const CSSSelector* prevSubSelector = this;
    const CSSSelector* subSelector = tagHistory();

    while (subSelector) {
        if (prevSubSelector->relation() != CSSSelector::SubSelector)
            return false;
        if (!validateSubSelector(subSelector))
            return false;

        prevSubSelector = subSelector;
        subSelector = subSelector->tagHistory();
    }

    return true;
}

bool CSSSelector::parseNth() const
{
    if (!m_hasRareData)
        return false;
    if (m_parsedNth)
        return true;
    m_parsedNth = m_data.m_rareData->parseNth();
    return m_parsedNth;
}

bool CSSSelector::matchNth(int count) const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

CSSSelector::RareData::RareData(const AtomicString& value)
    : m_value(value)
    , m_a(0)
    , m_b(0)
    , m_attribute(anyQName())
    , m_argument(nullAtom)
{
}

CSSSelector::RareData::~RareData()
{
}

// a helper function for parsing nth-arguments
bool CSSSelector::RareData::parseNth()
{
    String argument = m_argument.lower();

    if (argument.isEmpty())
        return false;

    m_a = 0;
    m_b = 0;
    if (argument == "odd") {
        m_a = 2;
        m_b = 1;
    } else if (argument == "even") {
        m_a = 2;
        m_b = 0;
    } else {
        size_t n = argument.find('n');
        if (n != kNotFound) {
            if (argument[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = argument.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = argument.substring(0, n).toInt();

            size_t p = argument.find('+', n);
            if (p != kNotFound)
                m_b = argument.substring(p + 1, argument.length() - p - 1).toInt();
            else {
                p = argument.find('-', n);
                if (p != kNotFound)
                    m_b = -argument.substring(p + 1, argument.length() - p - 1).toInt();
            }
        } else
            m_b = argument.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
bool CSSSelector::RareData::matchNth(int count)
{
    if (!m_a)
        return count == m_b;
    else if (m_a > 0) {
        if (count < m_b)
            return false;
        return (count - m_b) % m_a == 0;
    } else {
        if (count > m_b)
            return false;
        return (m_b - count) % (-m_a) == 0;
    }
}

} // namespace WebCore
