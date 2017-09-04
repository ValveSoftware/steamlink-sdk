/*
 * This file is part of the select element layoutObject in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 *           (C) 2009 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#include "core/layout/LayoutMenuList.h"

#include "core/dom/AXObjectCache.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTheme.h"
#include "platform/text/PlatformLocale.h"
#include <math.h>

namespace blink {

LayoutMenuList::LayoutMenuList(Element* element)
    : LayoutFlexibleBox(element),
      m_buttonText(nullptr),
      m_innerBlock(nullptr),
      m_isEmpty(false),
      m_hasUpdatedActiveOption(false),
      m_innerBlockHeight(LayoutUnit()),
      m_optionsWidth(0),
      m_lastActiveIndex(-1) {
  ASSERT(isHTMLSelectElement(element));
}

LayoutMenuList::~LayoutMenuList() {}

// FIXME: Instead of this hack we should add a ShadowRoot to <select> with no
// insertion point to prevent children from rendering.
bool LayoutMenuList::isChildAllowed(LayoutObject* object,
                                    const ComputedStyle&) const {
  return object->isAnonymous() && !object->isLayoutFullScreen();
}

void LayoutMenuList::createInnerBlock() {
  if (m_innerBlock) {
    ASSERT(firstChild() == m_innerBlock);
    ASSERT(!m_innerBlock->nextSibling());
    return;
  }

  // Create an anonymous block.
  ASSERT(!firstChild());
  m_innerBlock = createAnonymousBlock();

  m_buttonText = new LayoutText(&document(), StringImpl::empty());
  // We need to set the text explicitly though it was specified in the
  // constructor because LayoutText doesn't refer to the text
  // specified in the constructor in a case of re-transforming.
  m_buttonText->setStyle(mutableStyle());
  m_innerBlock->addChild(m_buttonText);

  adjustInnerStyle();
  LayoutFlexibleBox::addChild(m_innerBlock);
}

void LayoutMenuList::adjustInnerStyle() {
  ComputedStyle& innerStyle = m_innerBlock->mutableStyleRef();
  innerStyle.setFlexGrow(1);
  innerStyle.setFlexShrink(1);
  // min-width: 0; is needed for correct shrinking.
  innerStyle.setMinWidth(Length(0, Fixed));
  // Use margin:auto instead of align-items:center to get safe centering, i.e.
  // when the content overflows, treat it the same as align-items: flex-start.
  // But we only do that for the cases where html.css would otherwise use
  // center.
  if (style()->alignItemsPosition() == ItemPositionCenter) {
    innerStyle.setMarginTop(Length());
    innerStyle.setMarginBottom(Length());
    innerStyle.setAlignSelfPosition(ItemPositionFlexStart);
  }

  Length paddingStart =
      Length(LayoutTheme::theme().popupInternalPaddingStart(styleRef()), Fixed);
  Length paddingEnd =
      Length(LayoutTheme::theme().popupInternalPaddingEnd(styleRef()), Fixed);
  innerStyle.setPaddingLeft(styleRef().direction() == LTR ? paddingStart
                                                          : paddingEnd);
  innerStyle.setPaddingRight(styleRef().direction() == LTR ? paddingEnd
                                                           : paddingStart);
  innerStyle.setPaddingTop(
      Length(LayoutTheme::theme().popupInternalPaddingTop(styleRef()), Fixed));
  innerStyle.setPaddingBottom(Length(
      LayoutTheme::theme().popupInternalPaddingBottom(styleRef()), Fixed));

  if (m_optionStyle) {
    if ((m_optionStyle->direction() != innerStyle.direction() ||
         m_optionStyle->unicodeBidi() != innerStyle.unicodeBidi()))
      m_innerBlock->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::StyleChange);
    innerStyle.setTextAlign(style()->isLeftToRightDirection()
                                ? ETextAlign::Left
                                : ETextAlign::Right);
    innerStyle.setDirection(m_optionStyle->direction());
    innerStyle.setUnicodeBidi(m_optionStyle->unicodeBidi());
  }
}

HTMLSelectElement* LayoutMenuList::selectElement() const {
  return toHTMLSelectElement(node());
}

void LayoutMenuList::addChild(LayoutObject* newChild,
                              LayoutObject* beforeChild) {
  m_innerBlock->addChild(newChild, beforeChild);
  ASSERT(m_innerBlock == firstChild());

  if (AXObjectCache* cache = document().existingAXObjectCache())
    cache->childrenChanged(this);
}

void LayoutMenuList::removeChild(LayoutObject* oldChild) {
  if (oldChild == m_innerBlock || !m_innerBlock) {
    LayoutFlexibleBox::removeChild(oldChild);
    m_innerBlock = nullptr;
  } else {
    m_innerBlock->removeChild(oldChild);
  }
}

void LayoutMenuList::styleDidChange(StyleDifference diff,
                                    const ComputedStyle* oldStyle) {
  LayoutBlock::styleDidChange(diff, oldStyle);

  if (!m_innerBlock)
    createInnerBlock();

  m_buttonText->setStyle(mutableStyle());
  adjustInnerStyle();
  updateInnerBlockHeight();
}

void LayoutMenuList::updateInnerBlockHeight() {
  const SimpleFontData* fontData = style()->font().primaryFont();
  DCHECK(fontData);
  m_innerBlockHeight = (fontData ? fontData->getFontMetrics().height() : 0) +
                       m_innerBlock->borderAndPaddingHeight();
}

void LayoutMenuList::updateOptionsWidth() const {
  float maxOptionWidth = 0;

  for (const auto& option : selectElement()->optionList()) {
    String text = option->textIndentedToRespectGroupLabel();
    const ComputedStyle* itemStyle =
        option->computedStyle() ? option->computedStyle() : style();
    applyTextTransform(itemStyle, text, ' ');
    // We apply SELECT's style, not OPTION's style because m_optionsWidth is
    // used to determine intrinsic width of the menulist box.
    TextRun textRun = constructTextRun(style()->font(), text, *style());
    maxOptionWidth = std::max(maxOptionWidth, style()->font().width(textRun));
  }
  m_optionsWidth = static_cast<int>(ceilf(maxOptionWidth));
}

void LayoutMenuList::updateFromElement() {
  HTMLSelectElement* select = selectElement();
  HTMLOptionElement* option = select->optionToBeShown();
  String text = emptyString();
  m_optionStyle.clear();

  if (select->isMultiple()) {
    unsigned selectedCount = 0;
    HTMLOptionElement* selectedOptionElement = nullptr;
    for (const auto& option : select->optionList()) {
      if (option->selected()) {
        if (++selectedCount == 1)
          selectedOptionElement = option;
      }
    }

    if (selectedCount == 1) {
      text = selectedOptionElement->textIndentedToRespectGroupLabel();
      m_optionStyle = selectedOptionElement->mutableComputedStyle();
    } else {
      Locale& locale = select->locale();
      String localizedNumberString =
          locale.convertToLocalizedNumber(String::number(selectedCount));
      text = locale.queryString(WebLocalizedString::SelectMenuListText,
                                localizedNumberString);
      ASSERT(!m_optionStyle);
    }
  } else {
    if (option) {
      text = option->textIndentedToRespectGroupLabel();
      m_optionStyle = option->mutableComputedStyle();
    }
  }

  setText(text.stripWhiteSpace());

  didUpdateActiveOption(option);
}

void LayoutMenuList::setText(const String& s) {
  if (s.isEmpty()) {
    // FIXME: This is a hack. We need the select to have the same baseline
    // positioning as any surrounding text. Wihtout any content, we align the
    // bottom of the select to the bottom of the text. With content (In this
    // case the faked " ") we correctly align the middle of the select to the
    // middle of the text. It should be possible to remove this, just set
    // s.impl() into the text and have things align correctly...
    // crbug.com/485982
    m_isEmpty = true;
    m_buttonText->setText(StringImpl::create(" ", 1), true);
  } else {
    m_isEmpty = false;
    m_buttonText->setText(s.impl(), true);
  }
  adjustInnerStyle();
}

String LayoutMenuList::text() const {
  return m_buttonText && !m_isEmpty ? m_buttonText->text() : String();
}

LayoutRect LayoutMenuList::controlClipRect(
    const LayoutPoint& additionalOffset) const {
  // Clip to the intersection of the content box and the content box for the
  // inner box. This will leave room for the arrows which sit in the inner box
  // padding, and if the inner box ever spills out of the outer box, that will
  // get clipped too.
  LayoutRect outerBox = contentBoxRect();
  outerBox.moveBy(additionalOffset);

  LayoutRect innerBox(
      additionalOffset + m_innerBlock->location() +
          LayoutSize(m_innerBlock->paddingLeft(), m_innerBlock->paddingTop()),
      m_innerBlock->contentSize());

  return intersection(outerBox, innerBox);
}

void LayoutMenuList::computeIntrinsicLogicalWidths(
    LayoutUnit& minLogicalWidth,
    LayoutUnit& maxLogicalWidth) const {
  updateOptionsWidth();

  maxLogicalWidth =
      std::max(m_optionsWidth,
               LayoutTheme::theme().minimumMenuListSize(styleRef())) +
      m_innerBlock->paddingLeft() + m_innerBlock->paddingRight();
  if (!style()->width().isPercentOrCalc())
    minLogicalWidth = maxLogicalWidth;
  else
    minLogicalWidth = LayoutUnit();
}

void LayoutMenuList::computeLogicalHeight(
    LayoutUnit logicalHeight,
    LayoutUnit logicalTop,
    LogicalExtentComputedValues& computedValues) const {
  if (style()->hasAppearance())
    logicalHeight = m_innerBlockHeight + borderAndPaddingHeight();
  LayoutBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void LayoutMenuList::didSelectOption(HTMLOptionElement* option) {
  didUpdateActiveOption(option);
}

void LayoutMenuList::didUpdateActiveOption(HTMLOptionElement* option) {
  if (!document().existingAXObjectCache())
    return;

  int optionIndex = option ? option->index() : -1;
  if (m_lastActiveIndex == optionIndex)
    return;
  m_lastActiveIndex = optionIndex;

  if (optionIndex < 0)
    return;

  // We skip sending accessiblity notifications for the very first option,
  // otherwise we get extra focus and select events that are undesired.
  if (!m_hasUpdatedActiveOption) {
    m_hasUpdatedActiveOption = true;
    return;
  }

  document().existingAXObjectCache()->handleUpdateActiveMenuOption(this,
                                                                   optionIndex);
}

LayoutUnit LayoutMenuList::clientPaddingLeft() const {
  return paddingLeft() + m_innerBlock->paddingLeft();
}

LayoutUnit LayoutMenuList::clientPaddingRight() const {
  return paddingRight() + m_innerBlock->paddingRight();
}

}  // namespace blink
