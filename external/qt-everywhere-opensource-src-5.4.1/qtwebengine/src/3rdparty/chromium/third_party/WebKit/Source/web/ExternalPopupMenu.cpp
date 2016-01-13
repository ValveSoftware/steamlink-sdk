/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "web/ExternalPopupMenu.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "platform/PopupMenuClient.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/IntPoint.h"
#include "platform/text/TextDirection.h"
#include "public/platform/WebVector.h"
#include "public/web/WebExternalPopupMenu.h"
#include "public/web/WebMenuItemInfo.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebViewClient.h"
#include "web/WebViewImpl.h"

using namespace WebCore;

namespace blink {

ExternalPopupMenu::ExternalPopupMenu(LocalFrame& frame, PopupMenuClient* popupMenuClient, WebViewImpl& webView)
    : m_popupMenuClient(popupMenuClient)
    , m_frameView(frame.view())
    , m_webView(webView)
    , m_dispatchEventTimer(this, &ExternalPopupMenu::dispatchEvent)
    , m_webExternalPopupMenu(0)
{
}

ExternalPopupMenu::~ExternalPopupMenu()
{
}

void ExternalPopupMenu::show(const FloatQuad& controlPosition, const IntSize&, int index)
{
    IntRect rect(controlPosition.enclosingBoundingBox());
    // WebCore reuses the PopupMenu of an element.
    // For simplicity, we do recreate the actual external popup everytime.
    if (m_webExternalPopupMenu) {
        m_webExternalPopupMenu->close();
        m_webExternalPopupMenu = 0;
    }

    WebPopupMenuInfo info;
    getPopupMenuInfo(&info);
    if (info.items.isEmpty())
        return;
    m_webExternalPopupMenu = m_webView.client()->createExternalPopupMenu(info, this);
    if (m_webExternalPopupMenu) {
        m_webExternalPopupMenu->show(m_frameView->contentsToWindow(rect));
#if OS(MACOSX)
        const WebInputEvent* currentEvent = WebViewImpl::currentInputEvent();
        if (currentEvent && currentEvent->type == WebInputEvent::MouseDown) {
            m_syntheticEvent = adoptPtr(new WebMouseEvent);
            *m_syntheticEvent = *static_cast<const WebMouseEvent*>(currentEvent);
            m_syntheticEvent->type = WebInputEvent::MouseUp;
            m_dispatchEventTimer.startOneShot(0, FROM_HERE);
            // FIXME: show() is asynchronous. If preparing a popup is slow and
            // a user released the mouse button before showing the popup,
            // mouseup and click events are correctly dispatched. Dispatching
            // the synthetic mouseup event is redundant in this case.
        }
#endif
    } else {
        // The client might refuse to create a popup (when there is already one pending to be shown for example).
        didCancel();
    }
}

void ExternalPopupMenu::dispatchEvent(Timer<ExternalPopupMenu>*)
{
    m_webView.handleInputEvent(*m_syntheticEvent);
    m_syntheticEvent.clear();
}

void ExternalPopupMenu::hide()
{
    if (m_popupMenuClient)
        m_popupMenuClient->popupDidHide();
    if (!m_webExternalPopupMenu)
        return;
    m_webExternalPopupMenu->close();
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::updateFromElement()
{
}

void ExternalPopupMenu::disconnectClient()
{
    hide();
    m_popupMenuClient = 0;
}

void ExternalPopupMenu::didChangeSelection(int index)
{
    if (m_popupMenuClient)
        m_popupMenuClient->selectionChanged(index);
}

void ExternalPopupMenu::didAcceptIndex(int index)
{
    // Calling methods on the PopupMenuClient might lead to this object being
    // derefed. This ensures it does not get deleted while we are running this
    // method.
    RefPtr<ExternalPopupMenu> guard(this);

    if (m_popupMenuClient) {
        m_popupMenuClient->popupDidHide();
        m_popupMenuClient->valueChanged(index);
    }
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::didAcceptIndices(const WebVector<int>& indices)
{
    if (!m_popupMenuClient) {
        m_webExternalPopupMenu = 0;
        return;
    }

    // Calling methods on the PopupMenuClient might lead to this object being
    // derefed. This ensures it does not get deleted while we are running this
    // method.
    RefPtr<ExternalPopupMenu> protect(this);

    if (!indices.size())
        m_popupMenuClient->valueChanged(-1, true);
    else {
        for (size_t i = 0; i < indices.size(); ++i)
            m_popupMenuClient->listBoxSelectItem(indices[i], (i > 0), false, (i == indices.size() - 1));
    }

    // The call to valueChanged above might have lead to a call to
    // disconnectClient, so we might not have a PopupMenuClient anymore.
    if (m_popupMenuClient)
        m_popupMenuClient->popupDidHide();

    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::didCancel()
{
    // See comment in didAcceptIndex on why we need this.
    RefPtr<ExternalPopupMenu> guard(this);

    if (m_popupMenuClient)
        m_popupMenuClient->popupDidHide();
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::getPopupMenuInfo(WebPopupMenuInfo* info)
{
    int itemCount = m_popupMenuClient->listSize();
    WebVector<WebMenuItemInfo> items(static_cast<size_t>(itemCount));
    for (int i = 0; i < itemCount; ++i) {
        WebMenuItemInfo& popupItem = items[i];
        popupItem.label = m_popupMenuClient->itemText(i);
        popupItem.toolTip = m_popupMenuClient->itemToolTip(i);
        if (m_popupMenuClient->itemIsSeparator(i))
            popupItem.type = WebMenuItemInfo::Separator;
        else if (m_popupMenuClient->itemIsLabel(i))
            popupItem.type = WebMenuItemInfo::Group;
        else
            popupItem.type = WebMenuItemInfo::Option;
        popupItem.enabled = m_popupMenuClient->itemIsEnabled(i);
        popupItem.checked = m_popupMenuClient->itemIsSelected(i);
        PopupMenuStyle style = m_popupMenuClient->itemStyle(i);
        if (style.textDirection() == WebCore::RTL)
            popupItem.textDirection = WebTextDirectionRightToLeft;
        else
            popupItem.textDirection = WebTextDirectionLeftToRight;
        popupItem.hasTextDirectionOverride = style.hasTextDirectionOverride();
    }

    info->itemHeight = m_popupMenuClient->menuStyle().font().fontMetrics().height();
    info->itemFontSize = static_cast<int>(m_popupMenuClient->menuStyle().font().fontDescription().computedSize());
    info->selectedIndex = m_popupMenuClient->selectedIndex();
    info->rightAligned = m_popupMenuClient->menuStyle().textDirection() == WebCore::RTL;
    info->allowMultipleSelection = m_popupMenuClient->multiple();
    info->items.swap(items);
}

}
