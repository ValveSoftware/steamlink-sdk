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

#ifndef QWEBENGINEPROFILE_H
#define QWEBENGINEPROFILE_H

#include <QtWebEngineWidgets/qtwebenginewidgetsglobal.h>

#include <QtCore/qobject.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE

class QObject;
class QUrl;
class QWebEngineCookieStore;
class QWebEngineDownloadItem;
class QWebEnginePage;
class QWebEnginePagePrivate;
class QWebEngineProfilePrivate;
class QWebEngineSettings;
class QWebEngineScriptCollection;
class QWebEngineUrlRequestInterceptor;
class QWebEngineUrlSchemeHandler;

class QWEBENGINEWIDGETS_EXPORT QWebEngineProfile : public QObject {
    Q_OBJECT
public:
    explicit QWebEngineProfile(QObject *parent = Q_NULLPTR);
    explicit QWebEngineProfile(const QString &name, QObject *parent = Q_NULLPTR);
    virtual ~QWebEngineProfile();

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
    bool isOffTheRecord() const;

    QString persistentStoragePath() const;
    void setPersistentStoragePath(const QString &path);

    QString cachePath() const;
    void setCachePath(const QString &path);

    QString httpUserAgent() const;
    void setHttpUserAgent(const QString &userAgent);

    HttpCacheType httpCacheType() const;
    void setHttpCacheType(QWebEngineProfile::HttpCacheType);

    void setHttpAcceptLanguage(const QString &httpAcceptLanguage);
    QString httpAcceptLanguage() const;

    PersistentCookiesPolicy persistentCookiesPolicy() const;
    void setPersistentCookiesPolicy(QWebEngineProfile::PersistentCookiesPolicy);

    int httpCacheMaximumSize() const;
    void setHttpCacheMaximumSize(int maxSize);

    QWebEngineCookieStore* cookieStore();
    void setRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor);

    void clearAllVisitedLinks();
    void clearVisitedLinks(const QList<QUrl> &urls);
    bool visitedLinksContainsUrl(const QUrl &url) const;

    QWebEngineSettings *settings() const;
    QWebEngineScriptCollection *scripts() const;

    const QWebEngineUrlSchemeHandler *urlSchemeHandler(const QByteArray &) const;
    void installUrlSchemeHandler(const QByteArray &scheme, QWebEngineUrlSchemeHandler *);
    void removeUrlScheme(const QByteArray &scheme);
    void removeUrlSchemeHandler(QWebEngineUrlSchemeHandler *);
    void removeAllUrlSchemeHandlers();

    void clearHttpCache();

    void setSpellCheckLanguages(const QStringList &languages);
    QStringList spellCheckLanguages() const;
    void setSpellCheckEnabled(bool enabled);
    bool isSpellCheckEnabled() const;

    static QWebEngineProfile *defaultProfile();

Q_SIGNALS:
    void downloadRequested(QWebEngineDownloadItem *download);

private Q_SLOTS:
    void destroyedUrlSchemeHandler(QWebEngineUrlSchemeHandler *obj);

private:
    Q_DISABLE_COPY(QWebEngineProfile)
    Q_DECLARE_PRIVATE(QWebEngineProfile)
    QWebEngineProfile(QWebEngineProfilePrivate *, QObject *parent = Q_NULLPTR);

    friend class QWebEnginePagePrivate;
    friend class QWebEngineUrlSchemeHandler;
    QScopedPointer<QWebEngineProfilePrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QWEBENGINEPROFILE_H
