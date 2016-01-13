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

#ifndef PopupContainer_h
#define PopupContainer_h

#include "platform/PopupMenuStyle.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/scroll/FramelessScrollView.h"
#include "web/PopupListBox.h"

namespace WebCore {
class ChromeClient;
class FrameView;
class PopupMenuClient;
}

namespace blink {

struct WebPopupMenuInfo;

class PopupContainer FINAL : public WebCore::FramelessScrollView {
public:
    static PassRefPtr<PopupContainer> create(WebCore::PopupMenuClient*, bool deviceSupportsTouch);

    // Whether a key event should be sent to this popup.
    bool isInterestedInEventForKey(int keyCode);

    // FramelessScrollView
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&) OVERRIDE;
    virtual void hide() OVERRIDE;
    virtual bool handleMouseDownEvent(const WebCore::PlatformMouseEvent&) OVERRIDE;
    virtual bool handleMouseMoveEvent(const WebCore::PlatformMouseEvent&) OVERRIDE;
    virtual bool handleMouseReleaseEvent(const WebCore::PlatformMouseEvent&) OVERRIDE;
    virtual bool handleWheelEvent(const WebCore::PlatformWheelEvent&) OVERRIDE;
    virtual bool handleKeyEvent(const WebCore::PlatformKeyboardEvent&) OVERRIDE;
    virtual bool handleTouchEvent(const WebCore::PlatformTouchEvent&) OVERRIDE;
    virtual bool handleGestureEvent(const WebCore::PlatformGestureEvent&) OVERRIDE;

    // PopupContainer methods

    // Show the popup
    void showPopup(WebCore::FrameView*);

    // Show the popup in the specified rect for the specified frame.
    // Note: this code was somehow arbitrarily factored-out of the Popup class
    // so WebViewImpl can create a PopupContainer. This method is used for
    // displaying auto complete popup menus on Mac Chromium, and for all
    // popups on other platforms.
    void showInRect(const WebCore::FloatQuad& controlPosition, const WebCore::IntSize& controlSize, WebCore::FrameView*, int index);

    // Hides the popup.
    void hidePopup();

    // The popup was hidden.
    void notifyPopupHidden();

    PopupListBox* listBox() const { return m_listBox.get(); }

    bool isRTL() const;

    // Gets the index of the item that the user is currently moused-over or
    // has selected with the keyboard up/down arrows.
    int selectedIndex() const;

    // Refresh the popup values from the PopupMenuClient.
    WebCore::IntRect refresh(const WebCore::IntRect& targetControlRect);

    // The menu per-item data.
    const Vector<PopupItem*>& popupData() const;

    // The height of a row in the menu.
    int menuItemHeight() const;

    // The size of the font being used.
    int menuItemFontSize() const;

    // The style of the menu being used.
    WebCore::PopupMenuStyle menuStyle() const;

    // While hovering popup menu window, we want to show tool tip message.
    String getSelectedItemToolTip();

    // This is public for testing.
    static WebCore::IntRect layoutAndCalculateWidgetRectInternal(WebCore::IntRect widgetRectInScreen, int targetControlHeight, const WebCore::FloatRect& windowRect, const WebCore::FloatRect& screen, bool isRTL, const int rtlOffset, const int verticalOffset, const WebCore::IntSize& transformOffset, PopupContent*, bool& needToResizeView);

private:
    friend class WTF::RefCounted<PopupContainer>;

    PopupContainer(WebCore::PopupMenuClient*, bool deviceSupportsTouch);
    virtual ~PopupContainer();

    // Paint the border.
    void paintBorder(WebCore::GraphicsContext*, const WebCore::IntRect&);

    // Layout and calculate popup widget size and location and returns it as IntRect.
    WebCore::IntRect layoutAndCalculateWidgetRect(int targetControlHeight, const WebCore::IntSize& transformOffset, const WebCore::IntPoint& popupInitialCoordinate);

    void fitToListBox();

    void popupOpened(const WebCore::IntRect& bounds);
    void getPopupMenuInfo(WebPopupMenuInfo*);

    // Returns the ChromeClient of the page this popup is associated with.
    WebCore::ChromeClient& chromeClient();

    RefPtr<PopupListBox> m_listBox;
    RefPtr<WebCore::FrameView> m_frameView;

    // m_controlPosition contains the transformed position of the
    // <select>/<input> associated with this popup. m_controlSize is the size
    // of the <select>/<input> without transform.
    // The popup menu will be positioned as follows:
    // LTR : If the popup is positioned down it will align with the bottom left
    //       of m_controlPosition (p4)
    //       If the popup is positioned up it will align with the top left of
    //       m_controlPosition (p1)
    // RTL : If the popup is positioned down it will align with the bottom right
    //       of m_controlPosition (p3)
    //       If the popup is positioned up it will align with the top right of
    //       m_controlPosition (p2)
    WebCore::FloatQuad m_controlPosition;
    WebCore::IntSize m_controlSize;

    // Whether the popup is currently open.
    bool m_popupOpen;
};

} // namespace WebCore

#endif
