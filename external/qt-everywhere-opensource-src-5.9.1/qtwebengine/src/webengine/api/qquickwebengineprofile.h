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

#ifndef QQUICKWEBENGINEPROFILE_H
#define QQUICKWEBENGINEPROFILE_H


#include <QtWebEngine/qtwebengineglobal.h>

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>
#include <QtQml/QQmlListProperty>

namespace QtWebEngineCore {
class BrowserContextAdapter;
}

QT_BEGIN_NAMESPACE

class QQuickWebEngineDownloadItem;
class QQuickWebEngineProfilePrivate;
class QQuickWebEngineScript;
class QQuickWebEngineSettings;
class QWebEngineCookieStore;
class QWebEngineUrlRequestInterceptor;
class QWebEngineUrlSchemeHandler;

class Q_WEBENGINE_EXPORT QQuickWebEngineProfile : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString storageName READ storageName WRITE setStorageName NOTIFY storageNameChanged FINAL)
    Q_PROPERTY(bool offTheRecord READ isOffTheRecord WRITE setOffTheRecord NOTIFY offTheRecordChanged FINAL)
    Q_PROPERTY(QString persistentStoragePath READ persistentStoragePath WRITE setPersistentStoragePath NOTIFY persistentStoragePathChanged FINAL)
    Q_PROPERTY(QString cachePath READ cachePath WRITE setCachePath NOTIFY cachePathChanged FINAL)
    Q_PROPERTY(QString httpUserAgent READ httpUserAgent WRITE setHttpUserAgent NOTIFY httpUserAgentChanged FINAL)
    Q_PROPERTY(HttpCacheType httpCacheType READ httpCacheType WRITE setHttpCacheType NOTIFY httpCacheTypeChanged FINAL)
    Q_PROPERTY(QString httpAcceptLanguage READ httpAcceptLanguage WRITE setHttpAcceptLanguage NOTIFY httpAcceptLanguageChanged FINAL REVISION 1)
    Q_PROPERTY(PersistentCookiesPolicy persistentCookiesPolicy READ persistentCookiesPolicy WRITE setPersistentCookiesPolicy NOTIFY persistentCookiesPolicyChanged FINAL)
    Q_PROPERTY(int httpCacheMaximumSize READ httpCacheMaximumSize WRITE setHttpCacheMaximumSize NOTIFY httpCacheMaximumSizeChanged FINAL)
    Q_PROPERTY(QStringList spellCheckLanguages READ spellCheckLanguages WRITE setSpellCheckLanguages NOTIFY spellCheckLanguagesChanged FINAL REVISION 3)
    Q_PROPERTY(bool spellCheckEnabled READ isSpellCheckEnabled WRITE setSpellCheckEnabled NOTIFY spellCheckEnabledChanged FINAL REVISION 3)
    Q_PROPERTY(QQmlListProperty<QQuickWebEngineScript> userScripts READ userScripts FINAL REVISION 4)

public:
    QQuickWebEngineProfile(QObject *parent = Q_NULLPTR);
    ~QQuickWebEngineProfile();

    enum HttpCacheType {
        MemoryHttpCache,
        DiskHttpCache,
        NoCache
    };
    Q_ENUM(HttpCacheType)

    enum PersistentCookiesPolicy {
        NoPersistentCookies,
        AllowPersistentCookies,
        ForcePersistentCookies
    };
    Q_ENUM(PersistentCookiesPolicy)

    QString storageName() const;
    void setStorageName(const QString &name);

    bool isOffTheRecord() const;
    void setOffTheRecord(bool offTheRecord);

    QString persistentStoragePath() const;
    void setPersistentStoragePath(const QString &path);

    QString cachePath() const;
    void setCachePath(const QString &path);

    QString httpUserAgent() const;
    void setHttpUserAgent(const QString &userAgent);

    HttpCacheType httpCacheType() const;
    void setHttpCacheType(QQuickWebEngineProfile::HttpCacheType);

    PersistentCookiesPolicy persistentCookiesPolicy() const;
    void setPersistentCookiesPolicy(QQuickWebEngineProfile::PersistentCookiesPolicy);

    int httpCacheMaximumSize() const;
    void setHttpCacheMaximumSize(int maxSize);

    QString httpAcceptLanguage() const;
    void setHttpAcceptLanguage(const QString &httpAcceptLanguage);

    QWebEngineCookieStore *cookieStore() const;

    void setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor);

    const QWebEngineUrlSchemeHandler *urlSchemeHandler(const QByteArray &) const;
    void installUrlSchemeHandler(const QByteArray &scheme, QWebEngineUrlSchemeHandler *);
    void removeUrlScheme(const QByteArray &scheme);
    void removeUrlSchemeHandler(QWebEngineUrlSchemeHandler *);
    void removeAllUrlSchemeHandlers();

    Q_REVISION(2) Q_INVOKABLE void clearHttpCache();

    void setSpellCheckLanguages(const QStringList &languages);
    QStringList spellCheckLanguages() const;
    void setSpellCheckEnabled(bool enabled);
    bool isSpellCheckEnabled() const;

    QQmlListProperty<QQuickWebEngineScript> userScripts();

    static QQuickWebEngineProfile *defaultProfile();

Q_SIGNALS:
    void storageNameChanged();
    void offTheRecordChanged();
    void persistentStoragePathChanged();
    void cachePathChanged();
    void httpUserAgentChanged();
    void httpCacheTypeChanged();
    void persistentCookiesPolicyChanged();
    void httpCacheMaximumSizeChanged();
    Q_REVISION(1) void httpAcceptLanguageChanged();
    Q_REVISION(3) void spellCheckLanguagesChanged();
    Q_REVISION(3) void spellCheckEnabledChanged();

    void downloadRequested(QQuickWebEngineDownloadItem *download);
    void downloadFinished(QQuickWebEngineDownloadItem *download);

private Q_SLOTS:
    void destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler *obj);

private:
    Q_DECLARE_PRIVATE(QQuickWebEngineProfile)
    QQuickWebEngineProfile(QQuickWebEngineProfilePrivate *, QObject *parent = Q_NULLPTR);
    QQuickWebEngineSettings *settings() const;

    friend class QQuickWebEngineSettings;
    friend class QQuickWebEngineSingleton;
    friend class QQuickWebEngineViewPrivate;
    friend class QQuickWebEngineDownloadItem;
    friend class QQuickWebEngineDownloadItemPrivate;
    QScopedPointer<QQuickWebEngineProfilePrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QQUICKWEBENGINEPROFILE_H
