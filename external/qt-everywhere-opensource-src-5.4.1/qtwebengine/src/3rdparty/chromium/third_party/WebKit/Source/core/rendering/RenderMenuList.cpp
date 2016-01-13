/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *               2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "core/rendering/RenderMenuList.h"

#include "core/HTMLNames.h"
#include "core/accessibility/AXMenuList.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/page/Chrome.h"
#include "core/rendering/RenderBR.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/IntSize.h"
#include "platform/text/PlatformLocale.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMenuList::RenderMenuList(Element* element)
    : RenderFlexibleBox(element)
    , m_buttonText(0)
    , m_innerBlock(0)
    , m_optionsChanged(true)
    , m_optionsWidth(0)
    , m_lastActiveIndex(-1)
    , m_popupIsVisible(false)
{
    ASSERT(isHTMLSelectElement(element));
}

RenderMenuList::~RenderMenuList()
{
    if (m_popup)
        m_popup->disconnectClient();
    m_popup = nullptr;
}

// FIXME: Instead of this hack we should add a ShadowRoot to <select> with no insertion point
// to prevent children from rendering.
bool RenderMenuList::isChildAllowed(RenderObject* object, RenderStyle*) const
{
    return object->isAnonymous() && !object->isRenderFullScreen();
}

void RenderMenuList::createInnerBlock()
{
    if (m_innerBlock) {
        ASSERT(firstChild() == m_innerBlock);
        ASSERT(!m_innerBlock->nextSibling());
        return;
    }

    // Create an anonymous block.
    ASSERT(!firstChild());
    m_innerBlock = createAnonymousBlock();
    adjustInnerStyle();
    RenderFlexibleBox::addChild(m_innerBlock);
}

void RenderMenuList::adjustInnerStyle()
{
    RenderStyle* innerStyle = m_innerBlock->style();
    innerStyle->setFlexGrow(1);
    innerStyle->setFlexShrink(1);
    // Use margin:auto instead of align-items:center to get safe centering, i.e.
    // when the content overflows, treat it the same as align-items: flex-start.
    // But we only do that for the cases where html.css would otherwise use center.
    if (style()->alignItems() == ItemPositionCenter) {
        innerStyle->setMarginTop(Length());
        innerStyle->setMarginBottom(Length());
        innerStyle->setAlignSelf(ItemPositionFlexStart);
    }

    innerStyle->setPaddingLeft(Length(RenderTheme::theme().popupInternalPaddingLeft(style()), Fixed));
    innerStyle->setPaddingRight(Length(RenderTheme::theme().popupInternalPaddingRight(style()), Fixed));
    innerStyle->setPaddingTop(Length(RenderTheme::theme().popupInternalPaddingTop(style()), Fixed));
    innerStyle->setPaddingBottom(Length(RenderTheme::theme().popupInternalPaddingBottom(style()), Fixed));

    if (m_optionStyle) {
        if ((m_optionStyle->direction() != innerStyle->direction() || m_optionStyle->unicodeBidi() != innerStyle->unicodeBidi()))
            m_innerBlock->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        innerStyle->setTextAlign(style()->isLeftToRightDirection() ? LEFT : RIGHT);
        innerStyle->setDirection(m_optionStyle->direction());
        innerStyle->setUnicodeBidi(m_optionStyle->unicodeBidi());
    }
}

inline HTMLSelectElement* RenderMenuList::selectElement() const
{
    return toHTMLSelectElement(node());
}

void RenderMenuList::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createInnerBlock();
    m_innerBlock->addChild(newChild, beforeChild);
    ASSERT(m_innerBlock == firstChild());

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this);
}

void RenderMenuList::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_innerBlock || !m_innerBlock) {
        RenderFlexibleBox::removeChild(oldChild);
        m_innerBlock = 0;
    } else
        m_innerBlock->removeChild(oldChild);
}

void RenderMenuList::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_buttonText)
        m_buttonText->setStyle(style());
    if (m_innerBlock) // RenderBlock handled updating the anonymous block's style.
        adjustInnerStyle();

    bool fontChanged = !oldStyle || oldStyle->font() != style()->font();
    if (fontChanged)
        updateOptionsWidth();
}

void RenderMenuList::updateOptionsWidth()
{
    float maxOptionWidth = 0;
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    int size = listItems.size();
    FontCachePurgePreventer fontCachePurgePreventer;

    for (int i = 0; i < size; ++i) {
        HTMLElement* element = listItems[i];
        if (!isHTMLOptionElement(*element))
            continue;

        String text = toHTMLOptionElement(element)->textIndentedToRespectGroupLabel();
        applyTextTransform(style(), text, ' ');
        if (RenderTheme::theme().popupOptionSupportsTextIndent()) {
            // Add in the option's text indent.  We can't calculate percentage values for now.
            float optionWidth = 0;
            if (RenderStyle* optionStyle = element->renderStyle())
                optionWidth += minimumValueForLength(optionStyle->textIndent(), 0);
            if (!text.isEmpty())
                optionWidth += style()->font().width(text);
            maxOptionWidth = max(maxOptionWidth, optionWidth);
        } else if (!text.isEmpty())
            maxOptionWidth = max(maxOptionWidth, style()->font().width(text));
    }

    int width = static_cast<int>(ceilf(maxOptionWidth));
    if (m_optionsWidth == width)
        return;

    m_optionsWidth = width;
    if (parent())
        setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
}

void RenderMenuList::updateFromElement()
{
    if (m_optionsChanged) {
        updateOptionsWidth();
        m_optionsChanged = false;
    }

    if (m_popupIsVisible) {
        m_popup->updateFromElement();
    } else {
        if (selectElement()->suggestedIndex() >= 0)
            setTextFromOption(selectElement()->suggestedIndex());
        else
            setTextFromOption(selectElement()->selectedIndex());
    }
}

void RenderMenuList::setTextFromOption(int optionIndex)
{
    HTMLSelectElement* select = selectElement();
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = select->listItems();
    const int size = listItems.size();

    String text = emptyString();
    m_optionStyle.clear();

    if (multiple()) {
        unsigned selectedCount = 0;
        int firstSelectedIndex = -1;
        for (int i = 0; i < size; ++i) {
            Element* element = listItems[i];
            if (!isHTMLOptionElement(*element))
                continue;

            if (toHTMLOptionElement(element)->selected()) {
                if (++selectedCount == 1)
                    firstSelectedIndex = i;
            }
        }

        if (selectedCount == 1) {
            ASSERT(0 <= firstSelectedIndex);
            ASSERT(firstSelectedIndex < size);
            HTMLOptionElement* selectedOptionElement = toHTMLOptionElement(listItems[firstSelectedIndex]);
            ASSERT(selectedOptionElement->selected());
            text = selectedOptionElement->textIndentedToRespectGroupLabel();
            m_optionStyle = selectedOptionElement->renderStyle();
        } else {
            Locale& locale = select->locale();
            String localizedNumberString = locale.convertToLocalizedNumber(String::number(selectedCount));
            text = locale.queryString(blink::WebLocalizedString::SelectMenuListText, localizedNumberString);
            ASSERT(!m_optionStyle);
        }
    } else {
        const int i = select->optionToListIndex(optionIndex);
        if (i >= 0 && i < size) {
            Element* element = listItems[i];
            if (isHTMLOptionElement(*element)) {
                text = toHTMLOptionElement(element)->textIndentedToRespectGroupLabel();
                m_optionStyle = element->renderStyle();
            }
        }
    }

    setText(text.stripWhiteSpace());

    didUpdateActiveOption(optionIndex);
}

void RenderMenuList::setText(const String& s)
{
    if (s.isEmpty()) {
        if (!m_buttonText || !m_buttonText->isBR()) {
            // FIXME: We should not modify the structure of the render tree
            // during layout. crbug.com/370462
            DeprecatedDisableModifyRenderTreeStructureAsserts disabler;
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new RenderBR(&document());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    } else {
        if (m_buttonText && !m_buttonText->isBR())
            m_buttonText->setText(s.impl(), true);
        else {
            // FIXME: We should not modify the structure of the render tree
            // during layout. crbug.com/370462
            DeprecatedDisableModifyRenderTreeStructureAsserts disabler;
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new RenderText(&document(), s.impl());
            m_buttonText->setStyle(style());
            // We need to set the text explicitly though it was specified in the
            // constructor because RenderText doesn't refer to the text
            // specified in the constructor in a case of re-transforming.
            m_buttonText->setText(s.impl(), true);
            addChild(m_buttonText);
        }
        adjustInnerStyle();
    }
}

String RenderMenuList::text() const
{
    return m_buttonText ? m_buttonText->text() : String();
}

LayoutRect RenderMenuList::controlClipRect(const LayoutPoint& additionalOffset) const
{
    // Clip to the intersection of the content box and the content box for the inner box
    // This will leave room for the arrows which sit in the inner box padding,
    // and if the inner box ever spills out of the outer box, that will get clipped too.
    LayoutRect outerBox(additionalOffset.x() + borderLeft() + paddingLeft(),
                   additionalOffset.y() + borderTop() + paddingTop(),
                   contentWidth(),
                   contentHeight());

    LayoutRect innerBox(additionalOffset.x() + m_innerBlock->x() + m_innerBlock->paddingLeft(),
                   additionalOffset.y() + m_innerBlock->y() + m_innerBlock->paddingTop(),
                   m_innerBlock->contentWidth(),
                   m_innerBlock->contentHeight());

    return intersection(outerBox, innerBox);
}

void RenderMenuList::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    maxLogicalWidth = max(m_optionsWidth, RenderTheme::theme().minimumMenuListSize(style())) + m_innerBlock->paddingLeft() + m_innerBlock->paddingRight();
    if (!style()->width().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderMenuList::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;
    RenderStyle* styleToUse = style();

    if (styleToUse->width().isFixed() && styleToUse->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse->width().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse->minWidth().isFixed() && styleToUse->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->minWidth().value()));
    }

    if (styleToUse->maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->maxWidth().value()));
    }

    LayoutUnit toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    clearPreferredLogicalWidthsDirty();
}

void RenderMenuList::showPopup()
{
    if (m_popupIsVisible)
        return;

    if (document().frameHost()->chrome().hasOpenedPopup())
        return;

    // Create m_innerBlock here so it ends up as the first child.
    // This is important because otherwise we might try to create m_innerBlock
    // inside the showPopup call and it would fail.
    createInnerBlock();
    if (!m_popup)
        m_popup = document().frameHost()->chrome().createPopupMenu(*document().frame(), this);
    m_popupIsVisible = true;

    FloatQuad quad(localToAbsoluteQuad(FloatQuad(borderBoundingBox())));
    IntSize size = pixelSnappedIntRect(frameRect()).size();
    HTMLSelectElement* select = selectElement();
    m_popup->show(quad, size, select->optionToListIndex(select->selectedIndex()));
}

void RenderMenuList::hidePopup()
{
    if (m_popup)
        m_popup->hide();
}

void RenderMenuList::valueChanged(unsigned listIndex, bool fireOnChange)
{
    // Check to ensure a page navigation has not occurred while
    // the popup was up.
    Document& doc = toElement(node())->document();
    if (&doc != doc.frame()->document())
        return;

    HTMLSelectElement* select = selectElement();
    select->optionSelectedByUser(select->listToOptionIndex(listIndex), fireOnChange);
}

void RenderMenuList::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    selectElement()->listBoxSelectItem(listIndex, allowMultiplySelections, shift, fireOnChangeNow);
}

bool RenderMenuList::multiple() const
{
    return selectElement()->multiple();
}

void RenderMenuList::didSetSelectedIndex(int listIndex)
{
    didUpdateActiveOption(selectElement()->listToOptionIndex(listIndex));
}

void RenderMenuList::didUpdateActiveOption(int optionIndex)
{
    if (!AXObjectCache::accessibilityEnabled() || !document().existingAXObjectCache())
        return;

    if (m_lastActiveIndex == optionIndex)
        return;
    m_lastActiveIndex = optionIndex;

    HTMLSelectElement* select = selectElement();
    int listIndex = select->optionToListIndex(optionIndex);
    if (listIndex < 0 || listIndex >= static_cast<int>(select->listItems().size()))
        return;
    if (AXMenuList* menuList = toAXMenuList(document().axObjectCache()->get(this)))
        menuList->didUpdateActiveOption(optionIndex);
}

String RenderMenuList::itemText(unsigned listIndex) const
{
    HTMLSelectElement* select = selectElement();
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = select->listItems();
    if (listIndex >= listItems.size())
        return String();

    String itemString;
    Element* element = listItems[listIndex];
    if (isHTMLOptGroupElement(*element))
        itemString = toHTMLOptGroupElement(*element).groupLabelText();
    else if (isHTMLOptionElement(*element))
        itemString = toHTMLOptionElement(*element).textIndentedToRespectGroupLabel();

    applyTextTransform(style(), itemString, ' ');
    return itemString;
}

String RenderMenuList::itemAccessibilityText(unsigned listIndex) const
{
    // Allow the accessible name be changed if necessary.
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size())
        return String();
    return listItems[listIndex]->fastGetAttribute(aria_labelAttr);
}

String RenderMenuList::itemToolTip(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size())
        return String();
    return listItems[listIndex]->title();
}

bool RenderMenuList::itemIsEnabled(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size())
        return false;
    HTMLElement* element = listItems[listIndex];
    if (!isHTMLOptionElement(*element))
        return false;

    bool groupEnabled = true;
    if (Element* parentElement = element->parentElement()) {
        if (isHTMLOptGroupElement(*parentElement))
            groupEnabled = !parentElement->isDisabledFormControl();
    }
    if (!groupEnabled)
        return false;

    return !element->isDisabledFormControl();
}

PopupMenuStyle RenderMenuList::itemStyle(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size()) {
        // If we are making an out of bounds access, then we want to use the style
        // of a different option element (index 0). However, if there isn't an option element
        // before at index 0, we fall back to the menu's style.
        if (!listIndex)
            return menuStyle();

        // Try to retrieve the style of an option element we know exists (index 0).
        listIndex = 0;
    }
    HTMLElement* element = listItems[listIndex];

    Color itemBackgroundColor;
    bool itemHasCustomBackgroundColor;
    getItemBackgroundColor(listIndex, itemBackgroundColor, itemHasCustomBackgroundColor);

    RenderStyle* style = element->renderStyle() ? element->renderStyle() : element->computedStyle();
    return style ? PopupMenuStyle(resolveColor(style, CSSPropertyColor), itemBackgroundColor, style->font(), style->visibility() == VISIBLE,
        style->display() == NONE, style->textIndent(), style->direction(), isOverride(style->unicodeBidi()),
        itemHasCustomBackgroundColor ? PopupMenuStyle::CustomBackgroundColor : PopupMenuStyle::DefaultBackgroundColor) : menuStyle();
}

void RenderMenuList::getItemBackgroundColor(unsigned listIndex, Color& itemBackgroundColor, bool& itemHasCustomBackgroundColor) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size()) {
        itemBackgroundColor = resolveColor(CSSPropertyBackgroundColor);
        itemHasCustomBackgroundColor = false;
        return;
    }
    HTMLElement* element = listItems[listIndex];

    Color backgroundColor;
    if (element->renderStyle())
        backgroundColor = resolveColor(element->renderStyle(), CSSPropertyBackgroundColor);
    itemHasCustomBackgroundColor = backgroundColor.alpha();
    // If the item has an opaque background color, return that.
    if (!backgroundColor.hasAlpha()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // Otherwise, the item's background is overlayed on top of the menu background.
    backgroundColor = resolveColor(CSSPropertyBackgroundColor).blend(backgroundColor);
    if (!backgroundColor.hasAlpha()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // If the menu background is not opaque, then add an opaque white background behind.
    itemBackgroundColor = Color(Color::white).blend(backgroundColor);
}

PopupMenuStyle RenderMenuList::menuStyle() const
{
    const RenderObject* o = m_innerBlock ? m_innerBlock : this;
    const RenderStyle* s = o->style();
    return PopupMenuStyle(o->resolveColor(CSSPropertyColor), o->resolveColor(CSSPropertyBackgroundColor), s->font(), s->visibility() == VISIBLE,
        s->display() == NONE, s->textIndent(), style()->direction(), isOverride(style()->unicodeBidi()));
}

LayoutUnit RenderMenuList::clientPaddingLeft() const
{
    return paddingLeft() + m_innerBlock->paddingLeft();
}

const int endOfLinePadding = 2;
LayoutUnit RenderMenuList::clientPaddingRight() const
{
    if (style()->appearance() == MenulistPart || style()->appearance() == MenulistButtonPart) {
        // For these appearance values, the theme applies padding to leave room for the
        // drop-down button. But leaving room for the button inside the popup menu itself
        // looks strange, so we return a small default padding to avoid having a large empty
        // space appear on the side of the popup menu.
        return endOfLinePadding;
    }

    // If the appearance isn't MenulistPart, then the select is styled (non-native), so
    // we want to return the user specified padding.
    return paddingRight() + m_innerBlock->paddingRight();
}

int RenderMenuList::listSize() const
{
    return selectElement()->listItems().size();
}

int RenderMenuList::selectedIndex() const
{
    HTMLSelectElement* select = selectElement();
    return select->optionToListIndex(select->selectedIndex());
}

void RenderMenuList::popupDidHide()
{
    m_popupIsVisible = false;
}

bool RenderMenuList::itemIsSeparator(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    return listIndex < listItems.size() && isHTMLHRElement(*listItems[listIndex]);
}

bool RenderMenuList::itemIsLabel(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    return listIndex < listItems.size() && isHTMLOptGroupElement(*listItems[listIndex]);
}

bool RenderMenuList::itemIsSelected(unsigned listIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement()->listItems();
    if (listIndex >= listItems.size())
        return false;
    HTMLElement* element = listItems[listIndex];
    return isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected();
}

void RenderMenuList::setTextFromItem(unsigned listIndex)
{
    setTextFromOption(selectElement()->listToOptionIndex(listIndex));
}

}
