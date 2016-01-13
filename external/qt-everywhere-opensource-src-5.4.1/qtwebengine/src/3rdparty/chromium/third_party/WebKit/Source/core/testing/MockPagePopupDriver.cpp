/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/testing/MockPagePopupDriver.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/PagePopup.h"
#include "core/page/PagePopupController.h"
#include "platform/Timer.h"

namespace WebCore {

class MockPagePopup : public PagePopup, public RefCounted<MockPagePopup> {
public:
    static PassRefPtr<MockPagePopup> create(PagePopupClient*, const IntRect& originBoundsInRootView, LocalFrame*);
    virtual ~MockPagePopup();
    bool initialize();
    void closeLater();

private:
    MockPagePopup(PagePopupClient*, const IntRect& originBoundsInRootView, LocalFrame*);
    void close(Timer<MockPagePopup>*);

    PagePopupClient* m_popupClient;
    RefPtrWillBePersistent<HTMLIFrameElement> m_iframe;
    Timer<MockPagePopup> m_closeTimer;
};

inline MockPagePopup::MockPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView, LocalFrame* mainFrame)
    : m_popupClient(client)
    , m_closeTimer(this, &MockPagePopup::close)
{
    Document* document = mainFrame->document();
    ASSERT(document);
    m_iframe = HTMLIFrameElement::create(*document);
    m_iframe->setIdAttribute("mock-page-popup");
    m_iframe->setInlineStyleProperty(CSSPropertyBorderWidth, 0.0, CSSPrimitiveValue::CSS_PX);
    m_iframe->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    m_iframe->setInlineStyleProperty(CSSPropertyLeft, originBoundsInRootView.x(), CSSPrimitiveValue::CSS_PX, true);
    m_iframe->setInlineStyleProperty(CSSPropertyTop, originBoundsInRootView.maxY(), CSSPrimitiveValue::CSS_PX, true);
    if (document->body())
        document->body()->appendChild(m_iframe.get());
}

bool MockPagePopup::initialize()
{
    const char scriptToSetUpPagePopupController[] = "<script>window.pagePopupController = parent.internals.pagePopupController;</script>";
    RefPtr<SharedBuffer> data = SharedBuffer::create(scriptToSetUpPagePopupController, sizeof(scriptToSetUpPagePopupController));
    m_popupClient->writeDocument(data.get());
    LocalFrame* localFrame = toLocalFrame(m_iframe->contentFrame());
    if (!localFrame)
        return false;
    localFrame->loader().load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    return true;
}

PassRefPtr<MockPagePopup> MockPagePopup::create(PagePopupClient* client, const IntRect& originBoundsInRootView, LocalFrame* mainFrame)
{
    return adoptRef(new MockPagePopup(client, originBoundsInRootView, mainFrame));
}

void MockPagePopup::closeLater()
{
    ref();
    m_popupClient->didClosePopup();
    m_popupClient = 0;
    // This can be called in detach(), and we should not change DOM structure
    // during detach().
    m_closeTimer.startOneShot(0, FROM_HERE);
}

void MockPagePopup::close(Timer<MockPagePopup>*)
{
    deref();
}

MockPagePopup::~MockPagePopup()
{
    if (m_iframe && m_iframe->parentNode())
        m_iframe->parentNode()->removeChild(m_iframe.get());
}

inline MockPagePopupDriver::MockPagePopupDriver(LocalFrame* mainFrame)
    : m_mainFrame(mainFrame)
{
}

PassOwnPtr<MockPagePopupDriver> MockPagePopupDriver::create(LocalFrame* mainFrame)
{
    return adoptPtr(new MockPagePopupDriver(mainFrame));
}

MockPagePopupDriver::~MockPagePopupDriver()
{
    closePagePopup(m_mockPagePopup.get());
}

PagePopup* MockPagePopupDriver::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    if (m_mockPagePopup)
        closePagePopup(m_mockPagePopup.get());
    if (!client || !m_mainFrame)
        return 0;
    m_pagePopupController = PagePopupController::create(client);
    m_mockPagePopup = MockPagePopup::create(client, originBoundsInRootView, m_mainFrame);
    if (!m_mockPagePopup->initialize()) {
        m_mockPagePopup->closeLater();
        m_mockPagePopup.clear();
    }
    return m_mockPagePopup.get();
}

void MockPagePopupDriver::closePagePopup(PagePopup* popup)
{
    if (!popup || popup != m_mockPagePopup.get())
        return;
    m_mockPagePopup->closeLater();
    m_mockPagePopup.clear();
    m_pagePopupController->clearPagePopupClient();
    m_pagePopupController.clear();
}

}
