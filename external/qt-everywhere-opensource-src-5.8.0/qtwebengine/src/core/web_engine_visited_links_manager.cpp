/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "web_engine_visited_links_manager.h"

#include "browser_context_adapter.h"
#include "browser_context_qt.h"
#include "content_browser_client_qt.h"
#include "type_conversion.h"

#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_master.h"

namespace QtWebEngineCore {

namespace {
class BasicUrlIterator : public visitedlink::VisitedLinkMaster::URLIterator {
public:
    BasicUrlIterator(const QList<QUrl> &urls) : m_urls(urls) {}
    virtual const GURL& NextURL() { m_currentUrl = toGurl(m_urls.takeFirst()); return m_currentUrl; }
    virtual bool HasNextURL() const { return !m_urls.isEmpty(); }
private:
    QList<QUrl> m_urls;
    GURL m_currentUrl;

};
} // Anonymous namespace

// Due to the design of the visitedLink component, it seems safer to provide a
// basic delegate that will simply rebuild an empty visitedLink table if needed
// from the application history rather than crashing. This is functionality
// that was covered by QWebHistoryInterface in QtWebKitWidgets.

class VisitedLinkDelegateQt : public visitedlink::VisitedLinkDelegate
{
public:
    ~VisitedLinkDelegateQt() {}
    void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) { enumerator->OnComplete(true); }
};

void WebEngineVisitedLinksManager::deleteAllVisitedLinkData()
{
    m_visitedLinkMaster->DeleteAllURLs();
}

void WebEngineVisitedLinksManager::deleteVisitedLinkDataForUrls(const QList<QUrl> &urlsToDelete)
{
    BasicUrlIterator iterator(urlsToDelete);
    m_visitedLinkMaster->DeleteURLs(&iterator);
}

bool WebEngineVisitedLinksManager::containsUrl(const QUrl &url) const
{
    return m_visitedLinkMaster->IsVisited(toGurl(url));
}

WebEngineVisitedLinksManager::WebEngineVisitedLinksManager(BrowserContextAdapter *adapter)
    : m_delegate(new VisitedLinkDelegateQt)
{
    Q_ASSERT(adapter && adapter->browserContext());
    BrowserContextQt *browserContext = adapter->browserContext();
    m_visitedLinkMaster.reset(new visitedlink::VisitedLinkMaster(browserContext, m_delegate.data(), adapter->persistVisitedLinks()));
    m_visitedLinkMaster->Init();
}

WebEngineVisitedLinksManager::~WebEngineVisitedLinksManager()
{
}

void WebEngineVisitedLinksManager::addUrl(const GURL &urlToAdd)
{
    Q_ASSERT(m_visitedLinkMaster);
    m_visitedLinkMaster->AddURL(urlToAdd);
}

} // namespace QtWebEngineCore
