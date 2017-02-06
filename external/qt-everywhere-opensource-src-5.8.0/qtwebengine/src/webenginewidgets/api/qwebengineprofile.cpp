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

#include "qwebengineprofile.h"

#include "qwebenginecookiestore.h"
#include "qwebenginedownloaditem.h"
#include "qwebenginedownloaditem_p.h"
#include "qwebenginepage.h"
#include "qwebengineprofile_p.h"
#include "qwebenginesettings.h"
#include "qwebenginescriptcollection_p.h"

#include "browser_context_adapter.h"
#include <qtwebenginecoreglobal.h>
#include "web_engine_visited_links_manager.h"
#include "web_engine_settings.h"

QT_BEGIN_NAMESPACE

ASSERT_ENUMS_MATCH(QWebEngineDownloadItem::UnknownSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::UnknownSavePageFormat)
ASSERT_ENUMS_MATCH(QWebEngineDownloadItem::SingleHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::SingleHtmlSaveFormat)
ASSERT_ENUMS_MATCH(QWebEngineDownloadItem::CompleteHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::CompleteHtmlSaveFormat)
ASSERT_ENUMS_MATCH(QWebEngineDownloadItem::MimeHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::MimeHtmlSaveFormat)

using QtWebEngineCore::BrowserContextAdapter;

/*!
    \class QWebEngineProfile
    \brief The QWebEngineProfile class provides a web engine profile shared by multiple pages.
    \since 5.5

    \inmodule QtWebEngineWidgets

    A web engine profile contains settings, scripts, persistent cookie policy, and the list of
    visited links shared by all web engine pages that belong to the profile.

    All pages that belong to the profile share a common QWebEngineSettings instance, which can
    be accessed with the settings() method. Likewise, the scripts() method provides access
    to a common QWebEngineScriptCollection instance.

    Information about visited links is stored together with persistent cookies and other persistent
    data in a storage returned by persistentStoragePath(). The cache can be cleared of links by
    clearVisitedLinks() or clearAllVisitedLinks(). PersistentCookiesPolicy describes whether
    session and persistent cookies are saved to and restored from memory or disk.

    Profiles can be used to isolate pages from each other. A typical use case is a dedicated
    \e {off-the-record profile} for a \e {private browsing} mode. Using QWebEngineProfile() without
    defining a storage name constructs a new off-the-record profile that leaves no record on the
    local machine, and has no persistent data or cache. The isOffTheRecord() method can be used
    to check whether a profile is off-the-record.

    The default profile can be accessed by defaultProfile(). It is a built-in profile that all
    web pages not specifically created with another profile belong to.

    Implementing the QWebEngineUrlRequestInterceptor interface and registering the interceptor on a
    profile by setRequestInterceptor() enables intercepting, blocking, and modifying URL
    requests (QWebEngineUrlRequestInfo) before they reach the networking stack of Chromium.

    A QWebEngineUrlSchemeHandler can be registered for a profile by installUrlSchemeHandler()
    to add support for custom URL schemes. Requests for the scheme are then issued to
    QWebEngineUrlSchemeHandler::requestStarted() as QWebEngineUrlRequestJob objects.

    Spellchecking HTML form fields can be enabled per profile by using the setSpellCheckEnabled()
    method and the current languages used for spellchecking can be set by using the
    setSpellCheckLanguages() method.

*/

/*!
    \enum QWebEngineProfile::HttpCacheType

    This enum describes the HTTP cache type:

    \value MemoryHttpCache Use an in-memory cache. This is the only setting possible if
    \c off-the-record is set or no cache path is available.
    \value DiskHttpCache Use a disk cache. This is the default.
    \value NoCache Disable both in-memory and disk caching. (Added in Qt 5.7)
*/

/*!
    \enum QWebEngineProfile::PersistentCookiesPolicy

    This enum describes policy for cookie persistency:

    \value  NoPersistentCookies
            Both session and persistent cookies are stored in memory. This is the only setting
            possible if \c off-the-record is set or no persistent data path is available.
    \value  AllowPersistentCookies
            Cookies marked persistent are saved to and restored from disk, whereas session cookies
            are only stored to disk for crash recovery. This is the default setting.
    \value  ForcePersistentCookies
            Both session and persistent cookies are saved to and restored from disk.
*/

/*!
  \fn QWebEngineProfile::downloadRequested(QWebEngineDownloadItem *download)

  \since 5.5

  This signal is emitted whenever a download has been triggered.
  The \a download argument holds the state of the download.
  The download has to be explicitly accepted with QWebEngineDownloadItem::accept() or it will be
  cancelled by default.
  The download item is parented by the profile. If it is not accepted, it
  will be deleted immediately after the signal emission.
  This signal cannot be used with a queued connection.

  \sa QWebEngineDownloadItem
*/

QWebEngineProfilePrivate::QWebEngineProfilePrivate(QSharedPointer<BrowserContextAdapter> browserContext)
        : m_settings(new QWebEngineSettings())
        , m_scriptCollection(new QWebEngineScriptCollection(new QWebEngineScriptCollectionPrivate(browserContext->userResourceController())))
        , m_browserContextRef(browserContext)
{
    m_browserContextRef->addClient(this);
    m_settings->d_ptr->initDefaults(browserContext->isOffTheRecord());
}

QWebEngineProfilePrivate::~QWebEngineProfilePrivate()
{
    delete m_settings;
    m_settings = 0;
    m_browserContextRef->removeClient(this);

    Q_FOREACH (QWebEngineDownloadItem* download, m_ongoingDownloads) {
        if (download)
            download->cancel();
    }

    m_ongoingDownloads.clear();
}

void QWebEngineProfilePrivate::cancelDownload(quint32 downloadId)
{
    browserContext()->cancelDownload(downloadId);
}

void QWebEngineProfilePrivate::downloadDestroyed(quint32 downloadId)
{
    m_ongoingDownloads.remove(downloadId);
}

void QWebEngineProfilePrivate::downloadRequested(DownloadItemInfo &info)
{
    Q_Q(QWebEngineProfile);

    Q_ASSERT(!m_ongoingDownloads.contains(info.id));
    QWebEngineDownloadItemPrivate *itemPrivate = new QWebEngineDownloadItemPrivate(this, info.url);
    itemPrivate->downloadId = info.id;
    itemPrivate->downloadState = info.accepted ? QWebEngineDownloadItem::DownloadInProgress
                                               : QWebEngineDownloadItem::DownloadRequested;
    itemPrivate->downloadPath = info.path;
    itemPrivate->mimeType = info.mimeType;
    itemPrivate->savePageFormat = static_cast<QWebEngineDownloadItem::SavePageFormat>(info.savePageFormat);
    itemPrivate->type = static_cast<QWebEngineDownloadItem::DownloadType>(info.downloadType);

    QWebEngineDownloadItem *download = new QWebEngineDownloadItem(itemPrivate, q);

    m_ongoingDownloads.insert(info.id, download);

    Q_EMIT q->downloadRequested(download);

    QWebEngineDownloadItem::DownloadState state = download->state();

    info.path = download->path();
    info.savePageFormat = static_cast<QtWebEngineCore::BrowserContextAdapterClient::SavePageFormat>(
                download->savePageFormat());
    info.accepted = state != QWebEngineDownloadItem::DownloadCancelled;

    if (state == QWebEngineDownloadItem::DownloadRequested) {
        // Delete unaccepted downloads.
        info.accepted = false;
        m_ongoingDownloads.remove(info.id);
        delete download;
    }
}

void QWebEngineProfilePrivate::downloadUpdated(const DownloadItemInfo &info)
{
    if (!m_ongoingDownloads.contains(info.id))
        return;

    QWebEngineDownloadItem* download = m_ongoingDownloads.value(info.id).data();

    if (!download) {
        downloadDestroyed(info.id);
        return;
    }

    download->d_func()->update(info);

    if (download->isFinished())
        m_ongoingDownloads.remove(info.id);
}

/*!
    Constructs a new off-the-record profile with the parent \a parent.

    An off-the-record profile leaves no record on the local machine, and has no persistent data or cache.
    Thus, the HTTP cache can only be in memory and the cookies can only be non-persistent. Trying to change
    these settings will have no effect.

    \sa isOffTheRecord()
*/
QWebEngineProfile::QWebEngineProfile(QObject *parent)
    : QObject(parent)
    , d_ptr(new QWebEngineProfilePrivate(QSharedPointer<BrowserContextAdapter>::create(true)))
{
    d_ptr->q_ptr = this;
}

/*!
    Constructs a new profile with the storage name \a storageName and parent \a parent.

    The storage name must be unique.

    A disk-based QWebEngineProfile should be destroyed on or before application exit, otherwise the cache
    and persistent data may not be fully flushed to disk.

    \sa storageName()
*/
QWebEngineProfile::QWebEngineProfile(const QString &storageName, QObject *parent)
    : QObject(parent)
    , d_ptr(new QWebEngineProfilePrivate(QSharedPointer<BrowserContextAdapter>::create(storageName)))
{
    d_ptr->q_ptr = this;
}

/*! \internal
*/
QWebEngineProfile::QWebEngineProfile(QWebEngineProfilePrivate *privatePtr, QObject *parent)
    : QObject(parent)
    , d_ptr(privatePtr)
{
    d_ptr->q_ptr = this;
}

/*! \internal
*/
QWebEngineProfile::~QWebEngineProfile()
{
}

/*!
    Returns the storage name for the profile.

    The storage name is used to give each profile that uses the disk separate subdirectories for persistent data and cache.
*/
QString QWebEngineProfile::storageName() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->storageName();
}

/*!
    Returns \c true if this is an off-the-record profile that leaves no record on the computer.

    This will force cookies and HTTP cache to be in memory, but also force all other normally
    persistent data to be stored in memory.
*/
bool QWebEngineProfile::isOffTheRecord() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->isOffTheRecord();
}

/*!
    Returns the path used to store persistent data for the browser and web content.

    Persistent data includes persistent cookies, HTML5 local storage, and visited links.

    By default, this is below QStandardPaths::writableLocation() in a storage name specific
    directory.

    \sa setPersistentStoragePath(), storageName(), QStandardPaths::writableLocation()
*/
QString QWebEngineProfile::persistentStoragePath() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->dataPath();
}

/*!
    Overrides the default path used to store persistent web engine data.

    If \a path is set to the null string, the default path is restored.

    \sa persistentStoragePath()
*/
void QWebEngineProfile::setPersistentStoragePath(const QString &path)
{
    const Q_D(QWebEngineProfile);
    d->browserContext()->setDataPath(path);
}

/*!
    Returns the path used for caches.

    By default, this is below QStandardPaths::writableLocation() in a storage name specific
    directory.

    \sa setCachePath(), storageName(), QStandardPaths::writableLocation()
*/
QString QWebEngineProfile::cachePath() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->cachePath();
}

/*!
    Overrides the default path used for disk caches, setting it to \a path.

    If set to the null string, the default path is restored.

    \sa cachePath()
*/
void QWebEngineProfile::setCachePath(const QString &path)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setCachePath(path);
}

/*!
    Returns the user-agent string sent with HTTP to identify the browser.

    \sa setHttpUserAgent()
*/
QString QWebEngineProfile::httpUserAgent() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->httpUserAgent();
}

/*!
    Overrides the default user-agent string, setting it to \a userAgent.

    \sa httpUserAgent()
*/
void QWebEngineProfile::setHttpUserAgent(const QString &userAgent)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setHttpUserAgent(userAgent);
}

/*!
    Returns the type of HTTP cache used.

    If the profile is off-the-record, MemoryHttpCache is returned.

    \sa setHttpCacheType(), cachePath()
*/
QWebEngineProfile::HttpCacheType QWebEngineProfile::httpCacheType() const
{
    const Q_D(QWebEngineProfile);
    return QWebEngineProfile::HttpCacheType(d->browserContext()->httpCacheType());
}

/*!
    Sets the HTTP cache type to \a httpCacheType.

    \sa httpCacheType(), setCachePath()
*/
void QWebEngineProfile::setHttpCacheType(QWebEngineProfile::HttpCacheType httpCacheType)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setHttpCacheType(BrowserContextAdapter::HttpCacheType(httpCacheType));
}

/*!
    Sets the value of the Accept-Language HTTP request-header field to \a httpAcceptLanguage.

    \since 5.6
 */
void QWebEngineProfile::setHttpAcceptLanguage(const QString &httpAcceptLanguage)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setHttpAcceptLanguage(httpAcceptLanguage);
}

/*!
    Returns the value of the Accept-Language HTTP request-header field.

    \since 5.6
 */
QString QWebEngineProfile::httpAcceptLanguage() const
{
    Q_D(const QWebEngineProfile);
    return d->browserContext()->httpAcceptLanguage();
}

/*!
    Returns the current policy for persistent cookies.

    If the profile is off-the-record, NoPersistentCookies is returned.

    \sa setPersistentCookiesPolicy()
*/
QWebEngineProfile::PersistentCookiesPolicy QWebEngineProfile::persistentCookiesPolicy() const
{
    const Q_D(QWebEngineProfile);
    return QWebEngineProfile::PersistentCookiesPolicy(d->browserContext()->persistentCookiesPolicy());
}

/*!
    Sets the policy for persistent cookies to \a newPersistentCookiesPolicy.

    \sa persistentCookiesPolicy()
*/
void QWebEngineProfile::setPersistentCookiesPolicy(QWebEngineProfile::PersistentCookiesPolicy newPersistentCookiesPolicy)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setPersistentCookiesPolicy(BrowserContextAdapter::PersistentCookiesPolicy(newPersistentCookiesPolicy));
}

/*!
    Returns the maximum size of the HTTP cache in bytes.

    Will return \c 0 if the size is automatically controlled by QtWebEngine.

    \sa setHttpCacheMaximumSize(), httpCacheType()
*/
int QWebEngineProfile::httpCacheMaximumSize() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->httpCacheMaxSize();
}

/*!
    Sets the maximum size of the HTTP cache to \a maxSize bytes.

    Setting it to \c 0 means the size will be controlled automatically by QtWebEngine.

    \sa httpCacheMaximumSize(), setHttpCacheType()
*/
void QWebEngineProfile::setHttpCacheMaximumSize(int maxSize)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setHttpCacheMaxSize(maxSize);
}

/*!
    Returns the cookie store for this profile.

    \since 5.6
*/

QWebEngineCookieStore* QWebEngineProfile::cookieStore()
{
    Q_D(QWebEngineProfile);
    return d->browserContext()->cookieStore();
}


/*!
    Registers a request interceptor singleton \a interceptor to intercept URL requests.

    The profile does not take ownership of the pointer.

    \since 5.6
    \sa QWebEngineUrlRequestInfo
*/

void QWebEngineProfile::setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setRequestInterceptor(interceptor);
}

/*!
    Clears all links from the visited links database.

    \sa clearVisitedLinks()
*/
void QWebEngineProfile::clearAllVisitedLinks()
{
    Q_D(QWebEngineProfile);
    d->browserContext()->visitedLinksManager()->deleteAllVisitedLinkData();
}

/*!
    Clears the links in \a urls from the visited links database.

    \sa clearAllVisitedLinks()
*/
void QWebEngineProfile::clearVisitedLinks(const QList<QUrl> &urls)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->visitedLinksManager()->deleteVisitedLinkDataForUrls(urls);
}

/*!
    Returns \c true if \a url is considered a visited link by this profile.
*/
bool QWebEngineProfile::visitedLinksContainsUrl(const QUrl &url) const
{
    Q_D(const QWebEngineProfile);
    return d->browserContext()->visitedLinksManager()->containsUrl(url);
}

/*!
    Returns the collection of scripts that are injected into all pages that share
    this profile.

    \sa QWebEngineScriptCollection, QWebEngineScript, QWebEnginePage::scripts()
*/
QWebEngineScriptCollection *QWebEngineProfile::scripts() const
{
    Q_D(const QWebEngineProfile);
    return d->m_scriptCollection.data();
}

/*!
    Returns the default profile.

    The default profile uses the storage name "Default".

    \sa storageName()
*/
QWebEngineProfile *QWebEngineProfile::defaultProfile()
{
    static QWebEngineProfile* profile = new QWebEngineProfile(
                new QWebEngineProfilePrivate(BrowserContextAdapter::defaultContext()),
                BrowserContextAdapter::globalQObjectRoot());
    return profile;
}

/*!
    \since 5.8

    Sets the current list of \a languages for the spell checker.
    Each language should match the name of the \c .bdic dictionary.
    For example, the language \c en-US will load the \c en-US.bdic
    dictionary file.

    Qt WebEngine checks for the \c qtwebengine_dictionaries subdirectory
    first in the local directory and if it is not found, in the Qt
    installation directory.

    On macOS, depending on how Qt WebEngine is configured at build time, there are two possibilities
    how spellchecking data is found:

    \list
        \li Hunspell dictionaries (default) - .bdic dictionaries are used, just like on other
            platforms
        \li Native dictionaries - the macOS spellchecking APIs are used (which means the results
            will depend on the installed OS dictionaries)
    \endlist

    Thus, in the macOS Hunspell case, Qt WebEngine will look in the \e qtwebengine_dictionaries
    subdirectory located inside the application bundle \c Resources directory, and also in the
    \c Resources directory located inside the Qt framework bundle.

    To summarize, in case of Hunspell usage, the following paths are considered:

    \list
        \li QCoreApplication::applicationDirPath()/qtwebengine_dictionaries
            or QCoreApplication::applicationDirPath()/../Contents/Resources/qtwebengine_dictionaries
            (on macOS)
        \li [QLibraryInfo::DataPath]/qtwebengine_dictionaries
            or path/to/QtWebEngineCore.framework/Resources/qtwebengine_dictionaries (Qt framework
            bundle on macOS)
    \endlist

    For more information about how to compile \c .bdic dictionaries, see the
    \l{WebEngine Widgets Spellchecker Example}{Spellchecker Example}.

*/
void QWebEngineProfile::setSpellCheckLanguages(const QStringList &languages)
{
    Q_D(QWebEngineProfile);
    d->browserContext()->setSpellCheckLanguages(languages);
}

/*!
    \since 5.8

    Returns the list of languages used by the spell checker.
*/
QStringList QWebEngineProfile::spellCheckLanguages() const
{
    const Q_D(QWebEngineProfile);
    return d->browserContext()->spellCheckLanguages();
}

/*!
    \since 5.8

    Enables spell checker if \a enable is \c true, otherwise disables it.
    \sa isSpellCheckEnabled()
 */
void QWebEngineProfile::setSpellCheckEnabled(bool enable)
{
     Q_D(QWebEngineProfile);
     d->browserContext()->setSpellCheckEnabled(enable);
}
/*!
    \since 5.8

    Returns \c true if the spell checker is enabled; otherwise returns \c false.
    \sa setSpellCheckEnabled()
 */
bool QWebEngineProfile::isSpellCheckEnabled() const
{
     const Q_D(QWebEngineProfile);
     return d->browserContext()->isSpellCheckEnabled();
}

/*!
    Returns the default settings for all pages in this profile.
*/
QWebEngineSettings *QWebEngineProfile::settings() const
{
    const Q_D(QWebEngineProfile);
    return d->settings();
}

/*!
    \since 5.6

    Returns the custom URL scheme handler register for the URL scheme \a scheme.
*/
const QWebEngineUrlSchemeHandler *QWebEngineProfile::urlSchemeHandler(const QByteArray &scheme) const
{
    const Q_D(QWebEngineProfile);
    if (d->browserContext()->customUrlSchemeHandlers().contains(scheme))
        return d->browserContext()->customUrlSchemeHandlers().value(scheme);
    return 0;
}

static bool checkInternalScheme(const QByteArray &scheme)
{
    static QSet<QByteArray> internalSchemes;
    if (internalSchemes.isEmpty()) {
        internalSchemes << QByteArrayLiteral("qrc") << QByteArrayLiteral("data") << QByteArrayLiteral("blob")
                        << QByteArrayLiteral("http") << QByteArrayLiteral("ftp") << QByteArrayLiteral("javascript");
    }
    return internalSchemes.contains(scheme);
}

/*!
    \since 5.6

    Registers a handler \a handler for custom URL scheme \a scheme in the profile.
*/
void QWebEngineProfile::installUrlSchemeHandler(const QByteArray &scheme, QWebEngineUrlSchemeHandler *handler)
{
    Q_D(QWebEngineProfile);
    Q_ASSERT(handler);
    if (checkInternalScheme(scheme)) {
        qWarning("Cannot install a URL scheme handler overriding internal scheme: %s", scheme.constData());
        return;
    }

    if (d->browserContext()->customUrlSchemeHandlers().contains(scheme)) {
        if (d->browserContext()->customUrlSchemeHandlers().value(scheme) != handler)
            qWarning("URL scheme handler already installed for the scheme: %s", scheme.constData());
        return;
    }
    d->browserContext()->addCustomUrlSchemeHandler(scheme, handler);
    connect(handler, SIGNAL(_q_destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)), this, SLOT(destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)));
}

/*!
    \since 5.6

    Removes the custom URL scheme handler \a handler from the profile.

    \sa removeUrlScheme()
*/
void QWebEngineProfile::removeUrlSchemeHandler(QWebEngineUrlSchemeHandler *handler)
{
    Q_D(QWebEngineProfile);
    Q_ASSERT(handler);
    if (!d->browserContext()->removeCustomUrlSchemeHandler(handler))
        return;
    disconnect(handler, SIGNAL(_q_destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)), this, SLOT(destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)));
}

/*!
    \since 5.6

    Removes the custom URL scheme \a scheme from the profile.

    \sa removeUrlSchemeHandler()
*/
void QWebEngineProfile::removeUrlScheme(const QByteArray &scheme)
{
    Q_D(QWebEngineProfile);
    QWebEngineUrlSchemeHandler *handler = d->browserContext()->takeCustomUrlSchemeHandler(scheme);
    if (!handler)
        return;
    disconnect(handler, SIGNAL(_q_destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)), this, SLOT(destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)));
}

/*!
    \since 5.6

    Removes all custom URL scheme handlers installed in the profile.
*/
void QWebEngineProfile::removeAllUrlSchemeHandlers()
{
    Q_D(QWebEngineProfile);
    d->browserContext()->clearCustomUrlSchemeHandlers();
}

void QWebEngineProfile::destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler *obj)
{
    removeUrlSchemeHandler(obj);
}

/*!
    \since 5.7

    Removes the profile's cache entries.
*/
void QWebEngineProfile::clearHttpCache()
{
    Q_D(QWebEngineProfile);
    d->browserContext()->clearHttpCache();
}

QT_END_NAMESPACE
