/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef CSSDefaultStyleSheets_h
#define CSSDefaultStyleSheets_h

#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class Element;
class RuleSet;
class StyleSheetContents;

class CSSDefaultStyleSheets : public GarbageCollected<CSSDefaultStyleSheets> {
    WTF_MAKE_NONCOPYABLE(CSSDefaultStyleSheets);
public:
    static CSSDefaultStyleSheets& instance();

    void ensureDefaultStyleSheetsForElement(const Element&, bool& changedDefaultStyle);
    void ensureDefaultStyleSheetForFullscreen();

    RuleSet* defaultStyle() { return m_defaultStyle.get(); }
    RuleSet* defaultQuirksStyle() { return m_defaultQuirksStyle.get(); }
    RuleSet* defaultPrintStyle() { return m_defaultPrintStyle.get(); }
    RuleSet* defaultViewSourceStyle();
    RuleSet* defaultMobileViewportStyle();
    RuleSet* defaultTelevisionViewportStyle();

    // FIXME: Remove WAP support.
    RuleSet* defaultXHTMLMobileProfileStyle();

    StyleSheetContents* defaultStyleSheet() { return m_defaultStyleSheet.get(); }
    StyleSheetContents* quirksStyleSheet() { return m_quirksStyleSheet.get(); }
    StyleSheetContents* svgStyleSheet() { return m_svgStyleSheet.get(); }
    StyleSheetContents* mathmlStyleSheet() { return m_mathmlStyleSheet.get(); }
    StyleSheetContents* mediaControlsStyleSheet() { return m_mediaControlsStyleSheet.get(); }
    StyleSheetContents* fullscreenStyleSheet() { return m_fullscreenStyleSheet.get(); }

    DECLARE_TRACE();

private:
    CSSDefaultStyleSheets();

    Member<RuleSet> m_defaultStyle;
    Member<RuleSet> m_defaultMobileViewportStyle;
    Member<RuleSet> m_defaultTelevisionViewportStyle;
    Member<RuleSet> m_defaultQuirksStyle;
    Member<RuleSet> m_defaultPrintStyle;
    Member<RuleSet> m_defaultViewSourceStyle;
    Member<RuleSet> m_defaultXHTMLMobileProfileStyle;

    Member<StyleSheetContents> m_defaultStyleSheet;
    Member<StyleSheetContents> m_mobileViewportStyleSheet;
    Member<StyleSheetContents> m_televisionViewportStyleSheet;
    Member<StyleSheetContents> m_quirksStyleSheet;
    Member<StyleSheetContents> m_svgStyleSheet;
    Member<StyleSheetContents> m_mathmlStyleSheet;
    Member<StyleSheetContents> m_mediaControlsStyleSheet;
    Member<StyleSheetContents> m_fullscreenStyleSheet;
};

} // namespace blink

#endif // CSSDefaultStyleSheets_h
