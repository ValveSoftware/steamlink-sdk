/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/css/resolver/ViewportStyleResolver.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/MediaValuesInitialViewport.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

ViewportStyleResolver::ViewportStyleResolver(Document& document)
    : m_document(document) {
  DCHECK(document.frame());
  m_initialViewportMedium = new MediaQueryEvaluator(
      MediaValuesInitialViewport::create(*document.frame()));
}

void ViewportStyleResolver::reset() {
  m_viewportDependentMediaQueryResults.clear();
  m_deviceDependentMediaQueryResults.clear();
  m_propertySet = nullptr;
  m_hasAuthorStyle = false;
  m_hasViewportUnits = false;
  m_needsUpdate = NoUpdate;
}

void ViewportStyleResolver::collectViewportRulesFromUASheets() {
  CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
  WebViewportStyle viewportStyle = m_document->settings()
                                       ? m_document->settings()->viewportStyle()
                                       : WebViewportStyle::Default;
  StyleSheetContents* viewportContents = nullptr;
  switch (viewportStyle) {
    case WebViewportStyle::Default:
      break;
    case WebViewportStyle::Mobile:
      viewportContents = defaultStyleSheets.ensureMobileViewportStyleSheet();
      break;
    case WebViewportStyle::Television:
      viewportContents =
          defaultStyleSheets.ensureTelevisionViewportStyleSheet();
      break;
  }
  if (viewportContents)
    collectViewportChildRules(viewportContents->childRules(), UserAgentOrigin);

  if (m_document->isMobileDocument()) {
    collectViewportChildRules(
        defaultStyleSheets.ensureXHTMLMobileProfileStyleSheet()->childRules(),
        UserAgentOrigin);
  }
  DCHECK(!defaultStyleSheets.defaultStyleSheet()->hasViewportRule());
}

void ViewportStyleResolver::collectViewportChildRules(
    const HeapVector<Member<StyleRuleBase>>& rules,
    Origin origin) {
  for (auto& rule : rules) {
    if (rule->isViewportRule()) {
      addViewportRule(*toStyleRuleViewport(rule), origin);
    } else if (rule->isMediaRule()) {
      StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
      if (!mediaRule->mediaQueries() ||
          m_initialViewportMedium->eval(mediaRule->mediaQueries(),
                                        &m_viewportDependentMediaQueryResults,
                                        &m_deviceDependentMediaQueryResults))
        collectViewportChildRules(mediaRule->childRules(), origin);
    } else if (rule->isSupportsRule()) {
      StyleRuleSupports* supportsRule = toStyleRuleSupports(rule);
      if (supportsRule->conditionIsSupported())
        collectViewportChildRules(supportsRule->childRules(), origin);
    }
  }
}

void ViewportStyleResolver::collectViewportRulesFromImports(
    StyleSheetContents& contents) {
  for (const auto& importRule : contents.importRules()) {
    if (!importRule->styleSheet())
      continue;
    if (!importRule->styleSheet()->hasViewportRule())
      continue;
    if (importRule->mediaQueries() &&
        m_initialViewportMedium->eval(importRule->mediaQueries(),
                                      &m_viewportDependentMediaQueryResults,
                                      &m_deviceDependentMediaQueryResults))
      collectViewportRulesFromAuthorSheetContents(*importRule->styleSheet());
  }
}

void ViewportStyleResolver::collectViewportRulesFromAuthorSheetContents(
    StyleSheetContents& contents) {
  collectViewportRulesFromImports(contents);
  if (contents.hasViewportRule())
    collectViewportChildRules(contents.childRules(), AuthorOrigin);
}

void ViewportStyleResolver::collectViewportRulesFromAuthorSheet(
    const CSSStyleSheet& sheet) {
  DCHECK(sheet.contents());
  StyleSheetContents& contents = *sheet.contents();
  if (!contents.hasViewportRule() && contents.importRules().isEmpty())
    return;
  if (sheet.mediaQueries() &&
      !m_initialViewportMedium->eval(sheet.mediaQueries(),
                                     &m_viewportDependentMediaQueryResults,
                                     &m_deviceDependentMediaQueryResults))
    return;
  collectViewportRulesFromAuthorSheetContents(contents);
}

void ViewportStyleResolver::addViewportRule(StyleRuleViewport& viewportRule,
                                            Origin origin) {
  StylePropertySet& propertySet = viewportRule.mutableProperties();

  unsigned propertyCount = propertySet.propertyCount();
  if (!propertyCount)
    return;

  if (origin == AuthorOrigin)
    m_hasAuthorStyle = true;

  if (!m_propertySet) {
    m_propertySet = propertySet.mutableCopy();
    return;
  }

  // We cannot use mergeAndOverrideOnConflict() here because it doesn't
  // respect the !important declaration (but addRespectingCascade() does).
  for (unsigned i = 0; i < propertyCount; ++i)
    m_propertySet->addRespectingCascade(
        propertySet.propertyAt(i).toCSSProperty());
}

void ViewportStyleResolver::resolve() {
  if (!m_propertySet) {
    m_document->setViewportDescription(
        ViewportDescription(ViewportDescription::UserAgentStyleSheet));
    return;
  }

  ViewportDescription description(
      m_hasAuthorStyle ? ViewportDescription::AuthorStyleSheet
                       : ViewportDescription::UserAgentStyleSheet);

  description.userZoom = viewportArgumentValue(CSSPropertyUserZoom);
  description.zoom = viewportArgumentValue(CSSPropertyZoom);
  description.minZoom = viewportArgumentValue(CSSPropertyMinZoom);
  description.maxZoom = viewportArgumentValue(CSSPropertyMaxZoom);
  description.minWidth = viewportLengthValue(CSSPropertyMinWidth);
  description.maxWidth = viewportLengthValue(CSSPropertyMaxWidth);
  description.minHeight = viewportLengthValue(CSSPropertyMinHeight);
  description.maxHeight = viewportLengthValue(CSSPropertyMaxHeight);
  description.orientation = viewportArgumentValue(CSSPropertyOrientation);

  m_document->setViewportDescription(description);
}

float ViewportStyleResolver::viewportArgumentValue(CSSPropertyID id) const {
  float defaultValue = ViewportDescription::ValueAuto;

  // UserZoom default value is CSSValueZoom, which maps to true, meaning that
  // yes, it is user scalable. When the value is set to CSSValueFixed, we
  // return false.
  if (id == CSSPropertyUserZoom)
    defaultValue = 1;

  const CSSValue* value = m_propertySet->getPropertyCSSValue(id);
  if (!value || !(value->isPrimitiveValue() || value->isIdentifierValue()))
    return defaultValue;

  if (value->isIdentifierValue()) {
    switch (toCSSIdentifierValue(value)->getValueID()) {
      case CSSValueAuto:
        return defaultValue;
      case CSSValueLandscape:
        return ViewportDescription::ValueLandscape;
      case CSSValuePortrait:
        return ViewportDescription::ValuePortrait;
      case CSSValueZoom:
        return defaultValue;
      case CSSValueInternalExtendToZoom:
        return ViewportDescription::ValueExtendToZoom;
      case CSSValueFixed:
        return 0;
      default:
        return defaultValue;
    }
  }

  const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

  if (primitiveValue->isNumber() || primitiveValue->isPx())
    return primitiveValue->getFloatValue();

  if (primitiveValue->isFontRelativeLength())
    return primitiveValue->getFloatValue() *
           m_document->computedStyle()->getFontDescription().computedSize();

  if (primitiveValue->isPercentage()) {
    float percentValue = primitiveValue->getFloatValue() / 100.0f;
    switch (id) {
      case CSSPropertyMaxZoom:
      case CSSPropertyMinZoom:
      case CSSPropertyZoom:
        return percentValue;
      default:
        NOTREACHED();
        break;
    }
  }

  NOTREACHED();
  return defaultValue;
}

Length ViewportStyleResolver::viewportLengthValue(CSSPropertyID id) {
  ASSERT(id == CSSPropertyMaxHeight || id == CSSPropertyMinHeight ||
         id == CSSPropertyMaxWidth || id == CSSPropertyMinWidth);

  const CSSValue* value = m_propertySet->getPropertyCSSValue(id);
  if (!value || !(value->isPrimitiveValue() || value->isIdentifierValue()))
    return Length();  // auto

  if (value->isIdentifierValue()) {
    CSSValueID valueID = toCSSIdentifierValue(value)->getValueID();
    if (valueID == CSSValueInternalExtendToZoom)
      return Length(ExtendToZoom);
    if (valueID == CSSValueAuto)
      return Length(Auto);
  }

  const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
  ComputedStyle* documentStyle = m_document->mutableComputedStyle();

  // If we have viewport units the conversion will mark the document style as
  // having viewport units.
  bool documentStyleHasViewportUnits = documentStyle->hasViewportUnits();
  documentStyle->setHasViewportUnits(false);

  FrameView* view = m_document->frame()->view();
  DCHECK(view);

  CSSToLengthConversionData::FontSizes fontSizes(documentStyle, documentStyle);
  CSSToLengthConversionData::ViewportSize viewportSize(
      view->initialViewportWidth(), view->initialViewportHeight());

  Length result = primitiveValue->convertToLength(
      CSSToLengthConversionData(documentStyle, fontSizes, viewportSize, 1.0f));
  if (documentStyle->hasViewportUnits())
    m_hasViewportUnits = true;
  documentStyle->setHasViewportUnits(documentStyleHasViewportUnits);

  return result;
}

void ViewportStyleResolver::initialViewportChanged() {
  if (m_needsUpdate == CollectRules)
    return;
  if (m_hasViewportUnits)
    m_needsUpdate = Resolve;

  auto& results = m_viewportDependentMediaQueryResults;
  for (unsigned i = 0; i < results.size(); i++) {
    if (m_initialViewportMedium->eval(results[i]->expression()) !=
        results[i]->result()) {
      m_needsUpdate = CollectRules;
      break;
    }
  }
  if (m_needsUpdate == NoUpdate)
    return;
  m_document->scheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::setNeedsCollectRules() {
  m_needsUpdate = CollectRules;
  m_document->scheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::updateViewport(
    DocumentStyleSheetCollection& collection) {
  if (m_needsUpdate == NoUpdate)
    return;
  if (m_needsUpdate == CollectRules) {
    reset();
    collectViewportRulesFromUASheets();
    if (RuntimeEnabledFeatures::cssViewportEnabled())
      collection.collectViewportRules(*this);
  }
  resolve();
  m_needsUpdate = NoUpdate;
}

DEFINE_TRACE(ViewportStyleResolver) {
  visitor->trace(m_document);
  visitor->trace(m_propertySet);
  visitor->trace(m_initialViewportMedium);
  visitor->trace(m_viewportDependentMediaQueryResults);
  visitor->trace(m_deviceDependentMediaQueryResults);
}

}  // namespace blink
