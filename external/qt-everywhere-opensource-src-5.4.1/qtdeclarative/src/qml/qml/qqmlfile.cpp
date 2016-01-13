/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlfile.h"

#include <QtCore/qurl.h>
#include <QtCore/qobject.h>
#include <QtCore/qmetaobject.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>

/*!
\class QQmlFile
\brief The QQmlFile class gives access to local and remote files.

\internal

Supports file://, qrc:/, bundle:// uris and whatever QNetworkAccessManager supports.
*/

#define QQMLFILE_MAX_REDIRECT_RECURSION 16

QT_BEGIN_NAMESPACE

static char qrc_string[] = "qrc";
static char file_string[] = "file";
static char bundle_string[] = "bundle";

#if defined(Q_OS_ANDROID)
static char assets_string[] = "assets";
#endif

class QQmlFilePrivate;
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

class QQmlFilePrivate
{
public:
    QQmlFilePrivate();

    mutable QUrl url;
    mutable QString urlString;

    QQmlBundleData *bundle;
    const QQmlBundle::FileEntry *file;

    QByteArray data;

    enum Error {
        None, NotFound, CaseMismatch, Network
    };

    Error error;
    QString errorString;

    QQmlFileNetworkReply *reply;
};

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

QQmlFilePrivate::QQmlFilePrivate()
: bundle(0), file(0), error(None), reply(0)
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
: d(new QQmlFilePrivate)
{
    load(e, url);
}

QQmlFile::~QQmlFile()
{
    if (d->reply)
        delete d->reply;
    if (d->bundle)
        d->bundle->release();

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
    else if (d->reply)
        return Loading;
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
    if (d->file) return d->file->fileSize();
    else return d->data.size();
}

const char *QQmlFile::data() const
{
    if (d->file) return d->file->contents();
    else return d->data.constData();
}

QByteArray QQmlFile::dataByteArray() const
{
    if (d->file) return QByteArray(d->file->contents(), d->file->fileSize());
    else return d->data;
}

QByteArray QQmlFile::metaData(const QString &name) const
{
    if (d->file) {
        Q_ASSERT(d->bundle);
        const QQmlBundle::FileEntry *meta = d->bundle->link(d->file, name);
        if (meta)
            return QByteArray::fromRawData(meta->contents(), meta->fileSize());
    }
    return QByteArray();
}

void QQmlFile::load(QQmlEngine *engine, const QUrl &url)
{
    Q_ASSERT(engine);

    clear();
    d->url = url;

    if (isBundle(url)) {
        // Bundle
        QQmlEnginePrivate *p = QQmlEnginePrivate::get(engine);
        QQmlBundleData *bundle = p->typeLoader.getBundle(url.host());

        d->error = QQmlFilePrivate::NotFound;

        if (bundle) {
            QString filename = url.path().mid(1);
            const QQmlBundle::FileEntry *entry = bundle->find(filename);
            if (entry) {
                d->file = entry;
                d->bundle = bundle;
                d->bundle->addref();
                d->error = QQmlFilePrivate::None;
            }
            bundle->release();
        }
    } else if (isLocalFile(url)) {
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
        d->reply = new QQmlFileNetworkReply(engine, d, url);
    }
}

void QQmlFile::load(QQmlEngine *engine, const QString &url)
{
    Q_ASSERT(engine);

    clear();

    d->urlString = url;

    if (isBundle(url)) {
        // Bundle
        QQmlEnginePrivate *p = QQmlEnginePrivate::get(engine);

        d->error = QQmlFilePrivate::NotFound;

        int index = url.indexOf(QLatin1Char('/'), 9);
        if (index == -1)
            return;

        QStringRef identifier(&url, 9, index - 9);

        QQmlBundleData *bundle = p->typeLoader.getBundle(identifier);

        d->error = QQmlFilePrivate::NotFound;

        if (bundle) {
            QString filename = url.mid(index);
            const QQmlBundle::FileEntry *entry = bundle->find(filename);
            if (entry) {
                d->data = QByteArray(entry->contents(), entry->fileSize());
                d->error = QQmlFilePrivate::None;
            }
            bundle->release();
        }

    } else if (isLocalFile(url)) {
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
        QUrl qurl(url);
        d->url = qurl;
        d->urlString = QString();
        d->reply = new QQmlFileNetworkReply(engine, d, qurl);
    }
}

void QQmlFile::clear()
{
    d->url = QUrl();
    d->urlString = QString();
    d->data = QByteArray();
    if (d->bundle) d->bundle->release();
    d->bundle = 0;
    d->file = 0;
    d->error = QQmlFilePrivate::None;
}

void QQmlFile::clear(QObject *)
{
    clear();
}

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

/*!
Returns true if QQmlFile will open \a url synchronously.

Synchronous urls have a qrc:/, file://, or bundle:// scheme.

\note On Android, urls with assets:/ scheme are also considered synchronous.
*/
bool QQmlFile::isSynchronous(const QUrl &url)
{
    QString scheme = url.scheme();

    if ((scheme.length() == 4 && 0 == scheme.compare(QLatin1String(file_string), Qt::CaseInsensitive)) ||
        (scheme.length() == 6 && 0 == scheme.compare(QLatin1String(bundle_string), Qt::CaseInsensitive)) ||
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

Synchronous urls have a qrc:/, file://, or bundle:// scheme.

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

    } else if (f == QLatin1Char('b') || f == QLatin1Char('B')) {

        return url.length() >= 9 /* bundle:// */ &&
               url.startsWith(QLatin1String(bundle_string), Qt::CaseInsensitive) &&
               url[6] == QLatin1Char(':') && url[7] == QLatin1Char('/') && url[8] == QLatin1Char('/');

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
Returns true if \a url is a bundle.

Bundle urls have a bundle:// scheme.
*/
bool QQmlFile::isBundle(const QString &url)
{
    return url.length() >= 9 && url.startsWith(QLatin1String(bundle_string), Qt::CaseInsensitive) &&
           url[6] == QLatin1Char(':') && url[7] == QLatin1Char('/') && url[8] == QLatin1Char('/');
}

/*!
Returns true if \a url is a bundle.

Bundle urls have a bundle:// scheme.
*/
bool QQmlFile::isBundle(const QUrl &url)
{
    QString scheme = url.scheme();

    return scheme.length() == 6 && 0 == scheme.compare(QLatin1String(bundle_string), Qt::CaseInsensitive);
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
            return QLatin1Char(':') + url.mid(4);
        return QString();
    }

#if defined(Q_OS_ANDROID)
    else if (url.startsWith(QLatin1String("assets:"), Qt::CaseInsensitive)) {
        return url;
    }
#endif

    return toLocalFile(url);
}

bool QQmlFile::bundleDirectoryExists(const QString &dir, QQmlEngine *e)
{
    if (!isBundle(dir))
        return false;

    int index = dir.indexOf(QLatin1Char('/'), 9);

    if (index == -1 && dir.length() > 9) // We accept "bundle://<blah>" with no extra path
        index = dir.length();

    if (index == -1)
        return false;

    QStringRef identifier(&dir, 9, index - 9);

    QQmlBundleData *bundle = QQmlEnginePrivate::get(e)->typeLoader.getBundle(identifier);

    if (bundle) {
        int lastIndex = dir.lastIndexOf(QLatin1Char('/'));

        if (lastIndex <= index) {
            bundle->release();
            return true;
        }

        QStringRef d(&dir, index + 1, lastIndex - index);

        QList<const QQmlBundle::FileEntry *> entries = bundle->files();

        for (int ii = 0; ii < entries.count(); ++ii) {
            QString name = entries.at(ii)->fileName();
            if (name.startsWith(d)) {
                bundle->release();
                return true;
            }
        }

        bundle->release();
    }

    return false;
}

bool QQmlFile::bundleDirectoryExists(const QUrl &url, QQmlEngine *e)
{
    if (!isBundle(url))
        return false;

    QQmlBundleData *bundle = QQmlEnginePrivate::get(e)->typeLoader.getBundle(url.host());

    if (bundle) {
        QString path = url.path();

        int lastIndex = path.lastIndexOf(QLatin1Char('/'));

        if (lastIndex == -1) {
            bundle->release();
            return true;
        }

        QStringRef d(&path, 0, lastIndex);

        QList<const QQmlBundle::FileEntry *> entries = bundle->files();

        for (int ii = 0; ii < entries.count(); ++ii) {
            QString name = entries.at(ii)->fileName();
            if (name.startsWith(d)) {
                bundle->release();
                return true;
            }
        }

        bundle->release();
    }

    return false;
}

bool QQmlFile::bundleFileExists(const QString &file, QQmlEngine *e)
{
    if (!isBundle(file))
        return false;

    int index = file.indexOf(QLatin1Char('/'), 9);

    if (index == -1)
        return false;

    QStringRef identifier(&file, 9, index - 9);
    QStringRef path(&file, index + 1, file.length() - index - 1);

    QQmlBundleData *bundle = QQmlEnginePrivate::get(e)->typeLoader.getBundle(identifier);

    if (bundle) {
        const QQmlBundle::FileEntry *entry = bundle->find(path.constData(), path.length());
        bundle->release();

        return entry != 0;
    }

    return false;
}

bool QQmlFile::bundleFileExists(const QUrl &, QQmlEngine *)
{
    qFatal("Not implemented");
    return true;
}

/*!
Returns the file name for the bundle file referenced by \a url or an
empty string if \a url isn't a bundle url.
*/
QString QQmlFile::bundleFileName(const QString &url, QQmlEngine *e)
{
    if (!isBundle(url))
        return QString();

    int index = url.indexOf(QLatin1Char('/'), 9);

    if (index == -1)
        index = url.length();

    QStringRef identifier(&url, 9, index - 9);

    QQmlBundleData *bundle = QQmlEnginePrivate::get(e)->typeLoader.getBundle(identifier);

    if (bundle) {
        QString rv = bundle->fileName;
        bundle->release();
        return rv;
    }

    return QString();
}

/*!
Returns the file name for the bundle file referenced by \a url or an
empty string if \a url isn't a bundle url.
*/
QString QQmlFile::bundleFileName(const QUrl &url, QQmlEngine *e)
{
    if (!isBundle(url))
        return QString();

    QQmlBundleData *bundle = QQmlEnginePrivate::get(e)->typeLoader.getBundle(url.host());

    if (bundle) {
        QString rv = bundle->fileName;
        bundle->release();
        return rv;
    }

    return QString();
}

QT_END_NAMESPACE

#include "qqmlfile.moc"
