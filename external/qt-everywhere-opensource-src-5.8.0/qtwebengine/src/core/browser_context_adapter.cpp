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

#include "browser_context_adapter.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"

#include "browser_context_qt.h"
#include "content_client_qt.h"
#include "download_manager_delegate_qt.h"
#include "permission_manager_qt.h"
#include "type_conversion.h"
#include "web_engine_context.h"
#include "web_engine_visited_links_manager.h"
#include "url_request_context_getter_qt.h"
#include "renderer_host/user_resource_controller_host.h"

#include "net/proxy/proxy_service.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QStandardPaths>

namespace {
inline QString buildLocationFromStandardPath(const QString &standardPath, const QString &name) {
    QString location = standardPath;
    if (location.isEmpty())
        location = QDir::homePath() % QLatin1String("/.") % QCoreApplication::applicationName();

    location.append(QLatin1String("/QtWebEngine/") % name);
    return location;
}
}

namespace QtWebEngineCore {

BrowserContextAdapter::BrowserContextAdapter(bool offTheRecord)
    : m_offTheRecord(offTheRecord)
    , m_browserContext(new BrowserContextQt(this))
    , m_httpCacheType(DiskHttpCache)
    , m_persistentCookiesPolicy(AllowPersistentCookies)
    , m_visitedLinksPolicy(TrackVisitedLinksOnDisk)
    , m_httpCacheMaxSize(0)
{
    WebEngineContext::current(); // Ensure the WebEngineContext has been initialized
    content::BrowserContext::Initialize(m_browserContext.data(), toFilePath(dataPath()));
}

BrowserContextAdapter::BrowserContextAdapter(const QString &storageName)
    : m_name(storageName)
    , m_offTheRecord(false)
    , m_browserContext(new BrowserContextQt(this))
    , m_httpCacheType(DiskHttpCache)
    , m_persistentCookiesPolicy(AllowPersistentCookies)
    , m_visitedLinksPolicy(TrackVisitedLinksOnDisk)
    , m_httpCacheMaxSize(0)
{
    WebEngineContext::current(); // Ensure the WebEngineContext has been initialized
    content::BrowserContext::Initialize(m_browserContext.data(), toFilePath(dataPath()));
}

BrowserContextAdapter::~BrowserContextAdapter()
{
    if (m_downloadManagerDelegate)
        content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE, m_downloadManagerDelegate.take());
    BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(m_browserContext.data());
}

void BrowserContextAdapter::setStorageName(const QString &storageName)
{
    if (storageName == m_name)
        return;
    m_name = storageName;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateStorageSettings();
    if (m_visitedLinksManager)
        resetVisitedLinksManager();
}

void BrowserContextAdapter::setOffTheRecord(bool offTheRecord)
{
    if (offTheRecord == m_offTheRecord)
        return;
    m_offTheRecord = offTheRecord;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateStorageSettings();
    if (m_visitedLinksManager)
        resetVisitedLinksManager();
}

BrowserContextQt *BrowserContextAdapter::browserContext()
{
    return m_browserContext.data();
}

WebEngineVisitedLinksManager *BrowserContextAdapter::visitedLinksManager()
{
    if (!m_visitedLinksManager)
        resetVisitedLinksManager();
    return m_visitedLinksManager.data();
}

DownloadManagerDelegateQt *BrowserContextAdapter::downloadManagerDelegate()
{
    if (!m_downloadManagerDelegate)
        m_downloadManagerDelegate.reset(new DownloadManagerDelegateQt(this));
    return m_downloadManagerDelegate.data();
}

QWebEngineCookieStore *BrowserContextAdapter::cookieStore()
{
    if (!m_cookieStore)
        m_cookieStore.reset(new QWebEngineCookieStore);
    return m_cookieStore.data();
}

QWebEngineUrlRequestInterceptor *BrowserContextAdapter::requestInterceptor()
{
    return m_requestInterceptor.data();
}

void BrowserContextAdapter::setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor)
{
    m_requestInterceptor = interceptor;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateRequestInterceptor();
}

void BrowserContextAdapter::addClient(BrowserContextAdapterClient *adapterClient)
{
    m_clients.append(adapterClient);
}

void BrowserContextAdapter::removeClient(BrowserContextAdapterClient *adapterClient)
{
    m_clients.removeOne(adapterClient);
}

void BrowserContextAdapter::cancelDownload(quint32 downloadId)
{
    downloadManagerDelegate()->cancelDownload(downloadId);
}

QSharedPointer<BrowserContextAdapter> BrowserContextAdapter::defaultContext()
{
    return WebEngineContext::current()->defaultBrowserContext();
}

QObject* BrowserContextAdapter::globalQObjectRoot()
{
    return WebEngineContext::current()->globalQObject();
}

QString BrowserContextAdapter::dataPath() const
{
    if (m_offTheRecord)
        return QString();
    if (!m_dataPath.isEmpty())
        return m_dataPath;
    if (!m_name.isNull())
        return buildLocationFromStandardPath(QStandardPaths::writableLocation(QStandardPaths::DataLocation), m_name);
    return QString();
}

void BrowserContextAdapter::setDataPath(const QString &path)
{
    if (m_dataPath == path)
        return;
    m_dataPath = path;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateStorageSettings();
    if (m_visitedLinksManager)
        resetVisitedLinksManager();
}

QString BrowserContextAdapter::cachePath() const
{
    if (m_offTheRecord)
        return QString();
    if (!m_cachePath.isEmpty())
        return m_cachePath;
    if (!m_name.isNull())
        return buildLocationFromStandardPath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation), m_name);
    return QString();
}

void BrowserContextAdapter::setCachePath(const QString &path)
{
    if (m_cachePath == path)
        return;
    m_cachePath = path;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateHttpCache();
}

QString BrowserContextAdapter::cookiesPath() const
{
    if (m_offTheRecord)
        return QString();
    QString basePath = dataPath();
    if (!basePath.isEmpty()) {
        // This is a typo fix. We still need the old path in order to avoid breaking migration.
        QDir coookiesFolder(basePath % QLatin1String("/Coookies"));
        if (coookiesFolder.exists())
            return coookiesFolder.path();
        return basePath % QLatin1String("/Cookies");
    }
    return QString();
}

QString BrowserContextAdapter::channelIdPath() const
{
    if (m_offTheRecord)
        return QString();
    QString basePath = dataPath();
    if (!basePath.isEmpty())
        return basePath % QLatin1String("/Origin Bound Certs");
    return QString();
}

QString BrowserContextAdapter::httpCachePath() const
{
    if (m_offTheRecord)
        return QString();
    QString basePath = cachePath();
    if (!basePath.isEmpty())
        return basePath % QLatin1String("/Cache");
    return QString();
}

QString BrowserContextAdapter::httpUserAgent() const
{
    if (m_httpUserAgent.isNull())
        return QString::fromStdString(ContentClientQt::getUserAgent());
    return m_httpUserAgent;
}

void BrowserContextAdapter::setHttpUserAgent(const QString &userAgent)
{
    if (m_httpUserAgent == userAgent)
        return;
    m_httpUserAgent = userAgent.simplified();

    std::vector<content::WebContentsImpl *> list = content::WebContentsImpl::GetAllWebContents();
    Q_FOREACH (content::WebContentsImpl *web_contents, list)
        if (web_contents->GetBrowserContext() == m_browserContext.data())
            web_contents->SetUserAgentOverride(m_httpUserAgent.toStdString());

    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateUserAgent();
}

BrowserContextAdapter::HttpCacheType BrowserContextAdapter::httpCacheType() const
{
    if (isOffTheRecord() || httpCachePath().isEmpty())
        return MemoryHttpCache;
    return m_httpCacheType;
}

void BrowserContextAdapter::setHttpCacheType(BrowserContextAdapter::HttpCacheType newhttpCacheType)
{
    BrowserContextAdapter::HttpCacheType oldCacheType = httpCacheType();
    m_httpCacheType = newhttpCacheType;
    if (oldCacheType == httpCacheType())
        return;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateHttpCache();
}

BrowserContextAdapter::PersistentCookiesPolicy BrowserContextAdapter::persistentCookiesPolicy() const
{
    if (isOffTheRecord() || cookiesPath().isEmpty())
        return NoPersistentCookies;
    return m_persistentCookiesPolicy;
}

void BrowserContextAdapter::setPersistentCookiesPolicy(BrowserContextAdapter::PersistentCookiesPolicy newPersistentCookiesPolicy)
{
    BrowserContextAdapter::PersistentCookiesPolicy oldPolicy = persistentCookiesPolicy();
    m_persistentCookiesPolicy = newPersistentCookiesPolicy;
    if (oldPolicy == persistentCookiesPolicy())
        return;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateCookieStore();
}

BrowserContextAdapter::VisitedLinksPolicy BrowserContextAdapter::visitedLinksPolicy() const
{
    if (isOffTheRecord() || m_visitedLinksPolicy == DoNotTrackVisitedLinks)
        return DoNotTrackVisitedLinks;
    if (dataPath().isEmpty())
        return TrackVisitedLinksInMemory;
    return m_visitedLinksPolicy;
}

bool BrowserContextAdapter::trackVisitedLinks() const
{
    switch (visitedLinksPolicy()) {
    case DoNotTrackVisitedLinks:
        return false;
    default:
        break;
    }
    return true;
}

bool BrowserContextAdapter::persistVisitedLinks() const
{
    switch (visitedLinksPolicy()) {
    case DoNotTrackVisitedLinks:
    case TrackVisitedLinksInMemory:
        return false;
    default:
        break;
    }
    return true;
}

void BrowserContextAdapter::setVisitedLinksPolicy(BrowserContextAdapter::VisitedLinksPolicy visitedLinksPolicy)
{
    if (m_visitedLinksPolicy == visitedLinksPolicy)
        return;
    m_visitedLinksPolicy = visitedLinksPolicy;
    if (m_visitedLinksManager)
        resetVisitedLinksManager();
}

int BrowserContextAdapter::httpCacheMaxSize() const
{
    return m_httpCacheMaxSize;
}

void BrowserContextAdapter::setHttpCacheMaxSize(int maxSize)
{
    if (m_httpCacheMaxSize == maxSize)
        return;
    m_httpCacheMaxSize = maxSize;
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateHttpCache();
}

const QHash<QByteArray, QWebEngineUrlSchemeHandler *> &BrowserContextAdapter::customUrlSchemeHandlers() const
{
    return m_customUrlSchemeHandlers;
}

const QList<QByteArray> BrowserContextAdapter::customUrlSchemes() const
{
    return m_customUrlSchemeHandlers.keys();
}

void BrowserContextAdapter::updateCustomUrlSchemeHandlers()
{
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateJobFactory();
}

bool BrowserContextAdapter::removeCustomUrlSchemeHandler(QWebEngineUrlSchemeHandler *handler)
{
    bool removedOneOrMore = false;
    auto it = m_customUrlSchemeHandlers.begin();
    while (it != m_customUrlSchemeHandlers.end()) {
        if (it.value() == handler) {
            it = m_customUrlSchemeHandlers.erase(it);
            removedOneOrMore = true;
            continue;
        }
        ++it;
    }
    if (removedOneOrMore)
        updateCustomUrlSchemeHandlers();
    return removedOneOrMore;
}

QWebEngineUrlSchemeHandler *BrowserContextAdapter::takeCustomUrlSchemeHandler(const QByteArray &scheme)
{
    QWebEngineUrlSchemeHandler *handler = m_customUrlSchemeHandlers.take(scheme);
    if (handler)
        updateCustomUrlSchemeHandlers();
    return handler;
}

void BrowserContextAdapter::addCustomUrlSchemeHandler(const QByteArray &scheme, QWebEngineUrlSchemeHandler *handler)
{
    m_customUrlSchemeHandlers.insert(scheme, handler);
    updateCustomUrlSchemeHandlers();
}

void BrowserContextAdapter::clearCustomUrlSchemeHandlers()
{
    m_customUrlSchemeHandlers.clear();
    updateCustomUrlSchemeHandlers();
}

UserResourceControllerHost *BrowserContextAdapter::userResourceController()
{
    if (!m_userResourceController)
        m_userResourceController.reset(new UserResourceControllerHost);
    return m_userResourceController.data();
}

void BrowserContextAdapter::permissionRequestReply(const QUrl &origin, PermissionType type, bool reply)
{
    static_cast<PermissionManagerQt*>(browserContext()->GetPermissionManager())->permissionRequestReply(origin, type, reply);
}

bool BrowserContextAdapter::checkPermission(const QUrl &origin, PermissionType type)
{
    return static_cast<PermissionManagerQt*>(browserContext()->GetPermissionManager())->checkPermission(origin, type);
}

QString BrowserContextAdapter::httpAcceptLanguageWithoutQualities() const
{
    const QStringList list = m_httpAcceptLanguage.split(QLatin1Char(','));
    QString out;
    Q_FOREACH (const QString& str, list) {
        if (!out.isEmpty())
            out.append(QLatin1Char(','));
        out.append(str.split(QLatin1Char(';')).first());
    }
    return out;
}

QString BrowserContextAdapter::httpAcceptLanguage() const
{
    return m_httpAcceptLanguage;
}

void BrowserContextAdapter::setHttpAcceptLanguage(const QString &httpAcceptLanguage)
{
    if (m_httpAcceptLanguage == httpAcceptLanguage)
        return;
    m_httpAcceptLanguage = httpAcceptLanguage;

    std::vector<content::WebContentsImpl *> list = content::WebContentsImpl::GetAllWebContents();
    Q_FOREACH (content::WebContentsImpl *web_contents, list) {
        if (web_contents->GetBrowserContext() == m_browserContext.data()) {
            content::RendererPreferences* rendererPrefs = web_contents->GetMutableRendererPrefs();
            rendererPrefs->accept_languages = httpAcceptLanguageWithoutQualities().toStdString();
            web_contents->GetRenderViewHost()->SyncRendererPrefs();
        }
    }

    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->updateUserAgent();
}

void BrowserContextAdapter::clearHttpCache()
{
    if (m_browserContext->url_request_getter_.get())
        m_browserContext->url_request_getter_->clearHttpCache();
}

void BrowserContextAdapter::setSpellCheckLanguages(const QStringList &languages)
{
#if defined(ENABLE_SPELLCHECK)
    m_browserContext->setSpellCheckLanguages(languages);
#endif
}

QStringList BrowserContextAdapter::spellCheckLanguages() const
{
#if defined(ENABLE_SPELLCHECK)
    return m_browserContext->spellCheckLanguages();
#else
    return QStringList();
#endif
}

void BrowserContextAdapter::setSpellCheckEnabled(bool enabled)
{
#if defined(ENABLE_SPELLCHECK)
    m_browserContext->setSpellCheckEnabled(enabled);
#endif
}

bool BrowserContextAdapter::isSpellCheckEnabled() const
{
#if defined(ENABLE_SPELLCHECK)
    return m_browserContext->isSpellCheckEnabled();
#else
    return false;
#endif
}

void BrowserContextAdapter::resetVisitedLinksManager()
{
    m_visitedLinksManager.reset(new WebEngineVisitedLinksManager(this));
}

} // namespace QtWebEngineCore
