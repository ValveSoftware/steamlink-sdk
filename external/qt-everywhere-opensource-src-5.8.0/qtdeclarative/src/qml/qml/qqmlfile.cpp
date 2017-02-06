/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlfile.h"

#include <QtCore/qurl.h>
#include <QtCore/qobject.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qfile.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>

/*!
\class QQmlFile
\brief The QQmlFile class gives access to local and remote files.

\internal

Supports file:// and qrc:/ uris and whatever QNetworkAccessManager supports.
*/

#define QQMLFILE_MAX_REDIRECT_RECURSION 16

QT_BEGIN_NAMESPACE

static char qrc_string[] = "qrc";
static char file_string[] = "file";

#if defined(Q_OS_ANDROID)
static char assets_string[] = "assets";
#endif

class QQmlFilePrivate;

#if QT_CONFIG(qml_network)
class QQmlFileNetworkReply : public QObject
{
Q_OBJECT
public:
    QQmlFileNetworkReply(QQmlEngine *, QQmlFilePrivate *, const QUrl &);
    ~QQmlFileNetworkReply();

signals:
    void finished();
    void downloadProgress(qint64, qint64);

public slots:
    void networkFinished();
    void networkDownloadProgress(qint64, qint64);

public:
    static int finishedIndex;
    static int downloadProgressIndex;
    static int networkFinishedIndex;
    static int networkDownloadProgressIndex;
    static int replyFinishedIndex;
    static int replyDownloadProgressIndex;

private:
    QQmlEngine *m_engine;
    QQmlFilePrivate *m_p;

    int m_redirectCount;
    QNetworkReply *m_reply;
};
#endif

class QQmlFilePrivate
{
public:
    QQmlFilePrivate();

    mutable QUrl url;
    mutable QString urlString;

    QByteArray data;

    enum Error {
        None, NotFound, CaseMismatch, Network
    };

    Error error;
    QString errorString;
#if QT_CONFIG(qml_network)
    QQmlFileNetworkReply *reply;
#endif
};

#if QT_CONFIG(qml_network)
int QQmlFileNetworkReply::finishedIndex = -1;
int QQmlFileNetworkReply::downloadProgressIndex = -1;
int QQmlFileNetworkReply::networkFinishedIndex = -1;
int QQmlFileNetworkReply::networkDownloadProgressIndex = -1;
int QQmlFileNetworkReply::replyFinishedIndex = -1;
int QQmlFileNetworkReply::replyDownloadProgressIndex = -1;

QQmlFileNetworkReply::QQmlFileNetworkReply(QQmlEngine *e, QQmlFilePrivate *p, const QUrl &url)
: m_engine(e), m_p(p), m_redirectCount(0), m_reply(0)
{
    if (finishedIndex == -1) {
        finishedIndex = QMetaMethod::fromSignal(&QQmlFileNetworkReply::finished).methodIndex();
        downloadProgressIndex = QMetaMethod::fromSignal(&QQmlFileNetworkReply::downloadProgress).methodIndex();
        const QMetaObject *smo = &staticMetaObject;
        networkFinishedIndex = smo->indexOfMethod("networkFinished()");
        networkDownloadProgressIndex = smo->indexOfMethod("networkDownloadProgress(qint64,qint64)");

        replyFinishedIndex = QMetaMethod::fromSignal(&QNetworkReply::finished).methodIndex();
        replyDownloadProgressIndex = QMetaMethod::fromSignal(&QNetworkReply::downloadProgress).methodIndex();
    }
    Q_ASSERT(finishedIndex != -1 && downloadProgressIndex != -1 &&
             networkFinishedIndex != -1 && networkDownloadProgressIndex != -1 &&
             replyFinishedIndex != -1 && replyDownloadProgressIndex != -1);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    m_reply = m_engine->networkAccessManager()->get(req);
    QMetaObject::connect(m_reply, replyFinishedIndex, this, networkFinishedIndex);
    QMetaObject::connect(m_reply, replyDownloadProgressIndex, this, networkDownloadProgressIndex);
}

QQmlFileNetworkReply::~QQmlFileNetworkReply()
{
    if (m_reply) {
        m_reply->disconnect();
        m_reply->deleteLater();
    }
}

void QQmlFileNetworkReply::networkFinished()
{
    ++m_redirectCount;
    if (m_redirectCount < QQMLFILE_MAX_REDIRECT_RECURSION) {
        QVariant redirect = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = m_reply->url().resolved(redirect.toUrl());

            QNetworkRequest req(url);
            req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

            m_reply->deleteLater();
            m_reply = m_engine->networkAccessManager()->get(req);

            QMetaObject::connect(m_reply, replyFinishedIndex,
                                 this, networkFinishedIndex);
            QMetaObject::connect(m_reply, replyDownloadProgressIndex,
                                 this, networkDownloadProgressIndex);

            return;
        }
    }

    if (m_reply->error()) {
        m_p->errorString = m_reply->errorString();
        m_p->error = QQmlFilePrivate::Network;
    } else {
        m_p->data = m_reply->readAll();
    }

    m_reply->deleteLater();
    m_reply = 0;

    m_p->reply = 0;
    emit finished();
    delete this;
}

void QQmlFileNetworkReply::networkDownloadProgress(qint64 a, qint64 b)
{
    emit downloadProgress(a, b);
}
#endif // qml_network

QQmlFilePrivate::QQmlFilePrivate()
: error(None)
#if QT_CONFIG(qml_network)
, reply(0)
#endif
{
}

QQmlFile::QQmlFile()
: d(new QQmlFilePrivate)
{
}

QQmlFile::QQmlFile(QQmlEngine *e, const QUrl &url)
: d(new QQmlFilePrivate)
{
    load(e, url);
}

QQmlFile::QQmlFile(QQmlEngine *e, const QString &url)
    : QQmlFile(e, QUrl(url))
{
}

QQmlFile::~QQmlFile()
{
#if QT_CONFIG(qml_network)
    delete d->reply;
#endif
    delete d;
    d = 0;
}

bool QQmlFile::isNull() const
{
    return status() == Null;
}

bool QQmlFile::isReady() const
{
    return status() == Ready;
}

bool QQmlFile::isError() const
{
    return status() == Error;
}

bool QQmlFile::isLoading() const
{
    return status() == Loading;
}

QUrl QQmlFile::url() const
{
    if (!d->urlString.isEmpty()) {
        d->url = QUrl(d->urlString);
        d->urlString = QString();
    }
    return d->url;
}

QQmlFile::Status QQmlFile::status() const
{
    if (d->url.isEmpty() && d->urlString.isEmpty())
        return Null;
#if QT_CONFIG(qml_network)
    else if (d->reply)
        return Loading;
#endif
    else if (d->error != QQmlFilePrivate::None)
        return Error;
    else
        return Ready;
}

QString QQmlFile::error() const
{
    switch (d->error) {
    default:
    case QQmlFilePrivate::None:
        return QString();
    case QQmlFilePrivate::NotFound:
        return QLatin1String("File not found");
    case QQmlFilePrivate::CaseMismatch:
        return QLatin1String("File name case mismatch");
    }
}

qint64 QQmlFile::size() const
{
    return d->data.size();
}

const char *QQmlFile::data() const
{
    return d->data.constData();
}

QByteArray QQmlFile::dataByteArray() const
{
    return d->data;
}

void QQmlFile::load(QQmlEngine *engine, const QUrl &url)
{
    Q_ASSERT(engine);

    clear();
    d->url = url;

    if (isLocalFile(url)) {
        QString lf = urlToLocalFileOrQrc(url);

        if (!QQml_isFileCaseCorrect(lf)) {
            d->error = QQmlFilePrivate::CaseMismatch;
            return;
        }

        QFile file(lf);
        if (file.open(QFile::ReadOnly)) {
            d->data = file.readAll();
        } else {
            d->error = QQmlFilePrivate::NotFound;
        }
    } else {
#if QT_CONFIG(qml_network)
        d->reply = new QQmlFileNetworkReply(engine, d, url);
#else
        d->error = QQmlFilePrivate::NotFound;
#endif
    }
}

void QQmlFile::load(QQmlEngine *engine, const QString &url)
{
    Q_ASSERT(engine);

    clear();

    d->urlString = url;

    if (isLocalFile(url)) {
        QString lf = urlToLocalFileOrQrc(url);

        if (!QQml_isFileCaseCorrect(lf)) {
            d->error = QQmlFilePrivate::CaseMismatch;
            return;
        }

        QFile file(lf);
        if (file.open(QFile::ReadOnly)) {
            d->data = file.readAll();
        } else {
            d->error = QQmlFilePrivate::NotFound;
        }
    } else {
#if QT_CONFIG(qml_network)
        QUrl qurl(url);
        d->url = qurl;
        d->urlString = QString();
        d->reply = new QQmlFileNetworkReply(engine, d, qurl);
#else
        d->error = QQmlFilePrivate::NotFound;
#endif
    }
}

void QQmlFile::clear()
{
    d->url = QUrl();
    d->urlString = QString();
    d->data = QByteArray();
    d->error = QQmlFilePrivate::None;
}

void QQmlFile::clear(QObject *)
{
    clear();
}

#if QT_CONFIG(qml_network)
bool QQmlFile::connectFinished(QObject *object, const char *method)
{
    if (!d || !d->reply) {
        qWarning("QQmlFile: connectFinished() called when not loading.");
        return false;
    }

    return QObject::connect(d->reply, SIGNAL(finished()),
                            object, method);
}

bool QQmlFile::connectFinished(QObject *object, int method)
{
    if (!d || !d->reply) {
        qWarning("QQmlFile: connectFinished() called when not loading.");
        return false;
    }

    return QMetaObject::connect(d->reply, QQmlFileNetworkReply::finishedIndex,
                                object, method);
}

bool QQmlFile::connectDownloadProgress(QObject *object, const char *method)
{
    if (!d || !d->reply) {
        qWarning("QQmlFile: connectDownloadProgress() called when not loading.");
        return false;
    }

    return QObject::connect(d->reply, SIGNAL(downloadProgress(qint64,qint64)),
                            object, method);
}

bool QQmlFile::connectDownloadProgress(QObject *object, int method)
{
    if (!d || !d->reply) {
        qWarning("QQmlFile: connectDownloadProgress() called when not loading.");
        return false;
    }

    return QMetaObject::connect(d->reply, QQmlFileNetworkReply::downloadProgressIndex,
                                object, method);
}
#endif

/*!
Returns true if QQmlFile will open \a url synchronously.

Synchronous urls have a qrc:/ or file:// scheme.

\note On Android, urls with assets:/ scheme are also considered synchronous.
*/
bool QQmlFile::isSynchronous(const QUrl &url)
{
    QString scheme = url.scheme();

    if ((scheme.length() == 4 && 0 == scheme.compare(QLatin1String(file_string), Qt::CaseInsensitive)) ||
        (scheme.length() == 3 && 0 == scheme.compare(QLatin1String(qrc_string), Qt::CaseInsensitive))) {
        return true;

#if defined(Q_OS_ANDROID)
    } else if (scheme.length() == 6 && 0 == scheme.compare(QLatin1String(assets_string), Qt::CaseInsensitive)) {
        return true;
#endif

    } else {
        return false;
    }
}

/*!
Returns true if QQmlFile will open \a url synchronously.

Synchronous urls have a qrc:/ or file:// scheme.

\note On Android, urls with assets:/ scheme are also considered synchronous.
*/
bool QQmlFile::isSynchronous(const QString &url)
{
    if (url.length() < 5 /* qrc:/ */)
        return false;

    QChar f = url[0];

    if (f == QLatin1Char('f') || f == QLatin1Char('F')) {

        return url.length() >= 7 /* file:// */ &&
               url.startsWith(QLatin1String(file_string), Qt::CaseInsensitive) &&
               url[4] == QLatin1Char(':') && url[5] == QLatin1Char('/') && url[6] == QLatin1Char('/');

    } else if (f == QLatin1Char('q') || f == QLatin1Char('Q')) {

        return url.length() >= 5 /* qrc:/ */ &&
               url.startsWith(QLatin1String(qrc_string), Qt::CaseInsensitive) &&
               url[3] == QLatin1Char(':') && url[4] == QLatin1Char('/');

    }

#if defined(Q_OS_ANDROID)
    else if (f == QLatin1Char('a') || f == QLatin1Char('A')) {
        return url.length() >= 8 /* assets:/ */ &&
               url.startsWith(QLatin1String(assets_string), Qt::CaseInsensitive) &&
               url[6] == QLatin1Char(':') && url[7] == QLatin1Char('/');

    }
#endif

    return false;
}

/*!
Returns true if \a url is a local file that can be opened with QFile.

Local file urls have either a qrc:/ or file:// scheme.

\note On Android, urls with assets:/ scheme are also considered local files.
*/
bool QQmlFile::isLocalFile(const QUrl &url)
{
    QString scheme = url.scheme();

    if ((scheme.length() == 4 && 0 == scheme.compare(QLatin1String(file_string), Qt::CaseInsensitive)) ||
        (scheme.length() == 3 && 0 == scheme.compare(QLatin1String(qrc_string), Qt::CaseInsensitive))) {
        return true;

#if defined(Q_OS_ANDROID)
    } else if (scheme.length() == 6 && 0 == scheme.compare(QLatin1String(assets_string), Qt::CaseInsensitive)) {
        return true;
#endif

    } else {
        return false;
    }
}

/*!
Returns true if \a url is a local file that can be opened with QFile.

Local file urls have either a qrc:/ or file:// scheme.

\note On Android, urls with assets:/ scheme are also considered local files.
*/
bool QQmlFile::isLocalFile(const QString &url)
{
    if (url.length() < 5 /* qrc:/ */)
        return false;

    QChar f = url[0];

    if (f == QLatin1Char('f') || f == QLatin1Char('F')) {

        return url.length() >= 7 /* file:// */ &&
               url.startsWith(QLatin1String(file_string), Qt::CaseInsensitive) &&
               url[4] == QLatin1Char(':') && url[5] == QLatin1Char('/') && url[6] == QLatin1Char('/');

    } else if (f == QLatin1Char('q') || f == QLatin1Char('Q')) {

        return url.length() >= 5 /* qrc:/ */ &&
               url.startsWith(QLatin1String(qrc_string), Qt::CaseInsensitive) &&
               url[3] == QLatin1Char(':') && url[4] == QLatin1Char('/');

    }
#if defined(Q_OS_ANDROID)
    else if (f == QLatin1Char('a') || f == QLatin1Char('A')) {
        return url.length() >= 8 /* assets:/ */ &&
               url.startsWith(QLatin1String(assets_string), Qt::CaseInsensitive) &&
               url[6] == QLatin1Char(':') && url[7] == QLatin1Char('/');

    }
#endif

    return false;
}

/*!
If \a url is a local file returns a path suitable for passing to QFile.  Otherwise returns an
empty string.
*/
QString QQmlFile::urlToLocalFileOrQrc(const QUrl& url)
{
    if (url.scheme().compare(QLatin1String("qrc"), Qt::CaseInsensitive) == 0) {
        if (url.authority().isEmpty())
            return QLatin1Char(':') + url.path();
        return QString();
    }

#if defined(Q_OS_ANDROID)
    else if (url.scheme().compare(QLatin1String("assets"), Qt::CaseInsensitive) == 0) {
        if (url.authority().isEmpty())
            return url.toString();
        return QString();
    }
#endif

    return url.toLocalFile();
}

static QString toLocalFile(const QString &url)
{
    const QUrl file(url);
    if (!file.isLocalFile())
        return QString();

    //XXX TODO: handle windows hostnames: "//servername/path/to/file.txt"

    return file.toLocalFile();
}

/*!
If \a url is a local file returns a path suitable for passing to QFile.  Otherwise returns an
empty string.
*/
QString QQmlFile::urlToLocalFileOrQrc(const QString& url)
{
    if (url.startsWith(QLatin1String("qrc:"), Qt::CaseInsensitive)) {
        if (url.length() > 4)
            return QLatin1Char(':') + url.midRef(4);
        return QString();
    }

#if defined(Q_OS_ANDROID)
    else if (url.startsWith(QLatin1String("assets:"), Qt::CaseInsensitive)) {
        return url;
    }
#endif

    return toLocalFile(url);
}

QT_END_NAMESPACE

#include "qqmlfile.moc"
