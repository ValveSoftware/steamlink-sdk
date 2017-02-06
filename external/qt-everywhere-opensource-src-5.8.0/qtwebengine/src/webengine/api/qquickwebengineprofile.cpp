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

#include "qquickwebengineprofile.h"

#include "qquickwebenginedownloaditem_p.h"
#include "qquickwebenginedownloaditem_p_p.h"
#include "qquickwebengineprofile_p.h"
#include "qquickwebenginesettings_p.h"
#include "qwebenginecookiestore.h"

#include <QQmlEngine>

#include "browser_context_adapter.h"
#include <qtwebenginecoreglobal.h>
#include "web_engine_settings.h"

using QtWebEngineCore::BrowserContextAdapter;

QT_BEGIN_NAMESPACE

ASSERT_ENUMS_MATCH(QQuickWebEngineDownloadItem::UnknownSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::UnknownSavePageFormat)
ASSERT_ENUMS_MATCH(QQuickWebEngineDownloadItem::SingleHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::SingleHtmlSaveFormat)
ASSERT_ENUMS_MATCH(QQuickWebEngineDownloadItem::CompleteHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::CompleteHtmlSaveFormat)
ASSERT_ENUMS_MATCH(QQuickWebEngineDownloadItem::MimeHtmlSaveFormat, QtWebEngineCore::BrowserContextAdapterClient::MimeHtmlSaveFormat)

/*!
    \class QQuickWebEngineProfile
    \brief The QQuickWebEngineProfile class provides a web engine profile shared by multiple pages.
    \since 5.6

    \inmodule QtWebEngine

    A web engine profile contains properties and functionality shared by a group of web engine
    pages.

    Information about visited links is stored together with persistent cookies and other persistent
    data in a storage described by the persistentStoragePath property.

    Profiles can be used to isolate pages from each other. A typical use case is a dedicated
    \e {off-the-record profile} for a \e {private browsing} mode. An off-the-record profile forces
    cookies, the HTTP cache, and other normally persistent data to be stored only in memory. The
    offTheRecord property holds whether a profile is off-the-record.

    The default profile can be accessed by defaultProfile(). It is a built-in profile that all
    web pages not specifically created with another profile belong to.

    A WebEngineProfile instance can be created and accessed from C++ through the
    QQuickWebEngineProfile class, which exposes further functionality in C++. This allows Qt Quick
    applications to intercept URL requests (QQuickWebEngineProfile::setRequestInterceptor), or
    register custom URL schemes (QQuickWebEngineProfile::installUrlSchemeHandler).

    Spellchecking HTML form fields can be enabled per profile by setting the \l spellCheckEnabled
    property and the current languages used for spellchecking can be set by using the
    \l spellCheckLanguages property.
*/

/*!
    \enum QQuickWebEngineProfile::HttpCacheType

    This enum describes the HTTP cache type:

    \value MemoryHttpCache Use an in-memory cache. This is the only setting possible if
    \c off-the-record is set or no cache path is available.
    \value DiskHttpCache Use a disk cache. This is the default.
    \value NoCache Disable both in-memory and disk caching. (Added in Qt 5.7)
*/

/*!
    \enum QQuickWebEngineProfile::PersistentCookiesPolicy

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
  \fn QQuickWebEngineProfile::downloadRequested(QQuickWebEngineDownloadItem *download)

  This signal is emitted whenever a download has been triggered.
  The \a download argument holds the state of the download.
  The download has to be explicitly accepted with
  \c{QQuickWebEngineDownloadItem::accept()} or it will be
  cancelled by default.
  The download item is parented by the profile. If it is not accepted, it
  will be deleted immediately after the signal emission.
  This signal cannot be used with a queued connection.
*/

/*!
  \fn QQuickWebEngineProfile::downloadFinished(QQuickWebEngineDownloadItem *download)

  This signal is emitted whenever downloading stops, because it finished successfully, was
  cancelled, or was interrupted (for example, because connectivity was lost).
  The \a download argument holds the state of the finished download instance.
*/

QQuickWebEngineProfilePrivate::QQuickWebEngineProfilePrivate(QSharedPointer<BrowserContextAdapter> browserContext)
        : m_settings(new QQuickWebEngineSettings())
        , m_browserContextRef(browserContext)
{
    m_browserContextRef->addClient(this);
    m_settings->d_ptr->initDefaults(browserContext->isOffTheRecord());
    // Fullscreen API was implemented before the supported setting, so we must
    // make it default true to avoid change in default API behavior.
    m_settings->d_ptr->setAttribute(QtWebEngineCore::WebEngineSettings::FullScreenSupportEnabled, true);
}

QQuickWebEngineProfilePrivate::~QQuickWebEngineProfilePrivate()
{
    m_browserContextRef->removeClient(this);

    Q_FOREACH (QQuickWebEngineDownloadItem* download, m_ongoingDownloads) {
        if (download)
            download->cancel();
    }

    m_ongoingDownloads.clear();
}

void QQuickWebEngineProfilePrivate::cancelDownload(quint32 downloadId)
{
    browserContext()->cancelDownload(downloadId);
}

void QQuickWebEngineProfilePrivate::downloadDestroyed(quint32 downloadId)
{
    m_ongoingDownloads.remove(downloadId);
}

void QQuickWebEngineProfilePrivate::downloadRequested(DownloadItemInfo &info)
{
    Q_Q(QQuickWebEngineProfile);

    Q_ASSERT(!m_ongoingDownloads.contains(info.id));
    QQuickWebEngineDownloadItemPrivate *itemPrivate = new QQuickWebEngineDownloadItemPrivate(q);
    itemPrivate->downloadId = info.id;
    itemPrivate->downloadState = QQuickWebEngineDownloadItem::DownloadRequested;
    itemPrivate->totalBytes = info.totalBytes;
    itemPrivate->mimeType = info.mimeType;
    itemPrivate->downloadPath = info.path;
    itemPrivate->savePageFormat = static_cast<QQuickWebEngineDownloadItem::SavePageFormat>(
                info.savePageFormat);
    itemPrivate->type = static_cast<QQuickWebEngineDownloadItem::DownloadType>(info.downloadType);

    QQuickWebEngineDownloadItem *download = new QQuickWebEngineDownloadItem(itemPrivate, q);

    m_ongoingDownloads.insert(info.id, download);

    QQmlEngine::setObjectOwnership(download, QQmlEngine::JavaScriptOwnership);
    Q_EMIT q->downloadRequested(download);

    QQuickWebEngineDownloadItem::DownloadState state = download->state();
    info.path = download->path();
    info.savePageFormat = itemPrivate->savePageFormat;
    info.accepted = state != QQuickWebEngineDownloadItem::DownloadCancelled
                      && state != QQuickWebEngineDownloadItem::DownloadRequested;

    if (state == QQuickWebEngineDownloadItem::DownloadRequested) {
        // Delete unaccepted downloads.
        info.accepted = false;
        m_ongoingDownloads.remove(info.id);
        delete download;
    }
}

void QQuickWebEngineProfilePrivate::downloadUpdated(const DownloadItemInfo &info)
{
    if (!m_ongoingDownloads.contains(info.id))
        return;

    Q_Q(QQuickWebEngineProfile);

    QQuickWebEngineDownloadItem* download = m_ongoingDownloads.value(info.id).data();

    if (!download) {
        downloadDestroyed(info.id);
        return;
    }

    download->d_func()->update(info);

    if (info.state != BrowserContextAdapterClient::DownloadInProgress) {
        Q_EMIT q->downloadFinished(download);
        m_ongoingDownloads.remove(info.id);
    }
}

/*!
    \qmltype WebEngineProfile
    \instantiates QQuickWebEngineProfile
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1
    \brief Contains settings, scripts, and visited links common to multiple web engine views.

    WebEngineProfile contains settings, scripts, and the list of visited links shared by all
    views that belong to the profile. As such, profiles can be used to isolate views
    from each other. A typical use case is a dedicated profile for a 'private browsing' mode.

    Each web engine view has an associated profile. Views that do not have a specific profile set
    share a common default one.
*/

/*!
    \qmlsignal WebEngineProfile::downloadRequested(WebEngineDownloadItem download)

    This signal is emitted whenever a download has been triggered.
    The \a download argument holds the state of the download.
    The download has to be explicitly accepted with WebEngineDownloadItem::accept() or the
    download will be cancelled by default.
*/

/*!
    \qmlsignal WebEngineProfile::downloadFinished(WebEngineDownloadItem download)

    This signal is emitted whenever downloading stops, because it finished successfully, was
    cancelled, or was interrupted (for example, because connectivity was lost).
    The \a download argument holds the state of the finished download instance.
*/

/*!
    Constructs a new profile with the parent \a parent.
*/
QQuickWebEngineProfile::QQuickWebEngineProfile(QObject *parent)
    : QObject(parent),
      d_ptr(new QQuickWebEngineProfilePrivate(QSharedPointer<BrowserContextAdapter>::create(false)))
{
    // Sets up the global WebEngineContext
    QQuickWebEngineProfile::defaultProfile();
    d_ptr->q_ptr = this;
}

QQuickWebEngineProfile::QQuickWebEngineProfile(QQuickWebEngineProfilePrivate *privatePtr, QObject *parent)
    : QObject(parent)
    , d_ptr(privatePtr)
{
    d_ptr->q_ptr = this;
}

/*!
   \internal
*/
QQuickWebEngineProfile::~QQuickWebEngineProfile()
{
}

/*!
    \qmlproperty string WebEngineProfile::storageName

    The storage name that is used to create separate subdirectories for each profile that uses
    the disk for storing persistent data and cache.

    \sa WebEngineProfile::persistentStoragePath, WebEngineProfile::cachePath
*/

/*!
    \property QQuickWebEngineProfile::storageName

    The storage name that is used to create separate subdirectories for each profile that uses
    the disk for storing persistent data and cache.

    \sa QQuickWebEngineProfile::persistentStoragePath, QQuickWebEngineProfile::cachePath
*/

QString QQuickWebEngineProfile::storageName() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->storageName();
}

void QQuickWebEngineProfile::setStorageName(const QString &name)
{
    Q_D(QQuickWebEngineProfile);
    if (d->browserContext()->storageName() == name)
        return;
    BrowserContextAdapter::HttpCacheType oldCacheType = d->browserContext()->httpCacheType();
    BrowserContextAdapter::PersistentCookiesPolicy oldPolicy = d->browserContext()->persistentCookiesPolicy();
    d->browserContext()->setStorageName(name);
    emit storageNameChanged();
    emit persistentStoragePathChanged();
    emit cachePathChanged();
    if (d->browserContext()->httpCacheType() != oldCacheType)
        emit httpCacheTypeChanged();
    if (d->browserContext()->persistentCookiesPolicy() != oldPolicy)
        emit persistentCookiesPolicyChanged();
}

/*!
    \qmlproperty bool WebEngineProfile::offTheRecord

    Whether the web engine profile is \e off-the-record.
    An off-the-record profile forces cookies, the HTTP cache, and other normally persistent data
    to be stored only in memory.
*/


/*!
    \property QQuickWebEngineProfile::offTheRecord

    Whether the web engine profile is \e off-the-record.
    An off-the-record profile forces cookies, the HTTP cache, and other normally persistent data
    to be stored only in memory.
*/

bool QQuickWebEngineProfile::isOffTheRecord() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->isOffTheRecord();
}

void QQuickWebEngineProfile::setOffTheRecord(bool offTheRecord)
{
    Q_D(QQuickWebEngineProfile);
    if (d->browserContext()->isOffTheRecord() == offTheRecord)
        return;
    BrowserContextAdapter::HttpCacheType oldCacheType = d->browserContext()->httpCacheType();
    BrowserContextAdapter::PersistentCookiesPolicy oldPolicy = d->browserContext()->persistentCookiesPolicy();
    d->browserContext()->setOffTheRecord(offTheRecord);
    emit offTheRecordChanged();
    if (d->browserContext()->httpCacheType() != oldCacheType)
        emit httpCacheTypeChanged();
    if (d->browserContext()->persistentCookiesPolicy() != oldPolicy)
        emit persistentCookiesPolicyChanged();
}

/*!
    \qmlproperty string WebEngineProfile::persistentStoragePath

    The path to the location where the persistent data for the browser and web content are
    stored. Persistent data includes persistent cookies, HTML5 local storage, and visited links.

    By default, the storage is located below
    QStandardPaths::writableLocation(QStandardPaths::DataLocation) in a directory named using
    storageName.
*/

/*!
    \property QQuickWebEngineProfile::persistentStoragePath

    The path to the location where the persistent data for the browser and web content are
    stored. Persistent data includes persistent cookies, HTML5 local storage, and visited links.

    By default, the storage is located below
    QStandardPaths::writableLocation(QStandardPaths::DataLocation) in a directory named using
    storageName.
*/

QString QQuickWebEngineProfile::persistentStoragePath() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->dataPath();
}

void QQuickWebEngineProfile::setPersistentStoragePath(const QString &path)
{
    Q_D(QQuickWebEngineProfile);
    if (persistentStoragePath() == path)
        return;
    d->browserContext()->setDataPath(path);
    emit persistentStoragePathChanged();
}

/*!
    \qmlproperty string WebEngineProfile::cachePath

    The path to the location where the profile's caches are stored, in particular the HTTP cache.

    By default, the caches are stored
    below QStandardPaths::writableLocation(QStandardPaths::CacheLocation) in a directory named using
    storageName.
*/

/*!
    \property QQuickWebEngineProfile::cachePath

    The path to the location where the profile's caches are stored, in particular the HTTP cache.

    By default, the caches are stored
    below QStandardPaths::writableLocation(QStandardPaths::CacheLocation) in a directory named using
    storageName.
*/

QString QQuickWebEngineProfile::cachePath() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->cachePath();
}

void QQuickWebEngineProfile::setCachePath(const QString &path)
{
    Q_D(QQuickWebEngineProfile);
    if (cachePath() == path)
        return;
    d->browserContext()->setCachePath(path);
    emit cachePathChanged();
}

/*!
    \qmlproperty string WebEngineProfile::httpUserAgent

    The user-agent string sent with HTTP to identify the browser.
*/

/*!
    \property QQuickWebEngineProfile::httpUserAgent

    The user-agent string sent with HTTP to identify the browser.
*/

QString QQuickWebEngineProfile::httpUserAgent() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->httpUserAgent();
}

void QQuickWebEngineProfile::setHttpUserAgent(const QString &userAgent)
{
    Q_D(QQuickWebEngineProfile);
    if (d->browserContext()->httpUserAgent() == userAgent)
        return;
    d->browserContext()->setHttpUserAgent(userAgent);
    emit httpUserAgentChanged();
}


/*!
    \qmlproperty enumeration WebEngineProfile::httpCacheType

    This enumeration describes the type of the HTTP cache:

    \value  WebEngineProfile.MemoryHttpCache
            Uses an in-memory cache. This is the only setting possible if offTheRecord is set or
            no persistentStoragePath is available.
    \value  WebEngineProfile.DiskHttpCache
            Uses a disk cache. This is the default value.
    \value  WebEngineProfile.NoCache
            Disables caching. (Added in 5.7)
*/

/*!
    \property QQuickWebEngineProfile::httpCacheType

    This enumeration describes the type of the HTTP cache.

    If the profile is off-the-record, MemoryHttpCache is returned.
*/

QQuickWebEngineProfile::HttpCacheType QQuickWebEngineProfile::httpCacheType() const
{
    const Q_D(QQuickWebEngineProfile);
    return QQuickWebEngineProfile::HttpCacheType(d->browserContext()->httpCacheType());
}

void QQuickWebEngineProfile::setHttpCacheType(QQuickWebEngineProfile::HttpCacheType httpCacheType)
{
    Q_D(QQuickWebEngineProfile);
    BrowserContextAdapter::HttpCacheType oldCacheType = d->browserContext()->httpCacheType();
    d->browserContext()->setHttpCacheType(BrowserContextAdapter::HttpCacheType(httpCacheType));
    if (d->browserContext()->httpCacheType() != oldCacheType)
        emit httpCacheTypeChanged();
}

/*!
    \qmlproperty enumeration WebEngineProfile::persistentCookiesPolicy

    This enumeration describes the policy of cookie persistency:

    \value  WebEngineProfile.NoPersistentCookies
            Both session and persistent cookies are stored in memory. This is the only setting
            possible if offTheRecord is set or no persistentStoragePath is available.
    \value  WebEngineProfile.AllowPersistentCookies
            Cookies marked persistent are saved to and restored from disk, whereas session cookies
            are only stored to disk for crash recovery. This is the default setting.
    \value WebEngineProfile.ForcePersistentCookies
            Both session and persistent cookies are saved to and restored from disk.
*/

/*!
    \property QQuickWebEngineProfile::persistentCookiesPolicy

    This enumeration describes the policy of cookie persistency.
    If the profile is off-the-record, NoPersistentCookies is returned.
*/

QQuickWebEngineProfile::PersistentCookiesPolicy QQuickWebEngineProfile::persistentCookiesPolicy() const
{
    const Q_D(QQuickWebEngineProfile);
    return QQuickWebEngineProfile::PersistentCookiesPolicy(d->browserContext()->persistentCookiesPolicy());
}

void QQuickWebEngineProfile::setPersistentCookiesPolicy(QQuickWebEngineProfile::PersistentCookiesPolicy newPersistentCookiesPolicy)
{
    Q_D(QQuickWebEngineProfile);
    BrowserContextAdapter::PersistentCookiesPolicy oldPolicy = d->browserContext()->persistentCookiesPolicy();
    d->browserContext()->setPersistentCookiesPolicy(BrowserContextAdapter::PersistentCookiesPolicy(newPersistentCookiesPolicy));
    if (d->browserContext()->persistentCookiesPolicy() != oldPolicy)
        emit persistentCookiesPolicyChanged();
}

/*!
    \qmlproperty int WebEngineProfile::httpCacheMaximumSize

    The maximum size of the HTTP cache. If \c 0, the size will be controlled automatically by
    QtWebEngine. The default value is \c 0.

    \sa httpCacheType
*/

/*!
    \property QQuickWebEngineProfile::httpCacheMaximumSize

    The maximum size of the HTTP cache. If \c 0, the size will be controlled automatically by
    QtWebEngine. The default value is \c 0.

    \sa httpCacheType
*/

int QQuickWebEngineProfile::httpCacheMaximumSize() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->httpCacheMaxSize();
}

void QQuickWebEngineProfile::setHttpCacheMaximumSize(int maximumSize)
{
    Q_D(QQuickWebEngineProfile);
    if (d->browserContext()->httpCacheMaxSize() == maximumSize)
        return;
    d->browserContext()->setHttpCacheMaxSize(maximumSize);
    emit httpCacheMaximumSizeChanged();
}

/*!
    \qmlproperty string WebEngineProfile::httpAcceptLanguage

    The value of the Accept-Language HTTP request-header field.

    \since QtWebEngine 1.2
*/

/*!
    \property QQuickWebEngineProfile::httpAcceptLanguage

    The value of the Accept-Language HTTP request-header field.
*/

QString QQuickWebEngineProfile::httpAcceptLanguage() const
{
    Q_D(const QQuickWebEngineProfile);
    return d->browserContext()->httpAcceptLanguage();
}

void QQuickWebEngineProfile::setHttpAcceptLanguage(const QString &httpAcceptLanguage)
{
    Q_D(QQuickWebEngineProfile);
    if (d->browserContext()->httpAcceptLanguage() == httpAcceptLanguage)
        return;
    d->browserContext()->setHttpAcceptLanguage(httpAcceptLanguage);
    emit httpAcceptLanguageChanged();
}

/*!
    Returns the default profile.

    The default profile uses the storage name "Default".

    \sa storageName()
*/
QQuickWebEngineProfile *QQuickWebEngineProfile::defaultProfile()
{
    static QQuickWebEngineProfile *profile = new QQuickWebEngineProfile(
                new QQuickWebEngineProfilePrivate(BrowserContextAdapter::defaultContext()),
                BrowserContextAdapter::globalQObjectRoot());
    return profile;
}

/*!
    \property QQuickWebEngineProfile::spellCheckLanguages
    \brief The languages used by the spell checker.

    \since QtWebEngine 1.4
*/

/*!
    \qmlproperty list<string> WebEngineProfile::spellCheckLanguages

    This property holds the list of languages used by the spell checker.
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

    \since QtWebEngine 1.4
*/
void QQuickWebEngineProfile::setSpellCheckLanguages(const QStringList &languages)
{
    Q_D(QQuickWebEngineProfile);
    if (languages != d->browserContext()->spellCheckLanguages()) {
        d->browserContext()->setSpellCheckLanguages(languages);
        emit spellCheckLanguagesChanged();
    }
}

/*!
    \since 5.8

    Returns the list of languages used by the spell checker.
*/
QStringList QQuickWebEngineProfile::spellCheckLanguages() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->spellCheckLanguages();
}

/*!
    \property QQuickWebEngineProfile::spellCheckEnabled
    \brief whether the web engine spell checker is enabled.

    \since QtWebEngine 1.4
*/

/*!
    \qmlproperty bool WebEngineProfile::spellCheckEnabled

    This property holds whether the web engine spell checker is enabled.

    \since QtWebEngine 1.4
*/
void QQuickWebEngineProfile::setSpellCheckEnabled(bool enable)
{
     Q_D(QQuickWebEngineProfile);
    if (enable != isSpellCheckEnabled()) {
        d->browserContext()->setSpellCheckEnabled(enable);
        emit spellCheckEnabledChanged();
    }
}

bool QQuickWebEngineProfile::isSpellCheckEnabled() const
{
     const Q_D(QQuickWebEngineProfile);
     return d->browserContext()->isSpellCheckEnabled();
}

/*!

    Returns the cookie store for this profile.
*/
QWebEngineCookieStore *QQuickWebEngineProfile::cookieStore() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->browserContext()->cookieStore();
}

/*!
    \qmlmethod void WebEngineProfile::clearHttpCache()
    \since QtWebEngine 1.3

    Removes the profile's cache entries.

    \sa WebEngineProfile::cachePath
*/

/*!
    \since 5.7

    Removes the profile's cache entries.

    \sa WebEngineProfile::clearHttpCache
*/
void QQuickWebEngineProfile::clearHttpCache()
{
    Q_D(QQuickWebEngineProfile);
    d->browserContext()->clearHttpCache();
}


/*!
    Registers a request interceptor singleton \a interceptor to intercept URL requests.

    The profile does not take ownership of the pointer.

    \sa QWebEngineUrlRequestInterceptor
*/
void QQuickWebEngineProfile::setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor)
{
    Q_D(QQuickWebEngineProfile);
    d->browserContext()->setRequestInterceptor(interceptor);
}

/*!
    Returns the custom URL scheme handler register for the URL scheme \a scheme.
*/
const QWebEngineUrlSchemeHandler *QQuickWebEngineProfile::urlSchemeHandler(const QByteArray &scheme) const
{
    const Q_D(QQuickWebEngineProfile);
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
    Registers a handler \a handler for custom URL scheme \a scheme in the profile.
*/
void QQuickWebEngineProfile::installUrlSchemeHandler(const QByteArray &scheme, QWebEngineUrlSchemeHandler *handler)
{
    Q_D(QQuickWebEngineProfile);
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
    Removes the custom URL scheme handler \a handler from the profile.

    \sa removeUrlScheme()
*/
void QQuickWebEngineProfile::removeUrlSchemeHandler(QWebEngineUrlSchemeHandler *handler)
{
    Q_D(QQuickWebEngineProfile);
    Q_ASSERT(handler);
    if (!d->browserContext()->removeCustomUrlSchemeHandler(handler))
        return;
    disconnect(handler, SIGNAL(_q_destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)), this, SLOT(destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)));
}

/*!
    Removes the custom URL scheme \a scheme from the profile.

    \sa removeUrlSchemeHandler()
*/
void QQuickWebEngineProfile::removeUrlScheme(const QByteArray &scheme)
{
    Q_D(QQuickWebEngineProfile);
    QWebEngineUrlSchemeHandler *handler = d->browserContext()->takeCustomUrlSchemeHandler(scheme);
    if (!handler)
        return;
    disconnect(handler, SIGNAL(_q_destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)), this, SLOT(destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler*)));
}

/*!
    Removes all custom URL scheme handlers installed in the profile.
*/
void QQuickWebEngineProfile::removeAllUrlSchemeHandlers()
{
    Q_D(QQuickWebEngineProfile);
    d->browserContext()->clearCustomUrlSchemeHandlers();
}

void QQuickWebEngineProfile::destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler *obj)
{
    removeUrlSchemeHandler(obj);
}

QQuickWebEngineSettings *QQuickWebEngineProfile::settings() const
{
    const Q_D(QQuickWebEngineProfile);
    return d->settings();
}

QT_END_NAMESPACE
