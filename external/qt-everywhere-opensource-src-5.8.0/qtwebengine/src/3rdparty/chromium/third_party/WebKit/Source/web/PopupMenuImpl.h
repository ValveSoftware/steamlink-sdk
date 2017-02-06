// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PopupMenuImpl_h
#define PopupMenuImpl_h

#include "core/page/PagePopupClient.h"
#include "platform/PopupMenu.h"

namespace blink {

class ChromeClientImpl;
class PagePopup;
class HTMLElement;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;
class HTMLSelectElement;

class PopupMenuImpl final : public PopupMenu, public PagePopupClient {
public:
    static PopupMenuImpl* create(ChromeClientImpl*, HTMLSelectElement&);
    ~PopupMenuImpl() override;
    DECLARE_VIRTUAL_TRACE();

    void update();

    void dispose();

private:
    PopupMenuImpl(ChromeClientImpl*, HTMLSelectElement&);

    class ItemIterationContext;
    void addOption(ItemIterationContext&, HTMLOptionElement&);
    void addOptGroup(ItemIterationContext&, HTMLOptGroupElement&);
    void addSeparator(ItemIterationContext&, HTMLHRElement&);
    void addElementStyle(ItemIterationContext&, HTMLElement&);

    // PopupMenu functions:
    void show() override;
    void hide() override;
    void disconnectClient() override;
    void updateFromElement(UpdateReason) override;

    // PagePopupClient functions:
    void writeDocument(SharedBuffer*) override;
    void selectFontsFromOwnerDocument(Document&) override;
    void setValueAndClosePopup(int, const String&) override;
    void setValue(const String&) override;
    void closePopup() override;
    Element& ownerElement() override;
    Locale& locale() override;
    void didClosePopup() override;

    Member<ChromeClientImpl> m_chromeClient;
    Member<HTMLSelectElement> m_ownerElement;
    PagePopup* m_popup;
    bool m_needsUpdate;
};

} // namespace blink

#endif // PopupMenuImpl_h
