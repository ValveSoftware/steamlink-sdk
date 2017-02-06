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

#ifndef BROWSER_CONTEXT_ADAPTER_H
#define BROWSER_CONTEXT_ADAPTER_H

#include "qtwebenginecoreglobal.h"

#include <QEnableSharedFromThis>
#include <QList>
#include <QPointer>
#include <QScopedPointer>
#include <QString>
#include <QVector>

#include "api/qwebenginecookiestore.h"
#include "api/qwebengineurlrequestinterceptor.h"
#include "api/qwebengineurlschemehandler.h"

QT_FORWARD_DECLARE_CLASS(QObject)

namespace QtWebEngineCore {

class BrowserContextAdapterClient;
class BrowserContextQt;
class DownloadManagerDelegateQt;
class UserResourceControllerHost;
class WebEngineVisitedLinksManager;

class QWEBENGINE_EXPORT BrowserContextAdapter : public QEnableSharedFromThis<BrowserContextAdapter>
{
public:
    explicit BrowserContextAdapter(bool offTheRecord = false);
    explicit BrowserContextAdapter(const QString &storagePrefix);
    virtual ~BrowserContextAdapter();

    static QSharedPointer<BrowserContextAdapter> defaultContext();
    static QObject* globalQObjectRoot();

    WebEngineVisitedLinksManager *visitedLinksManager();
    DownloadManagerDelegateQt *downloadManagerDelegate();

    QWebEngineCookieStore *cookieStore();

    QWebEngineUrlRequestInterceptor* requestInterceptor();
    void setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor);

    QList<BrowserContextAdapterClient*> clients() { return m_clients; }
    void addClient(BrowserContextAdapterClient *adapterClient);
    void removeClient(BrowserContextAdapterClient *adapterClient);

    void cancelDownload(quint32 downloadId);

    BrowserContextQt *browserContext();

    QString storageName() const { return m_name; }
    void setStorageName(const QString &storageName);

    bool isOffTheRecord() const { return m_offTheRecord; }
    void setOffTheRecord(bool offTheRecord);

    QString dataPath() const;
    void setDataPath(const QString &path);

    QString cachePath() const;
    void setCachePath(const QString &path);

    QString httpCachePath() const;
    QString cookiesPath() const;
    QString channelIdPath() const;

    QString httpUserAgent() const;
    void setHttpUserAgent(const QString &userAgent);

    void setSpellCheckLanguages(const QStringList &language);
    QStringList spellCheckLanguages() const;
    void setSpellCheckEnabled(bool enabled);
    bool isSpellCheckEnabled() const;

    // KEEP IN SYNC with API or add mapping layer
    enum HttpCacheType {
        MemoryHttpCache = 0,
        DiskHttpCache,
        NoCache
    };

    enum PersistentCookiesPolicy {
        NoPersistentCookies = 0,
        AllowPersistentCookies,
        ForcePersistentCookies
    };

    enum VisitedLinksPolicy {
        DoNotTrackVisitedLinks = 0,
        TrackVisitedLinksInMemory,
        TrackVisitedLinksOnDisk,
    };

    enum PermissionType {
        UnsupportedPermission = 0,
        GeolocationPermission = 1,
// Reserved:
//        NotificationPermission = 2,
        AudioCapturePermission = 3,
        VideoCapturePermission = 4,
    };

    HttpCacheType httpCacheType() const;
    void setHttpCacheType(BrowserContextAdapter::HttpCacheType);

    PersistentCookiesPolicy persistentCookiesPolicy() const;
    void setPersistentCookiesPolicy(BrowserContextAdapter::PersistentCookiesPolicy);

    VisitedLinksPolicy visitedLinksPolicy() const;
    void setVisitedLinksPolicy(BrowserContextAdapter::VisitedLinksPolicy);

    int httpCacheMaxSize() const;
    void setHttpCacheMaxSize(int maxSize);

    bool trackVisitedLinks() const;
    bool persistVisitedLinks() const;

    const QHash<QByteArray, QWebEngineUrlSchemeHandler *> &customUrlSchemeHandlers() const;
    const QList<QByteArray> customUrlSchemes() const;
    void clearCustomUrlSchemeHandlers();
    void addCustomUrlSchemeHandler(const QByteArray &, QWebEngineUrlSchemeHandler *);
    bool removeCustomUrlSchemeHandler(QWebEngineUrlSchemeHandler *);
    QWebEngineUrlSchemeHandler *takeCustomUrlSchemeHandler(const QByteArray &);
    UserResourceControllerHost *userResourceController();

    void permissionRequestReply(const QUrl &origin, PermissionType type, bool reply);
    bool checkPermission(const QUrl &origin, PermissionType type);

    QString httpAcceptLanguageWithoutQualities() const;
    QString httpAcceptLanguage() const;
    void setHttpAcceptLanguage(const QString &httpAcceptLanguage);

    void clearHttpCache();

private:
    void updateCustomUrlSchemeHandlers();
    void resetVisitedLinksManager();

    QString m_name;
    bool m_offTheRecord;
    QScopedPointer<BrowserContextQt> m_browserContext;
    QScopedPointer<WebEngineVisitedLinksManager> m_visitedLinksManager;
    QScopedPointer<DownloadManagerDelegateQt> m_downloadManagerDelegate;
    QScopedPointer<UserResourceControllerHost> m_userResourceController;
    QScopedPointer<QWebEngineCookieStore> m_cookieStore;
    QPointer<QWebEngineUrlRequestInterceptor> m_requestInterceptor;

    QString m_dataPath;
    QString m_cachePath;
    QString m_httpUserAgent;
    HttpCacheType m_httpCacheType;
    QString m_httpAcceptLanguage;
    PersistentCookiesPolicy m_persistentCookiesPolicy;
    VisitedLinksPolicy m_visitedLinksPolicy;
    QHash<QByteArray, QWebEngineUrlSchemeHandler *> m_customUrlSchemeHandlers;
    QList<BrowserContextAdapterClient*> m_clients;
    int m_httpCacheMaxSize;

    Q_DISABLE_COPY(BrowserContextAdapter)
};

} // namespace QtWebEngineCore

#endif // BROWSER_CONTEXT_ADAPTER_H
