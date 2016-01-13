/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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
#include "web/PopupListBox.h"

#include "core/CSSValueKeywords.h"
#include "core/rendering/RenderTheme.h"
#include "platform/KeyboardCodes.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformScreen.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/PopupMenuClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontSelector.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/scroll/FramelessScrollViewClient.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/StringTruncator.h"
#include "platform/text/TextRun.h"
#include "web/PopupContainer.h"
#include "web/PopupMenuChromium.h"
#include "wtf/ASCIICType.h"
#include "wtf/CurrentTime.h"
#include <limits>

namespace blink {

using namespace WTF::Unicode;
using namespace WebCore;

const int PopupListBox::defaultMaxHeight = 500;
static const int maxVisibleRows = 20;
static const int minEndOfLinePadding = 2;
static const TimeStamp typeAheadTimeoutMs = 1000;

PopupListBox::PopupListBox(PopupMenuClient* client, bool deviceSupportsTouch)
    : m_deviceSupportsTouch(deviceSupportsTouch)
    , m_originalIndex(0)
    , m_selectedIndex(0)
    , m_acceptedIndexOnAbandon(-1)
    , m_visibleRows(0)
    , m_baseWidth(0)
    , m_maxHeight(defaultMaxHeight)
    , m_popupClient(client)
    , m_repeatingChar(0)
    , m_lastCharTime(0)
    , m_maxWindowWidth(std::numeric_limits<int>::max())
{
    setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
}

bool PopupListBox::handleMouseDownEvent(const PlatformMouseEvent& event)
{
    Scrollbar* scrollbar = scrollbarAtPoint(event.position());
    if (scrollbar) {
        m_capturingScrollbar = scrollbar;
        m_capturingScrollbar->mouseDown(event);
        return true;
    }

    if (!isPointInBounds(event.position()))
        abandon();

    return true;
}

bool PopupListBox::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    if (m_capturingScrollbar) {
        m_capturingScrollbar->mouseMoved(event);
        return true;
    }

    Scrollbar* scrollbar = scrollbarAtPoint(event.position());
    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->mouseExited();
        m_lastScrollbarUnderMouse = scrollbar;
    }

    if (scrollbar) {
        scrollbar->mouseMoved(event);
        return true;
    }

    if (!isPointInBounds(event.position()))
        return false;

    selectIndex(pointToRowIndex(event.position()));
    return true;
}

bool PopupListBox::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    if (m_capturingScrollbar) {
        m_capturingScrollbar->mouseUp(event);
        m_capturingScrollbar = nullptr;
        return true;
    }

    if (!isPointInBounds(event.position()))
        return true;

    if (acceptIndex(pointToRowIndex(event.position())) && m_focusedElement) {
        m_focusedElement->dispatchMouseEvent(event, EventTypeNames::mouseup);
        m_focusedElement->dispatchMouseEvent(event, EventTypeNames::click);

        // Clear m_focusedElement here, because we cannot clear in hidePopup()
        // which is called before dispatchMouseEvent() is called.
        m_focusedElement = nullptr;
    }

    return true;
}

bool PopupListBox::handleWheelEvent(const PlatformWheelEvent& event)
{
    if (!isPointInBounds(event.position())) {
        abandon();
        return true;
    }

    ScrollableArea::handleWheelEvent(event);
    return true;
}

// Should be kept in sync with handleKeyEvent().
bool PopupListBox::isInterestedInEventForKey(int keyCode)
{
    switch (keyCode) {
    case VKEY_ESCAPE:
    case VKEY_RETURN:
    case VKEY_UP:
    case VKEY_DOWN:
    case VKEY_PRIOR:
    case VKEY_NEXT:
    case VKEY_HOME:
    case VKEY_END:
    case VKEY_TAB:
        return true;
    default:
        return false;
    }
}

bool PopupListBox::handleTouchEvent(const PlatformTouchEvent&)
{
    return false;
}

bool PopupListBox::handleGestureEvent(const PlatformGestureEvent&)
{
    return false;
}

static bool isCharacterTypeEvent(const PlatformKeyboardEvent& event)
{
    // Check whether the event is a character-typed event or not.
    // We use RawKeyDown/Char/KeyUp event scheme on all platforms,
    // so PlatformKeyboardEvent::Char (not RawKeyDown) type event
    // is considered as character type event.
    return event.type() == PlatformEvent::Char;
}

bool PopupListBox::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    if (event.type() == PlatformEvent::KeyUp)
        return true;

    if (!numItems() && event.windowsVirtualKeyCode() != VKEY_ESCAPE)
        return true;

    switch (event.windowsVirtualKeyCode()) {
    case VKEY_ESCAPE:
        abandon(); // may delete this
        return true;
    case VKEY_RETURN:
        if (m_selectedIndex == -1)  {
            hidePopup();
            // Don't eat the enter if nothing is selected.
            return false;
        }
        acceptIndex(m_selectedIndex); // may delete this
        return true;
    case VKEY_UP:
        selectPreviousRow();
        break;
    case VKEY_DOWN:
        selectNextRow();
        break;
    case VKEY_PRIOR:
        adjustSelectedIndex(-m_visibleRows);
        break;
    case VKEY_NEXT:
        adjustSelectedIndex(m_visibleRows);
        break;
    case VKEY_HOME:
        adjustSelectedIndex(-m_selectedIndex);
        break;
    case VKEY_END:
        adjustSelectedIndex(m_items.size());
        break;
    default:
        if (!event.ctrlKey() && !event.altKey() && !event.metaKey()
            && isPrintableChar(event.windowsVirtualKeyCode())
            && isCharacterTypeEvent(event))
            typeAheadFind(event);
        break;
    }

    if (m_originalIndex != m_selectedIndex) {
        // Keyboard events should update the selection immediately (but we don't
        // want to fire the onchange event until the popup is closed, to match
        // IE). We change the original index so we revert to that when the
        // popup is closed.
        m_acceptedIndexOnAbandon = m_selectedIndex;

        setOriginalIndex(m_selectedIndex);
        m_popupClient->setTextFromItem(m_selectedIndex);
    }
    if (event.windowsVirtualKeyCode() == VKEY_TAB) {
        // TAB is a special case as it should select the current item if any and
        // advance focus.
        if (m_selectedIndex >= 0) {
            acceptIndex(m_selectedIndex); // May delete us.
            // Return false so the TAB key event is propagated to the page.
            return false;
        }
        // Call abandon() so we honor m_acceptedIndexOnAbandon if set.
        abandon();
        // Return false so the TAB key event is propagated to the page.
        return false;
    }

    return true;
}

HostWindow* PopupListBox::hostWindow() const
{
    // Our parent is the root ScrollView, so it is the one that has a
    // HostWindow. FrameView::hostWindow() works similarly.
    return parent() ? parent()->hostWindow() : 0;
}

bool PopupListBox::shouldPlaceVerticalScrollbarOnLeft() const
{
    return m_popupClient->menuStyle().textDirection() == RTL;
}

// From HTMLSelectElement.cpp
static String stripLeadingWhiteSpace(const String& string)
{
    int length = string.length();
    int i;
    for (i = 0; i < length; ++i)
        if (string[i] != noBreakSpace
            && !isSpaceOrNewline(string[i]))
            break;

    return string.substring(i, length - i);
}

// From HTMLSelectElement.cpp, with modifications
void PopupListBox::typeAheadFind(const PlatformKeyboardEvent& event)
{
    TimeStamp now = static_cast<TimeStamp>(currentTime() * 1000.0f);
    TimeStamp delta = now - m_lastCharTime;

    // Reset the time when user types in a character. The time gap between
    // last character and the current character is used to indicate whether
    // user typed in a string or just a character as the search prefix.
    m_lastCharTime = now;

    UChar c = event.windowsVirtualKeyCode();

    String prefix;
    int searchStartOffset = 1;
    if (delta > typeAheadTimeoutMs) {
        m_typedString = prefix = String(&c, 1);
        m_repeatingChar = c;
    } else {
        m_typedString.append(c);

        if (c == m_repeatingChar) {
            // The user is likely trying to cycle through all the items starting
            // with this character, so just search on the character.
            prefix = String(&c, 1);
        } else {
            m_repeatingChar = 0;
            prefix = m_typedString;
            searchStartOffset = 0;
        }
    }

    // Compute a case-folded copy of the prefix string before beginning the
    // search for a matching element. This code uses foldCase to work around the
    // fact that String::startWith does not fold non-ASCII characters. This code
    // can be changed to use startWith once that is fixed.
    String prefixWithCaseFolded(prefix.foldCase());
    int itemCount = numItems();
    int index = (max(0, m_selectedIndex) + searchStartOffset) % itemCount;
    for (int i = 0; i < itemCount; i++, index = (index + 1) % itemCount) {
        if (!isSelectableItem(index))
            continue;

        if (stripLeadingWhiteSpace(m_items[index]->label).foldCase().startsWith(prefixWithCaseFolded)) {
            selectIndex(index);
            return;
        }
    }
}

void PopupListBox::paint(GraphicsContext* gc, const IntRect& rect)
{
    // Adjust coords for scrolled frame.
    IntRect r = intersection(rect, frameRect());
    int tx = x() - scrollX() + ((shouldPlaceVerticalScrollbarOnLeft() && verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0);
    int ty = y() - scrollY();

    r.move(-tx, -ty);

    // Set clip rect to match revised damage rect.
    gc->save();
    gc->translate(static_cast<float>(tx), static_cast<float>(ty));
    gc->clip(r);

    // FIXME: Can we optimize scrolling to not require repainting the entire
    // window? Should we?
    for (int i = 0; i < numItems(); ++i)
        paintRow(gc, r, i);

    // Special case for an empty popup.
    if (!numItems())
        gc->fillRect(r, Color::white);

    gc->restore();

    ScrollView::paint(gc, rect);
}

static const int separatorPadding = 4;
static const int separatorHeight = 1;
static const int minRowHeight = 0;
static const int optionRowHeightForTouch = 28;

void PopupListBox::paintRow(GraphicsContext* gc, const IntRect& rect, int rowIndex)
{
    // This code is based largely on RenderListBox::paint* methods.

    IntRect rowRect = getRowBounds(rowIndex);
    if (!rowRect.intersects(rect))
        return;

    PopupMenuStyle style = m_popupClient->itemStyle(rowIndex);

    // Paint background
    Color backColor, textColor, labelColor;
    if (rowIndex == m_selectedIndex) {
        backColor = RenderTheme::theme().activeListBoxSelectionBackgroundColor();
        textColor = RenderTheme::theme().activeListBoxSelectionForegroundColor();
        labelColor = textColor;
    } else {
        backColor = style.backgroundColor();
        textColor = style.foregroundColor();

#if OS(LINUX) || OS(ANDROID)
        // On other platforms, the <option> background color is the same as the
        // <select> background color. On Linux, that makes the <option>
        // background color very dark, so by default, try to use a lighter
        // background color for <option>s.
        if (style.backgroundColorType() == PopupMenuStyle::DefaultBackgroundColor && RenderTheme::theme().systemColor(CSSValueButtonface) == backColor)
            backColor = RenderTheme::theme().systemColor(CSSValueMenu);
#endif

        // FIXME: for now the label color is hard-coded. It should be added to
        // the PopupMenuStyle.
        labelColor = Color(115, 115, 115);
    }

    // If we have a transparent background, make sure it has a color to blend
    // against.
    if (backColor.hasAlpha())
        gc->fillRect(rowRect, Color::white);

    gc->fillRect(rowRect, backColor);

    if (m_popupClient->itemIsSeparator(rowIndex)) {
        IntRect separatorRect(
            rowRect.x() + separatorPadding,
            rowRect.y() + (rowRect.height() - separatorHeight) / 2,
            rowRect.width() - 2 * separatorPadding, separatorHeight);
        gc->fillRect(separatorRect, textColor);
        return;
    }

    if (!style.isVisible())
        return;

    gc->setFillColor(textColor);

    FontCachePurgePreventer fontCachePurgePreventer;

    Font itemFont = getRowFont(rowIndex);
    // FIXME: http://crbug.com/19872 We should get the padding of individual option
    // elements. This probably implies changes to PopupMenuClient.
    bool rightAligned = m_popupClient->menuStyle().textDirection() == RTL;
    int textX = 0;
    int maxWidth = 0;
    if (rightAligned) {
        maxWidth = rowRect.width() - max<int>(0, m_popupClient->clientPaddingRight());
    } else {
        textX = max<int>(0, m_popupClient->clientPaddingLeft());
        maxWidth = rowRect.width() - textX;
    }
    // Prepare text to be drawn.
    String itemText = m_popupClient->itemText(rowIndex);

    // Prepare the directionality to draw text.
    TextRun textRun(itemText, 0, 0, TextRun::AllowTrailingExpansion, style.textDirection(), style.hasTextDirectionOverride());
    // If the text is right-to-left, make it right-aligned by adjusting its
    // beginning position.
    if (rightAligned)
        textX += maxWidth - itemFont.width(textRun);

    // Draw the item text.
    int textY = rowRect.y() + itemFont.fontMetrics().ascent() + (rowRect.height() - itemFont.fontMetrics().height()) / 2;
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = rowRect;
    gc->drawBidiText(itemFont, textRunPaintInfo, IntPoint(textX, textY));
}

Font PopupListBox::getRowFont(int rowIndex)
{
    Font itemFont = m_popupClient->itemStyle(rowIndex).font();
    if (m_popupClient->itemIsLabel(rowIndex)) {
        // Bold-ify labels (ie, an <optgroup> heading).
        FontDescription d = itemFont.fontDescription();
        d.setWeight(FontWeightBold);
        Font font(d);
        font.update(nullptr);
        return font;
    }

    return itemFont;
}

void PopupListBox::abandon()
{
    RefPtr<PopupListBox> keepAlive(this);

    m_selectedIndex = m_originalIndex;

    hidePopup();

    if (m_acceptedIndexOnAbandon >= 0) {
        if (m_popupClient)
            m_popupClient->valueChanged(m_acceptedIndexOnAbandon);
        m_acceptedIndexOnAbandon = -1;
    }
}

int PopupListBox::pointToRowIndex(const IntPoint& point)
{
    int y = scrollY() + point.y();

    // FIXME: binary search if perf matters.
    for (int i = 0; i < numItems(); ++i) {
        if (y < m_items[i]->yOffset)
            return i-1;
    }

    // Last item?
    if (y < contentsHeight())
        return m_items.size()-1;

    return -1;
}

bool PopupListBox::acceptIndex(int index)
{
    // Clear m_acceptedIndexOnAbandon once user accepts the selected index.
    if (m_acceptedIndexOnAbandon >= 0)
        m_acceptedIndexOnAbandon = -1;

    if (index >= numItems())
        return false;

    if (index < 0) {
        if (m_popupClient) {
            // Enter pressed with no selection, just close the popup.
            hidePopup();
        }
        return false;
    }

    if (isSelectableItem(index)) {
        RefPtr<PopupListBox> keepAlive(this);

        // Hide ourselves first since valueChanged may have numerous side-effects.
        hidePopup();

        // Tell the <select> PopupMenuClient what index was selected.
        m_popupClient->valueChanged(index);

        return true;
    }

    return false;
}

void PopupListBox::selectIndex(int index)
{
    if (index < 0 || index >= numItems())
        return;

    bool isSelectable = isSelectableItem(index);
    if (index != m_selectedIndex && isSelectable) {
        invalidateRow(m_selectedIndex);
        m_selectedIndex = index;
        invalidateRow(m_selectedIndex);

        scrollToRevealSelection();
        m_popupClient->selectionChanged(m_selectedIndex);
    } else if (!isSelectable)
        clearSelection();
}

void PopupListBox::setOriginalIndex(int index)
{
    m_originalIndex = m_selectedIndex = index;
}

int PopupListBox::getRowHeight(int index)
{
    int minimumHeight = m_deviceSupportsTouch ? optionRowHeightForTouch : minRowHeight;

    if (index < 0 || m_popupClient->itemStyle(index).isDisplayNone())
        return minimumHeight;

    // Separator row height is the same size as itself.
    if (m_popupClient->itemIsSeparator(index))
        return max(separatorHeight, minimumHeight);

    int fontHeight = getRowFont(index).fontMetrics().height();
    return max(fontHeight, minimumHeight);
}

IntRect PopupListBox::getRowBounds(int index)
{
    if (index < 0)
        return IntRect(0, 0, visibleWidth(), getRowHeight(index));

    return IntRect(0, m_items[index]->yOffset, visibleWidth(), getRowHeight(index));
}

void PopupListBox::invalidateRow(int index)
{
    if (index < 0)
        return;

    // Invalidate in the window contents, as FramelessScrollView::invalidateRect
    // paints in the window coordinates.
    IntRect clipRect = contentsToWindow(getRowBounds(index));
    if (shouldPlaceVerticalScrollbarOnLeft() && verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar())
        clipRect.move(verticalScrollbar()->width(), 0);
    invalidateRect(clipRect);
}

void PopupListBox::scrollToRevealRow(int index)
{
    if (index < 0)
        return;

    IntRect rowRect = getRowBounds(index);

    if (rowRect.y() < scrollY()) {
        // Row is above current scroll position, scroll up.
        ScrollView::setScrollPosition(IntPoint(0, rowRect.y()));
    } else if (rowRect.maxY() > scrollY() + visibleHeight()) {
        // Row is below current scroll position, scroll down.
        ScrollView::setScrollPosition(IntPoint(0, rowRect.maxY() - visibleHeight()));
    }
}

bool PopupListBox::isSelectableItem(int index)
{
    ASSERT(index >= 0 && index < numItems());
    return m_items[index]->type == PopupItem::TypeOption && m_popupClient->itemIsEnabled(index);
}

void PopupListBox::clearSelection()
{
    if (m_selectedIndex != -1) {
        invalidateRow(m_selectedIndex);
        m_selectedIndex = -1;
        m_popupClient->selectionCleared();
    }
}

void PopupListBox::selectNextRow()
{
    adjustSelectedIndex(1);
}

void PopupListBox::selectPreviousRow()
{
    adjustSelectedIndex(-1);
}

void PopupListBox::adjustSelectedIndex(int delta)
{
    int targetIndex = m_selectedIndex + delta;
    targetIndex = std::min(std::max(targetIndex, 0), numItems() - 1);
    if (!isSelectableItem(targetIndex)) {
        // We didn't land on an option. Try to find one.
        // We try to select the closest index to target, prioritizing any in
        // the range [current, target].

        int dir = delta > 0 ? 1 : -1;
        int testIndex = m_selectedIndex;
        int bestIndex = m_selectedIndex;
        bool passedTarget = false;
        while (testIndex >= 0 && testIndex < numItems()) {
            if (isSelectableItem(testIndex))
                bestIndex = testIndex;
            if (testIndex == targetIndex)
                passedTarget = true;
            if (passedTarget && bestIndex != m_selectedIndex)
                break;

            testIndex += dir;
        }

        // Pick the best index, which may mean we don't change.
        targetIndex = bestIndex;
    }

    // Select the new index, and ensure its visible. We do this regardless of
    // whether the selection changed to ensure keyboard events always bring the
    // selection into view.
    selectIndex(targetIndex);
    scrollToRevealSelection();
}

void PopupListBox::hidePopup()
{
    if (parent()) {
        PopupContainer* container = static_cast<PopupContainer*>(parent());
        if (container->client())
            container->client()->popupClosed(container);
        container->notifyPopupHidden();
    }

    if (m_popupClient)
        m_popupClient->popupDidHide();
}

void PopupListBox::updateFromElement()
{
    clear();

    int size = m_popupClient->listSize();
    for (int i = 0; i < size; ++i) {
        PopupItem::Type type;
        if (m_popupClient->itemIsSeparator(i))
            type = PopupItem::TypeSeparator;
        else if (m_popupClient->itemIsLabel(i))
            type = PopupItem::TypeGroup;
        else
            type = PopupItem::TypeOption;
        m_items.append(new PopupItem(m_popupClient->itemText(i), type));
        m_items[i]->enabled = isSelectableItem(i);
        PopupMenuStyle style = m_popupClient->itemStyle(i);
        m_items[i]->textDirection = style.textDirection();
        m_items[i]->hasTextDirectionOverride = style.hasTextDirectionOverride();
    }

    m_selectedIndex = m_popupClient->selectedIndex();
    setOriginalIndex(m_selectedIndex);

    layout();
}

void PopupListBox::setMaxWidthAndLayout(int maxWidth)
{
    m_maxWindowWidth = maxWidth;
    layout();
}

void PopupListBox::layout()
{
    bool isRightAligned = m_popupClient->menuStyle().textDirection() == RTL;

    // Size our child items.
    int baseWidth = 0;
    int paddingWidth = 0;
    int lineEndPaddingWidth = 0;
    int y = 0;
    for (int i = 0; i < numItems(); ++i) {
        // Place the item vertically.
        m_items[i]->yOffset = y;
        if (m_popupClient->itemStyle(i).isDisplayNone())
            continue;
        y += getRowHeight(i);

        // Ensure the popup is wide enough to fit this item.
        Font itemFont = getRowFont(i);
        String text = m_popupClient->itemText(i);
        int width = 0;
        if (!text.isEmpty())
            width = itemFont.width(TextRun(text));

        baseWidth = max(baseWidth, width);
        // FIXME: http://b/1210481 We should get the padding of individual
        // option elements.
        paddingWidth = max<int>(paddingWidth,
            m_popupClient->clientPaddingLeft() + m_popupClient->clientPaddingRight());
        lineEndPaddingWidth = max<int>(lineEndPaddingWidth,
            isRightAligned ? m_popupClient->clientPaddingLeft() : m_popupClient->clientPaddingRight());
    }

    // Calculate scroll bar width.
    int windowHeight = 0;
    m_visibleRows = std::min(numItems(), maxVisibleRows);

    for (int i = 0; i < m_visibleRows; ++i) {
        int rowHeight = getRowHeight(i);

        // Only clip the window height for non-Mac platforms.
        if (windowHeight + rowHeight > m_maxHeight) {
            m_visibleRows = i;
            break;
        }

        windowHeight += rowHeight;
    }

    // Set our widget and scrollable contents sizes.
    int scrollbarWidth = 0;
    if (m_visibleRows < numItems()) {
        if (!ScrollbarTheme::theme()->usesOverlayScrollbars())
            scrollbarWidth = ScrollbarTheme::theme()->scrollbarThickness();

        // Use minEndOfLinePadding when there is a scrollbar so that we use
        // as much as (lineEndPaddingWidth - minEndOfLinePadding) padding
        // space for scrollbar and allow user to use CSS padding to make the
        // popup listbox align with the select element.
        paddingWidth = paddingWidth - lineEndPaddingWidth + minEndOfLinePadding;
    }

    int windowWidth = baseWidth + scrollbarWidth + paddingWidth;
    if (windowWidth > m_maxWindowWidth) {
        // windowWidth exceeds m_maxWindowWidth, so we have to clip.
        windowWidth = m_maxWindowWidth;
        baseWidth = windowWidth - scrollbarWidth - paddingWidth;
        m_baseWidth = baseWidth;
    }
    int contentWidth = windowWidth - scrollbarWidth;

    if (windowWidth < m_baseWidth) {
        windowWidth = m_baseWidth;
        contentWidth = m_baseWidth - scrollbarWidth;
    } else {
        m_baseWidth = baseWidth;
    }

    resize(windowWidth, windowHeight);
    setContentsSize(IntSize(contentWidth, getRowBounds(numItems() - 1).maxY()));

    if (hostWindow())
        scrollToRevealSelection();

    invalidate();
}

void PopupListBox::clear()
{
    for (Vector<PopupItem*>::iterator it = m_items.begin(); it != m_items.end(); ++it)
        delete *it;
    m_items.clear();
}

bool PopupListBox::isPointInBounds(const IntPoint& point)
{
    return numItems() && IntRect(0, 0, width(), height()).contains(point);
}

int PopupListBox::popupContentHeight() const
{
    return height();
}

} // namespace blink
