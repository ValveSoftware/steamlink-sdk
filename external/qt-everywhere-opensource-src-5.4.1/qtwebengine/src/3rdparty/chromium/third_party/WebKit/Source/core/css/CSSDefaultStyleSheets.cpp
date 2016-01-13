/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/css/CSSDefaultStyleSheets.h"

#include "core/MathMLNames.h"
#include "core/UserAgentStyleSheets.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/LeakAnnotations.h"

namespace WebCore {

using namespace HTMLNames;

CSSDefaultStyleSheets& CSSDefaultStyleSheets::instance()
{
#if ENABLE(OILPAN)
    DEFINE_STATIC_LOCAL(Persistent<CSSDefaultStyleSheets>, cssDefaultStyleSheets, (new CSSDefaultStyleSheets()));
    return *cssDefaultStyleSheets;
#else
    DEFINE_STATIC_LOCAL(CSSDefaultStyleSheets, cssDefaultStyleSheets, ());
    return cssDefaultStyleSheets;
#endif
}

static const MediaQueryEvaluator& screenEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticScreenEval, ("screen"));
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticPrintEval, ("print"));
    return staticPrintEval;
}

static PassRefPtrWillBeRawPtr<StyleSheetContents> parseUASheet(const String& str)
{
    RefPtrWillBeRawPtr<StyleSheetContents> sheet = StyleSheetContents::create(CSSParserContext(UASheetMode, 0));
    sheet->parseString(str);
    // User Agent stylesheets are parsed once for the lifetime of the renderer
    // and are intentionally leaked.
    WTF_ANNOTATE_LEAKING_OBJECT_PTR(sheet.get());
    return sheet.release();
}

static PassRefPtrWillBeRawPtr<StyleSheetContents> parseUASheet(const char* characters, unsigned size)
{
    return parseUASheet(String(characters, size));
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets()
    : m_defaultStyle(nullptr)
    , m_defaultViewportStyle(nullptr)
    , m_defaultQuirksStyle(nullptr)
    , m_defaultPrintStyle(nullptr)
    , m_defaultViewSourceStyle(nullptr)
    , m_defaultXHTMLMobileProfileStyle(nullptr)
    , m_defaultTransitionStyle(nullptr)
    , m_defaultStyleSheet(nullptr)
    , m_viewportStyleSheet(nullptr)
    , m_quirksStyleSheet(nullptr)
    , m_svgStyleSheet(nullptr)
    , m_mathmlStyleSheet(nullptr)
    , m_mediaControlsStyleSheet(nullptr)
    , m_fullscreenStyleSheet(nullptr)
{
    m_defaultStyle = RuleSet::create();
    m_defaultViewportStyle = RuleSet::create();
    m_defaultPrintStyle = RuleSet::create();
    m_defaultQuirksStyle = RuleSet::create();

    // Strict-mode rules.
    String defaultRules = String(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet)) + RenderTheme::theme().extraDefaultStyleSheet();
    m_defaultStyleSheet = parseUASheet(defaultRules);
    m_defaultStyle->addRulesFromSheet(defaultStyleSheet(), screenEval());
#if OS(ANDROID)
    String viewportRules(viewportAndroidUserAgentStyleSheet, sizeof(viewportAndroidUserAgentStyleSheet));
#else
    String viewportRules;
#endif
    m_viewportStyleSheet = parseUASheet(viewportRules);
    m_defaultViewportStyle->addRulesFromSheet(viewportStyleSheet(), screenEval());
    m_defaultPrintStyle->addRulesFromSheet(defaultStyleSheet(), printEval());

    // Quirks-mode rules.
    String quirksRules = String(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet)) + RenderTheme::theme().extraQuirksStyleSheet();
    m_quirksStyleSheet = parseUASheet(quirksRules);
    m_defaultQuirksStyle->addRulesFromSheet(quirksStyleSheet(), screenEval());
}

RuleSet* CSSDefaultStyleSheets::defaultViewSourceStyle()
{
    if (!m_defaultViewSourceStyle) {
        m_defaultViewSourceStyle = RuleSet::create();
        // Loaded stylesheet is leaked on purpose.
        RefPtrWillBeRawPtr<StyleSheetContents> stylesheet = parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet));
        m_defaultViewSourceStyle->addRulesFromSheet(stylesheet.release().leakRef(), screenEval());
    }
    return m_defaultViewSourceStyle.get();
}

RuleSet* CSSDefaultStyleSheets::defaultTransitionStyle()
{
    if (!m_defaultTransitionStyle) {
        m_defaultTransitionStyle = RuleSet::create();
        // Loaded stylesheet is leaked on purpose.
        RefPtrWillBeRawPtr<StyleSheetContents> stylesheet = parseUASheet(navigationTransitionsUserAgentStyleSheet, sizeof(navigationTransitionsUserAgentStyleSheet));
        m_defaultTransitionStyle->addRulesFromSheet(stylesheet.release().leakRef(), screenEval());
    }
    return m_defaultTransitionStyle.get();
}

RuleSet* CSSDefaultStyleSheets::defaultXHTMLMobileProfileStyle()
{
    if (!m_defaultXHTMLMobileProfileStyle) {
        m_defaultXHTMLMobileProfileStyle = RuleSet::create();
        // Loaded stylesheet is leaked on purpose.
        RefPtrWillBeRawPtr<StyleSheetContents> stylesheet = parseUASheet(xhtmlmpUserAgentStyleSheet, sizeof(xhtmlmpUserAgentStyleSheet));
        m_defaultXHTMLMobileProfileStyle->addRulesFromSheet(stylesheet.release().leakRef(), screenEval());
    }
    return m_defaultXHTMLMobileProfileStyle.get();
}

void CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(Element* element, bool& changedDefaultStyle)
{
    // FIXME: We should assert that the sheet only styles SVG elements.
    if (element->isSVGElement() && !m_svgStyleSheet) {
        m_svgStyleSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        m_defaultStyle->addRulesFromSheet(svgStyleSheet(), screenEval());
        m_defaultPrintStyle->addRulesFromSheet(svgStyleSheet(), printEval());
        changedDefaultStyle = true;
    }

    // FIXME: We should assert that the sheet only styles MathML elements.
    if (element->namespaceURI() == MathMLNames::mathmlNamespaceURI
        && !m_mathmlStyleSheet) {
        m_mathmlStyleSheet = parseUASheet(mathmlUserAgentStyleSheet,
            sizeof(mathmlUserAgentStyleSheet));
        m_defaultStyle->addRulesFromSheet(mathmlStyleSheet(), screenEval());
        m_defaultPrintStyle->addRulesFromSheet(mathmlStyleSheet(), printEval());
        changedDefaultStyle = true;
    }

    // FIXME: We should assert that this sheet only contains rules for <video> and <audio>.
    if (!m_mediaControlsStyleSheet && (isHTMLVideoElement(*element) || isHTMLAudioElement(*element))) {
        String mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::theme().extraMediaControlsStyleSheet();
        m_mediaControlsStyleSheet = parseUASheet(mediaRules);
        m_defaultStyle->addRulesFromSheet(mediaControlsStyleSheet(), screenEval());
        m_defaultPrintStyle->addRulesFromSheet(mediaControlsStyleSheet(), printEval());
        changedDefaultStyle = true;
    }

    // FIXME: This only works because we Force recalc the entire document so the new sheet
    // is loaded for <html> and the correct styles apply to everyone.
    if (!m_fullscreenStyleSheet && FullscreenElementStack::isFullScreen(element->document())) {
        String fullscreenRules = String(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)) + RenderTheme::theme().extraFullScreenStyleSheet();
        m_fullscreenStyleSheet = parseUASheet(fullscreenRules);
        m_defaultStyle->addRulesFromSheet(fullscreenStyleSheet(), screenEval());
        m_defaultQuirksStyle->addRulesFromSheet(fullscreenStyleSheet(), screenEval());
        changedDefaultStyle = true;
    }

    ASSERT(!m_defaultStyle->features().hasIdsInSelectors());
    ASSERT(m_defaultStyle->features().siblingRules.isEmpty());
}

void CSSDefaultStyleSheets::trace(Visitor* visitor)
{
    visitor->trace(m_defaultStyle);
    visitor->trace(m_defaultViewportStyle);
    visitor->trace(m_defaultQuirksStyle);
    visitor->trace(m_defaultPrintStyle);
    visitor->trace(m_defaultViewSourceStyle);
    visitor->trace(m_defaultXHTMLMobileProfileStyle);
    visitor->trace(m_defaultTransitionStyle);
    visitor->trace(m_defaultStyleSheet);
    visitor->trace(m_viewportStyleSheet);
    visitor->trace(m_quirksStyleSheet);
    visitor->trace(m_svgStyleSheet);
    visitor->trace(m_mathmlStyleSheet);
    visitor->trace(m_mediaControlsStyleSheet);
    visitor->trace(m_fullscreenStyleSheet);
}

} // namespace WebCore
