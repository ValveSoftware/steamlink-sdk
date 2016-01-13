/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickpixmapcache_p.h"
#include <qqmlnetworkaccessmanagerfactory.h>
#include <qquickimageprovider.h>

#include <qqmlengine.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlengine_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qimage_p.h>
#include <qpa/qplatformintegration.h>

#include <QtQuick/private/qsgtexture_p.h>
#include <QtQuick/private/qsgcontext_p.h>

#include <QCoreApplication>
#include <QImageReader>
#include <QHash>
#include <QNetworkReply>
#include <QPixmapCache>
#include <QFile>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QBuffer>
#include <QWaitCondition>
#include <QtCore/qdebug.h>
#include <private/qobject_p.h>
#include <QSslError>
#include <QQmlFile>
#include <QMetaMethod>

#include <private/qquickprofiler_p.h>

#define IMAGEREQUEST_MAX_REQUEST_COUNT       8
#define IMAGEREQUEST_MAX_REDIRECT_RECURSION 16
#define CACHE_EXPIRE_TIME 30
#define CACHE_REMOVAL_FRACTION 4

#define PIXMAP_PROFILE(Code) Q_QUICK_PROFILE(QQuickProfiler::ProfilePixmapCache, Code)

QT_BEGIN_NAMESPACE


#ifndef QT_NO_DEBUG
static bool qsg_leak_check = !qgetenv("QML_LEAK_CHECK").isEmpty();
#endif

// The cache limit describes the maximum "junk" in the cache.
static int cache_limit = 2048 * 1024; // 2048 KB cache limit for embedded in qpixmapcache.cpp

static inline QString imageProviderId(const QUrl &url)
{
    return url.host();
}

static inline QString imageId(const QUrl &url)
{
    return url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
}

QQuickDefaultTextureFactory::QQuickDefaultTextureFactory(const QImage &image)
{
    if (image.format() == QImage::Format_ARGB32_Premultiplied
            || image.format() == QImage::Format_RGB32) {
        im = image;
    } else {
        im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
}


QSGTexture *QQuickDefaultTextureFactory::createTexture(QQuickWindow *) const
{
    QSGPlainTexture *t = new QSGPlainTexture();
    t->setImage(im);
    return t;
}

static QQuickTextureFactory *textureFactoryForImage(const QImage &image)
{
    if (image.isNull())
        return 0;
    QQuickTextureFactory *texture = QSGContext::createTextureFactoryFromImage(image);
    if (texture)
        return texture;
    return new QQuickDefaultTextureFactory(image);
}

class QQuickPixmapReader;
class QQuickPixmapData;
class QQuickPixmapReply : public QObject
{
    Q_OBJECT
public:
    enum ReadError { NoError, Loading, Decoding };

    QQuickPixmapReply(QQuickPixmapData *);
    ~QQuickPixmapReply();

    QQuickPixmapData *data;
    QQmlEngine *engineForReader; // always access reader inside readerMutex
    QSize requestSize;
    QUrl url;

    bool loading;
    int redirectCount;

    class Event : public QEvent {
    public:
        Event(ReadError, const QString &, const QSize &, QQuickTextureFactory *factory);

        ReadError error;
        QString errorString;
        QSize implicitSize;
        QQuickTextureFactory *textureFactory;
    };
    void postReply(ReadError, const QString &, const QSize &, QQuickTextureFactory *factory);


Q_SIGNALS:
    void finished();
    void downloadProgress(qint64, qint64);

protected:
    bool event(QEvent *event);

private:
    Q_DISABLE_COPY(QQuickPixmapReply)

public:
    static int finishedIndex;
    static int downloadProgressIndex;
};

class QQuickPixmapReaderThreadObject : public QObject {
    Q_OBJECT
public:
    QQuickPixmapReaderThreadObject(QQuickPixmapReader *);
    void processJobs();
    virtual bool event(QEvent *e);
private slots:
    void networkRequestDone();
private:
    QQuickPixmapReader *reader;
};

class QQuickPixmapData;
class QQuickPixmapReader : public QThread
{
    Q_OBJECT
public:
    QQuickPixmapReader(QQmlEngine *eng);
    ~QQuickPixmapReader();

    QQuickPixmapReply *getImage(QQuickPixmapData *);
    void cancel(QQuickPixmapReply *rep);

    static QQuickPixmapReader *instance(QQmlEngine *engine);
    static QQuickPixmapReader *existingInstance(QQmlEngine *engine);

protected:
    void run();

private:
    friend class QQuickPixmapReaderThreadObject;
    void processJobs();
    void processJob(QQuickPixmapReply *, const QUrl &, const QSize &);
    void networkRequestDone(QNetworkReply *);

    QList<QQuickPixmapReply*> jobs;
    QList<QQuickPixmapReply*> cancelled;
    QQmlEngine *engine;
    QObject *eventLoopQuitHack;

    QMutex mutex;
    QQuickPixmapReaderThreadObject *threadObject;
    QWaitCondition waitCondition;

    QNetworkAccessManager *networkAccessManager();
    QNetworkAccessManager *accessManager;

    QHash<QNetworkReply*,QQuickPixmapReply*> replies;

    static int replyDownloadProgress;
    static int replyFinished;
    static int downloadProgress;
    static int threadNetworkRequestDone;
    static QHash<QQmlEngine *,QQuickPixmapReader*> readers;
public:
    static QMutex readerMutex;
};

class QQuickPixmapData
{
public:
    QQuickPixmapData(QQuickPixmap *pixmap, const QUrl &u, const QSize &s, const QString &e)
    : refCount(1), inCache(false), pixmapStatus(QQuickPixmap::Error),
      url(u), errorString(e), requestSize(s), textureFactory(0), reply(0), prevUnreferenced(0),
      prevUnreferencedPtr(0), nextUnreferenced(0)
    {
        declarativePixmaps.insert(pixmap);
    }

    QQuickPixmapData(QQuickPixmap *pixmap, const QUrl &u, const QSize &r)
    : refCount(1), inCache(false), pixmapStatus(QQuickPixmap::Loading),
      url(u), requestSize(r), textureFactory(0), reply(0), prevUnreferenced(0), prevUnreferencedPtr(0),
      nextUnreferenced(0)
    {
        declarativePixmaps.insert(pixmap);
    }

    QQuickPixmapData(QQuickPixmap *pixmap, const QUrl &u, QQuickTextureFactory *texture, const QSize &s, const QSize &r)
    : refCount(1), inCache(false), pixmapStatus(QQuickPixmap::Ready),
      url(u), implicitSize(s), requestSize(r), textureFactory(texture), reply(0), prevUnreferenced(0),
      prevUnreferencedPtr(0), nextUnreferenced(0)
    {
        declarativePixmaps.insert(pixmap);
    }

    QQuickPixmapData(QQuickPixmap *pixmap, QQuickTextureFactory *texture)
    : refCount(1), inCache(false), pixmapStatus(QQuickPixmap::Ready),
      textureFactory(texture), reply(0), prevUnreferenced(0),
      prevUnreferencedPtr(0), nextUnreferenced(0)
    {
        if (texture)
            requestSize = implicitSize = texture->textureSize();
        declarativePixmaps.insert(pixmap);
    }

    ~QQuickPixmapData()
    {
        while (!declarativePixmaps.isEmpty()) {
            QQuickPixmap *referencer = declarativePixmaps.first();
            declarativePixmaps.remove(referencer);
            referencer->d = 0;
        }
        delete textureFactory;
    }

    int cost() const;
    void addref();
    void release();
    void addToCache();
    void removeFromCache();

    uint refCount;

    bool inCache:1;

    QQuickPixmap::Status pixmapStatus;
    QUrl url;
    QString errorString;
    QSize implicitSize;
    QSize requestSize;

    QQuickTextureFactory *textureFactory;

    QIntrusiveList<QQuickPixmap, &QQuickPixmap::dataListNode> declarativePixmaps;
    QQuickPixmapReply *reply;

    QQuickPixmapData *prevUnreferenced;
    QQuickPixmapData**prevUnreferencedPtr;
    QQuickPixmapData *nextUnreferenced;
};

int QQuickPixmapReply::finishedIndex = -1;
int QQuickPixmapReply::downloadProgressIndex = -1;

// XXX
QHash<QQmlEngine *,QQuickPixmapReader*> QQuickPixmapReader::readers;
QMutex QQuickPixmapReader::readerMutex;

int QQuickPixmapReader::replyDownloadProgress = -1;
int QQuickPixmapReader::replyFinished = -1;
int QQuickPixmapReader::downloadProgress = -1;
int QQuickPixmapReader::threadNetworkRequestDone = -1;


void QQuickPixmapReply::postReply(ReadError error, const QString &errorString,
                                        const QSize &implicitSize, QQuickTextureFactory *factory)
{
    loading = false;
    QCoreApplication::postEvent(this, new Event(error, errorString, implicitSize, factory));
}

QQuickPixmapReply::Event::Event(ReadError e, const QString &s, const QSize &iSize, QQuickTextureFactory *factory)
    : QEvent(QEvent::User), error(e), errorString(s), implicitSize(iSize), textureFactory(factory)
{
}

QNetworkAccessManager *QQuickPixmapReader::networkAccessManager()
{
    if (!accessManager) {
        Q_ASSERT(threadObject);
        accessManager = QQmlEnginePrivate::get(engine)->createNetworkAccessManager(threadObject);
    }
    return accessManager;
}

static void maybeRemoveAlpha(QImage *image)
{
    // If the image
    if (image->hasAlphaChannel() && image->data_ptr()
            && !image->data_ptr()->checkForAlphaPixels()) {
        switch (image->format()) {
        case QImage::Format_RGBA8888:
        case QImage::Format_RGBA8888_Premultiplied:
            *image = image->convertToFormat(QImage::Format_RGBX8888);
            break;
        case QImage::Format_A2BGR30_Premultiplied:
            *image = image->convertToFormat(QImage::Format_BGR30);
            break;
        case QImage::Format_A2RGB30_Premultiplied:
            *image = image->convertToFormat(QImage::Format_RGB30);
            break;
        default:
            *image = image->convertToFormat(QImage::Format_RGB32);
            break;
        }
    }
}

static bool readImage(const QUrl& url, QIODevice *dev, QImage *image, QString *errorString, QSize *impsize,
                      const QSize &requestSize)
{
    QImageReader imgio(dev);

    const bool force_scale = imgio.format() == "svg" || imgio.format() == "svgz";

    if (requestSize.width() > 0 || requestSize.height() > 0) {
        QSize s = imgio.size();
        qreal ratio = 0.0;
        if (requestSize.width() && (force_scale || requestSize.width() < s.width())) {
            ratio = qreal(requestSize.width())/s.width();
        }
        if (requestSize.height() && (force_scale || requestSize.height() < s.height())) {
            qreal hr = qreal(requestSize.height())/s.height();
            if (ratio == 0.0 || hr < ratio)
                ratio = hr;
        }
        if (ratio > 0.0) {
            s.setHeight(qRound(s.height() * ratio));
            s.setWidth(qRound(s.width() * ratio));
            imgio.setScaledSize(s);
        }
    }

    if (impsize)
        *impsize = imgio.size();

    if (imgio.read(image)) {
        maybeRemoveAlpha(image);
        if (impsize && impsize->width() < 0)
            *impsize = image->size();
        return true;
    } else {
        if (errorString)
            *errorString = QQuickPixmap::tr("Error decoding: %1: %2").arg(url.toString())
                                .arg(imgio.errorString());
        return false;
    }
}

QQuickPixmapReader::QQuickPixmapReader(QQmlEngine *eng)
: QThread(eng), engine(eng), threadObject(0), accessManager(0)
{
    eventLoopQuitHack = new QObject;
    eventLoopQuitHack->moveToThread(this);
    connect(eventLoopQuitHack, SIGNAL(destroyed(QObject*)), SLOT(quit()), Qt::DirectConnection);
    start(QThread::LowestPriority);
}

QQuickPixmapReader::~QQuickPixmapReader()
{
    readerMutex.lock();
    readers.remove(engine);
    readerMutex.unlock();

    mutex.lock();
    // manually cancel all outstanding jobs.
    foreach (QQuickPixmapReply *reply, jobs) {
        if (reply->data && reply->data->reply == reply)
            reply->data->reply = 0;
        delete reply;
    }
    jobs.clear();
    QList<QQuickPixmapReply*> activeJobs = replies.values();
    foreach (QQuickPixmapReply *reply, activeJobs) {
        if (reply->loading) {
            cancelled.append(reply);
            reply->data = 0;
        }
    }
    if (threadObject) threadObject->processJobs();
    mutex.unlock();

    eventLoopQuitHack->deleteLater();
    wait();
}

void QQuickPixmapReader::networkRequestDone(QNetworkReply *reply)
{
    QQuickPixmapReply *job = replies.take(reply);

    if (job) {
        job->redirectCount++;
        if (job->redirectCount < IMAGEREQUEST_MAX_REDIRECT_RECURSION) {
            QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            if (redirect.isValid()) {
                QUrl url = reply->url().resolved(redirect.toUrl());
                QNetworkRequest req(url);
                req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

                reply->deleteLater();
                reply = networkAccessManager()->get(req);

                QMetaObject::connect(reply, replyDownloadProgress, job, downloadProgress);
                QMetaObject::connect(reply, replyFinished, threadObject, threadNetworkRequestDone);

                replies.insert(reply, job);
                return;
            }
        }

        QImage image;
        QQuickPixmapReply::ReadError error = QQuickPixmapReply::NoError;
        QString errorString;
        QSize readSize;
        if (reply->error()) {
            error = QQuickPixmapReply::Loading;
            errorString = reply->errorString();
        } else {
            QByteArray all = reply->readAll();
            QBuffer buff(&all);
            buff.open(QIODevice::ReadOnly);
            if (!readImage(reply->url(), &buff, &image, &errorString, &readSize, job->requestSize))
                error = QQuickPixmapReply::Decoding;
       }
        // send completion event to the QQuickPixmapReply
        mutex.lock();
        if (!cancelled.contains(job))
            job->postReply(error, errorString, readSize, textureFactoryForImage(image));
        mutex.unlock();
    }
    reply->deleteLater();

    // kick off event loop again incase we have dropped below max request count
    threadObject->processJobs();
}

QQuickPixmapReaderThreadObject::QQuickPixmapReaderThreadObject(QQuickPixmapReader *i)
: reader(i)
{
}

void QQuickPixmapReaderThreadObject::processJobs()
{
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

bool QQuickPixmapReaderThreadObject::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        reader->processJobs();
        return true;
    } else {
        return QObject::event(e);
    }
}

void QQuickPixmapReaderThreadObject::networkRequestDone()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    reader->networkRequestDone(reply);
}

void QQuickPixmapReader::processJobs()
{
    QMutexLocker locker(&mutex);

    while (true) {
        if (cancelled.isEmpty() && (jobs.isEmpty() || replies.count() >= IMAGEREQUEST_MAX_REQUEST_COUNT))
            return; // Nothing else to do

        // Clean cancelled jobs
        if (cancelled.count()) {
            for (int i = 0; i < cancelled.count(); ++i) {
                QQuickPixmapReply *job = cancelled.at(i);
                QNetworkReply *reply = replies.key(job, 0);
                if (reply && reply->isRunning()) {
                    // cancel any jobs already started
                    replies.remove(reply);
                    reply->close();
                }
                PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingError>(job->url));
                // deleteLater, since not owned by this thread
                job->deleteLater();
            }
            cancelled.clear();
        }

        if (!jobs.isEmpty() && replies.count() < IMAGEREQUEST_MAX_REQUEST_COUNT) {
            QQuickPixmapReply *runningJob = jobs.takeLast();
            runningJob->loading = true;

            QUrl url = runningJob->url;
            PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingStarted>(url));

            QSize requestSize = runningJob->requestSize;
            locker.unlock();
            processJob(runningJob, url, requestSize);
            locker.relock();
        }
    }
}

void QQuickPixmapReader::processJob(QQuickPixmapReply *runningJob, const QUrl &url,
                                          const QSize &requestSize)
{
    // fetch
    if (url.scheme() == QLatin1String("image")) {
        // Use QQuickImageProvider
        QSize readSize;

        QQuickImageProvider::ImageType imageType = QQuickImageProvider::Invalid;
        QQuickImageProvider *provider = static_cast<QQuickImageProvider *>(engine->imageProvider(imageProviderId(url)));
        if (provider)
            imageType = provider->imageType();

        if (imageType == QQuickImageProvider::Invalid) {
            QQuickPixmapReply::ReadError errorCode = QQuickPixmapReply::Loading;
            QString errorStr = QQuickPixmap::tr("Invalid image provider: %1").arg(url.toString());
            QImage image;
            mutex.lock();
            if (!cancelled.contains(runningJob))
                runningJob->postReply(errorCode, errorStr, readSize, textureFactoryForImage(image));
            mutex.unlock();
        } else if (imageType == QQuickImageProvider::Image) {
            QImage image = provider->requestImage(imageId(url), &readSize, requestSize);
            QQuickPixmapReply::ReadError errorCode = QQuickPixmapReply::NoError;
            QString errorStr;
            if (image.isNull()) {
                errorCode = QQuickPixmapReply::Loading;
                errorStr = QQuickPixmap::tr("Failed to get image from provider: %1").arg(url.toString());
            }
            mutex.lock();
            if (!cancelled.contains(runningJob))
                runningJob->postReply(errorCode, errorStr, readSize, textureFactoryForImage(image));
            mutex.unlock();
        } else if (imageType == QQuickImageProvider::Pixmap) {
            const QPixmap pixmap = provider->requestPixmap(imageId(url), &readSize, requestSize);
            QQuickPixmapReply::ReadError errorCode = QQuickPixmapReply::NoError;
            QString errorStr;
            if (pixmap.isNull()) {
                errorCode = QQuickPixmapReply::Loading;
                errorStr = QQuickPixmap::tr("Failed to get image from provider: %1").arg(url.toString());
            }
            mutex.lock();
            if (!cancelled.contains(runningJob))
                runningJob->postReply(errorCode, errorStr, readSize, textureFactoryForImage(pixmap.toImage()));
            mutex.unlock();
        } else {
            QQuickTextureFactory *t = provider->requestTexture(imageId(url), &readSize, requestSize);
            QQuickPixmapReply::ReadError errorCode = QQuickPixmapReply::NoError;
            QString errorStr;
            if (!t) {
                errorCode = QQuickPixmapReply::Loading;
                errorStr = QQuickPixmap::tr("Failed to get texture from provider: %1").arg(url.toString());
            }
            mutex.lock();
            if (!cancelled.contains(runningJob))
                runningJob->postReply(errorCode, errorStr, readSize, t);
            else
                delete t;
            mutex.unlock();

        }

    } else {
        QString lf = QQmlFile::urlToLocalFileOrQrc(url);
        if (!lf.isEmpty()) {
            // Image is local - load/decode immediately
            QImage image;
            QQuickPixmapReply::ReadError errorCode = QQuickPixmapReply::NoError;
            QString errorStr;
            QFile f(lf);
            QSize readSize;
            if (f.open(QIODevice::ReadOnly)) {
                if (!readImage(url, &f, &image, &errorStr, &readSize, requestSize))
                    errorCode = QQuickPixmapReply::Loading;
            } else {
                errorStr = QQuickPixmap::tr("Cannot open: %1").arg(url.toString());
                errorCode = QQuickPixmapReply::Loading;
            }
            mutex.lock();
            if (!cancelled.contains(runningJob))
                runningJob->postReply(errorCode, errorStr, readSize, textureFactoryForImage(image));
            mutex.unlock();
        } else {
            // Network resource
            QNetworkRequest req(url);
            req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
            QNetworkReply *reply = networkAccessManager()->get(req);

            QMetaObject::connect(reply, replyDownloadProgress, runningJob, downloadProgress);
            QMetaObject::connect(reply, replyFinished, threadObject, threadNetworkRequestDone);

            replies.insert(reply, runningJob);
        }
    }
}

QQuickPixmapReader *QQuickPixmapReader::instance(QQmlEngine *engine)
{
    // XXX NOTE: must be called within readerMutex locking.
    QQuickPixmapReader *reader = readers.value(engine);
    if (!reader) {
        reader = new QQuickPixmapReader(engine);
        readers.insert(engine, reader);
    }

    return reader;
}

QQuickPixmapReader *QQuickPixmapReader::existingInstance(QQmlEngine *engine)
{
    // XXX NOTE: must be called within readerMutex locking.
    return readers.value(engine, 0);
}

QQuickPixmapReply *QQuickPixmapReader::getImage(QQuickPixmapData *data)
{
    mutex.lock();
    QQuickPixmapReply *reply = new QQuickPixmapReply(data);
    reply->engineForReader = engine;
    jobs.append(reply);
    // XXX
    if (threadObject) threadObject->processJobs();
    mutex.unlock();
    return reply;
}

void QQuickPixmapReader::cancel(QQuickPixmapReply *reply)
{
    mutex.lock();
    if (reply->loading) {
        cancelled.append(reply);
        reply->data = 0;
        // XXX
        if (threadObject) threadObject->processJobs();
    } else {
        // If loading was started (reply removed from jobs) but the reply was never processed
        // (otherwise it would have deleted itself) we need to profile an error.
        if (jobs.removeAll(reply) == 0) {
            PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingError>(reply->url));
        }
        delete reply;
    }
    mutex.unlock();
}

void QQuickPixmapReader::run()
{
    if (replyDownloadProgress == -1) {
        replyDownloadProgress = QMetaMethod::fromSignal(&QNetworkReply::downloadProgress).methodIndex();
        replyFinished = QMetaMethod::fromSignal(&QNetworkReply::finished).methodIndex();
        downloadProgress = QMetaMethod::fromSignal(&QQuickPixmapReply::downloadProgress).methodIndex();
        const QMetaObject *ir = &QQuickPixmapReaderThreadObject::staticMetaObject;
        threadNetworkRequestDone = ir->indexOfSlot("networkRequestDone()");
    }

    mutex.lock();
    threadObject = new QQuickPixmapReaderThreadObject(this);
    mutex.unlock();

    processJobs();
    exec();

    delete threadObject;
    threadObject = 0;
}

class QQuickPixmapKey
{
public:
    const QUrl *url;
    const QSize *size;
};

inline bool operator==(const QQuickPixmapKey &lhs, const QQuickPixmapKey &rhs)
{
    return *lhs.size == *rhs.size && *lhs.url == *rhs.url;
}

inline uint qHash(const QQuickPixmapKey &key)
{
    return qHash(*key.url) ^ key.size->width() ^ key.size->height();
}

class QSGContext;

class QQuickPixmapStore : public QObject
{
    Q_OBJECT
public:
    QQuickPixmapStore();
    ~QQuickPixmapStore();

    void unreferencePixmap(QQuickPixmapData *);
    void referencePixmap(QQuickPixmapData *);

    void purgeCache();

protected:
    virtual void timerEvent(QTimerEvent *);

public:
    QHash<QQuickPixmapKey, QQuickPixmapData *> m_cache;

private:
    void shrinkCache(int remove);

    QQuickPixmapData *m_unreferencedPixmaps;
    QQuickPixmapData *m_lastUnreferencedPixmap;

    int m_unreferencedCost;
    int m_timerId;
    bool m_destroying;
};
Q_GLOBAL_STATIC(QQuickPixmapStore, pixmapStore);


QQuickPixmapStore::QQuickPixmapStore()
    : m_unreferencedPixmaps(0), m_lastUnreferencedPixmap(0), m_unreferencedCost(0), m_timerId(-1), m_destroying(false)
{
}

QQuickPixmapStore::~QQuickPixmapStore()
{
    m_destroying = true;

#ifndef QT_NO_DEBUG
    int leakedPixmaps = 0;
#endif
    QList<QQuickPixmapData*> cachedData = m_cache.values();

    // Prevent unreferencePixmap() from assuming it needs to kick
    // off the cache expiry timer, as we're shrinking the cache
    // manually below after releasing all the pixmaps.
    m_timerId = -2;

    // unreference all (leaked) pixmaps
    foreach (QQuickPixmapData* pixmap, cachedData) {
        int currRefCount = pixmap->refCount;
        if (currRefCount) {
#ifndef QT_NO_DEBUG
            leakedPixmaps++;
#endif
            while (currRefCount > 0) {
                pixmap->release();
                currRefCount--;
            }
        }
    }

    // free all unreferenced pixmaps
    while (m_lastUnreferencedPixmap) {
        shrinkCache(20);
    }

#ifndef QT_NO_DEBUG
    if (leakedPixmaps && qsg_leak_check)
        qDebug("Number of leaked pixmaps: %i", leakedPixmaps);
#endif
}

void QQuickPixmapStore::unreferencePixmap(QQuickPixmapData *data)
{
    Q_ASSERT(data->prevUnreferenced == 0);
    Q_ASSERT(data->prevUnreferencedPtr == 0);
    Q_ASSERT(data->nextUnreferenced == 0);

    data->nextUnreferenced = m_unreferencedPixmaps;
    data->prevUnreferencedPtr = &m_unreferencedPixmaps;
    if (!m_destroying) // the texture factories may have been cleaned up already.
        m_unreferencedCost += data->cost();

    m_unreferencedPixmaps = data;
    if (m_unreferencedPixmaps->nextUnreferenced) {
        m_unreferencedPixmaps->nextUnreferenced->prevUnreferenced = m_unreferencedPixmaps;
        m_unreferencedPixmaps->nextUnreferenced->prevUnreferencedPtr = &m_unreferencedPixmaps->nextUnreferenced;
    }

    if (!m_lastUnreferencedPixmap)
        m_lastUnreferencedPixmap = data;

    shrinkCache(-1); // Shrink the cache in case it has become larger than cache_limit

    if (m_timerId == -1 && m_unreferencedPixmaps && !m_destroying)
        m_timerId = startTimer(CACHE_EXPIRE_TIME * 1000);
}

void QQuickPixmapStore::referencePixmap(QQuickPixmapData *data)
{
    Q_ASSERT(data->prevUnreferencedPtr);

    *data->prevUnreferencedPtr = data->nextUnreferenced;
    if (data->nextUnreferenced) {
        data->nextUnreferenced->prevUnreferencedPtr = data->prevUnreferencedPtr;
        data->nextUnreferenced->prevUnreferenced = data->prevUnreferenced;
    }
    if (m_lastUnreferencedPixmap == data)
        m_lastUnreferencedPixmap = data->prevUnreferenced;

    data->nextUnreferenced = 0;
    data->prevUnreferencedPtr = 0;
    data->prevUnreferenced = 0;

    m_unreferencedCost -= data->cost();
}

void QQuickPixmapStore::shrinkCache(int remove)
{
    while ((remove > 0 || m_unreferencedCost > cache_limit) && m_lastUnreferencedPixmap) {
        QQuickPixmapData *data = m_lastUnreferencedPixmap;
        Q_ASSERT(data->nextUnreferenced == 0);

        *data->prevUnreferencedPtr = 0;
        m_lastUnreferencedPixmap = data->prevUnreferenced;
        data->prevUnreferencedPtr = 0;
        data->prevUnreferenced = 0;

        if (!m_destroying) {
            remove -= data->cost();
            m_unreferencedCost -= data->cost();
        }
        data->removeFromCache();
        delete data;
    }
}

void QQuickPixmapStore::timerEvent(QTimerEvent *)
{
    int removalCost = m_unreferencedCost / CACHE_REMOVAL_FRACTION;

    shrinkCache(removalCost);

    if (m_unreferencedPixmaps == 0) {
        killTimer(m_timerId);
        m_timerId = -1;
    }
}

void QQuickPixmapStore::purgeCache()
{
    shrinkCache(m_unreferencedCost);
}

void QQuickPixmap::purgeCache()
{
    pixmapStore()->purgeCache();
}

QQuickPixmapReply::QQuickPixmapReply(QQuickPixmapData *d)
: data(d), engineForReader(0), requestSize(d->requestSize), url(d->url), loading(false), redirectCount(0)
{
    if (finishedIndex == -1) {
        finishedIndex = QMetaMethod::fromSignal(&QQuickPixmapReply::finished).methodIndex();
        downloadProgressIndex = QMetaMethod::fromSignal(&QQuickPixmapReply::downloadProgress).methodIndex();
    }
}

QQuickPixmapReply::~QQuickPixmapReply()
{
    // note: this->data->reply must be set to zero if this->data->reply == this
    // but it must be done within mutex locking, to be guaranteed to be safe.
}

bool QQuickPixmapReply::event(QEvent *event)
{
    if (event->type() == QEvent::User) {

        if (data) {
            Event *de = static_cast<Event *>(event);
            data->pixmapStatus = (de->error == NoError) ? QQuickPixmap::Ready : QQuickPixmap::Error;
            if (data->pixmapStatus == QQuickPixmap::Ready) {
                data->textureFactory = de->textureFactory;
                data->implicitSize = de->implicitSize;
                PIXMAP_PROFILE(pixmapLoadingFinished(data->url,
                        data->textureFactory != 0 && data->textureFactory->textureSize().isValid() ?
                        data->textureFactory->textureSize() :
                        (data->requestSize.isValid() ? data->requestSize : data->implicitSize)));
            } else {
                PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingError>(data->url));
                data->errorString = de->errorString;
                data->removeFromCache(); // We don't continue to cache error'd pixmaps
            }

            data->reply = 0;
            emit finished();
        } else {
            PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingError>(url));
        }

        delete this;
        return true;
    } else {
        return QObject::event(event);
    }
}

int QQuickPixmapData::cost() const
{
    if (textureFactory)
        return textureFactory->textureByteCount();
    return 0;
}

void QQuickPixmapData::addref()
{
    ++refCount;
    PIXMAP_PROFILE(pixmapCountChanged<QQuickProfiler::PixmapReferenceCountChanged>(url, refCount));
    if (prevUnreferencedPtr)
        pixmapStore()->referencePixmap(this);
}

void QQuickPixmapData::release()
{
    Q_ASSERT(refCount > 0);
    --refCount;
    PIXMAP_PROFILE(pixmapCountChanged<QQuickProfiler::PixmapReferenceCountChanged>(url, refCount));
    if (refCount == 0) {
        if (reply) {
            QQuickPixmapReply *cancelReply = reply;
            reply->data = 0;
            reply = 0;
            QQuickPixmapReader::readerMutex.lock();
            QQuickPixmapReader *reader = QQuickPixmapReader::existingInstance(cancelReply->engineForReader);
            if (reader)
                reader->cancel(cancelReply);
            QQuickPixmapReader::readerMutex.unlock();
        }

        if (pixmapStatus == QQuickPixmap::Ready) {
            if (inCache)
                pixmapStore()->unreferencePixmap(this);
            else
                delete this;
        } else {
            removeFromCache();
            delete this;
        }
    }
}

void QQuickPixmapData::addToCache()
{
    if (!inCache) {
        QQuickPixmapKey key = { &url, &requestSize };
        pixmapStore()->m_cache.insert(key, this);
        inCache = true;
        PIXMAP_PROFILE(pixmapCountChanged<QQuickProfiler::PixmapCacheCountChanged>(
                url, pixmapStore()->m_cache.count()));
    }
}

void QQuickPixmapData::removeFromCache()
{
    if (inCache) {
        QQuickPixmapKey key = { &url, &requestSize };
        pixmapStore()->m_cache.remove(key);
        inCache = false;
        PIXMAP_PROFILE(pixmapCountChanged<QQuickProfiler::PixmapCacheCountChanged>(
                url, pixmapStore()->m_cache.count()));
    }
}

static QQuickPixmapData* createPixmapDataSync(QQuickPixmap *declarativePixmap, QQmlEngine *engine, const QUrl &url, const QSize &requestSize, bool *ok)
{
    if (url.scheme() == QLatin1String("image")) {
        QSize readSize;

        QQuickImageProvider::ImageType imageType = QQuickImageProvider::Invalid;
        QQuickImageProvider *provider = static_cast<QQuickImageProvider *>(engine->imageProvider(imageProviderId(url)));
        if (provider)
            imageType = provider->imageType();

        switch (imageType) {
            case QQuickImageProvider::Invalid:
                return new QQuickPixmapData(declarativePixmap, url, requestSize,
                    QQuickPixmap::tr("Invalid image provider: %1").arg(url.toString()));
            case QQuickImageProvider::Texture:
            {
                QQuickTextureFactory *texture = provider->requestTexture(imageId(url), &readSize, requestSize);
                if (texture) {
                    *ok = true;
                    return new QQuickPixmapData(declarativePixmap, url, texture, readSize, requestSize);
                }
            }

            case QQuickImageProvider::Image:
            {
                QImage image = provider->requestImage(imageId(url), &readSize, requestSize);
                if (!image.isNull()) {
                    *ok = true;
                    return new QQuickPixmapData(declarativePixmap, url, textureFactoryForImage(image), readSize, requestSize);
                }
            }
            case QQuickImageProvider::Pixmap:
            {
                QPixmap pixmap = provider->requestPixmap(imageId(url), &readSize, requestSize);
                if (!pixmap.isNull()) {
                    *ok = true;
                    return new QQuickPixmapData(declarativePixmap, url, textureFactoryForImage(pixmap.toImage()), readSize, requestSize);
                }
            }
        }

        // provider has bad image type, or provider returned null image
        return new QQuickPixmapData(declarativePixmap, url, requestSize,
            QQuickPixmap::tr("Failed to get image from provider: %1").arg(url.toString()));
    }

    QString localFile = QQmlFile::urlToLocalFileOrQrc(url);
    if (localFile.isEmpty())
        return 0;

    QFile f(localFile);
    QSize readSize;
    QString errorString;

    if (f.open(QIODevice::ReadOnly)) {
        QImage image;

        if (readImage(url, &f, &image, &errorString, &readSize, requestSize)) {
            *ok = true;
            return new QQuickPixmapData(declarativePixmap, url, textureFactoryForImage(image), readSize, requestSize);
        }
        errorString = QQuickPixmap::tr("Invalid image data: %1").arg(url.toString());

    } else {
        errorString = QQuickPixmap::tr("Cannot open: %1").arg(url.toString());
    }
    return new QQuickPixmapData(declarativePixmap, url, requestSize, errorString);
}


struct QQuickPixmapNull {
    QUrl url;
    QSize size;
};
Q_GLOBAL_STATIC(QQuickPixmapNull, nullPixmap);

QQuickPixmap::QQuickPixmap()
: d(0)
{
}

QQuickPixmap::QQuickPixmap(QQmlEngine *engine, const QUrl &url)
: d(0)
{
    load(engine, url);
}

QQuickPixmap::QQuickPixmap(QQmlEngine *engine, const QUrl &url, const QSize &size)
: d(0)
{
    load(engine, url, size);
}

QQuickPixmap::QQuickPixmap(const QUrl &url, const QImage &image)
{
    d = new QQuickPixmapData(this, url, new QQuickDefaultTextureFactory(image), image.size(), QSize());
    d->addToCache();
}

QQuickPixmap::~QQuickPixmap()
{
    if (d) {
        d->declarativePixmaps.remove(this);
        d->release();
        d = 0;
    }
}

bool QQuickPixmap::isNull() const
{
    return d == 0;
}

bool QQuickPixmap::isReady() const
{
    return status() == Ready;
}

bool QQuickPixmap::isError() const
{
    return status() == Error;
}

bool QQuickPixmap::isLoading() const
{
    return status() == Loading;
}

QString QQuickPixmap::error() const
{
    if (d)
        return d->errorString;
    else
        return QString();
}

QQuickPixmap::Status QQuickPixmap::status() const
{
    if (d)
        return d->pixmapStatus;
    else
        return Null;
}

const QUrl &QQuickPixmap::url() const
{
    if (d)
        return d->url;
    else
        return nullPixmap()->url;
}

const QSize &QQuickPixmap::implicitSize() const
{
    if (d)
        return d->implicitSize;
    else
        return nullPixmap()->size;
}

const QSize &QQuickPixmap::requestSize() const
{
    if (d)
        return d->requestSize;
    else
        return nullPixmap()->size;
}

QQuickTextureFactory *QQuickPixmap::textureFactory() const
{
    if (d)
        return d->textureFactory;

    return 0;
}

QImage QQuickPixmap::image() const
{
    if (d && d->textureFactory)
        return d->textureFactory->image();
    return QImage();
}

void QQuickPixmap::setImage(const QImage &p)
{
    clear();

    if (!p.isNull())
        d = new QQuickPixmapData(this, textureFactoryForImage(p));
}

void QQuickPixmap::setPixmap(const QQuickPixmap &other)
{
    clear();

    if (other.d) {
        d = other.d;
        d->addref();
        d->declarativePixmaps.insert(this);
    }
}

int QQuickPixmap::width() const
{
    if (d && d->textureFactory)
        return d->textureFactory->textureSize().width();
    else
        return 0;
}

int QQuickPixmap::height() const
{
    if (d && d->textureFactory)
        return d->textureFactory->textureSize().height();
    else
        return 0;
}

QRect QQuickPixmap::rect() const
{
    if (d && d->textureFactory)
        return QRect(QPoint(), d->textureFactory->textureSize());
    else
        return QRect();
}

void QQuickPixmap::load(QQmlEngine *engine, const QUrl &url)
{
    load(engine, url, QSize(), QQuickPixmap::Cache);
}

void QQuickPixmap::load(QQmlEngine *engine, const QUrl &url, QQuickPixmap::Options options)
{
    load(engine, url, QSize(), options);
}

void QQuickPixmap::load(QQmlEngine *engine, const QUrl &url, const QSize &size)
{
    load(engine, url, size, QQuickPixmap::Cache);
}

void QQuickPixmap::load(QQmlEngine *engine, const QUrl &url, const QSize &requestSize, QQuickPixmap::Options options)
{
    if (d) {
        d->declarativePixmaps.remove(this);
        d->release();
        d = 0;
    }

    QQuickPixmapKey key = { &url, &requestSize };
    QQuickPixmapStore *store = pixmapStore();

    QHash<QQuickPixmapKey, QQuickPixmapData *>::Iterator iter = store->m_cache.end();

    // If Cache is disabled, the pixmap will always be loaded, even if there is an existing
    // cached version.
    if (options & QQuickPixmap::Cache)
        iter = store->m_cache.find(key);

    if (iter == store->m_cache.end()) {
        if (url.scheme() == QLatin1String("image")) {
            if (QQuickImageProvider *provider = static_cast<QQuickImageProvider *>(engine->imageProvider(imageProviderId(url)))) {
                const bool threadedPixmaps = QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ThreadedPixmaps);
                if (!threadedPixmaps && provider->imageType() == QQuickImageProvider::Pixmap) {
                    // pixmaps can only be loaded synchronously
                    options &= ~QQuickPixmap::Asynchronous;
                } else if (provider->flags() & QQuickImageProvider::ForceAsynchronousImageLoading) {
                    options |= QQuickPixmap::Asynchronous;
                }
            }
        }

        if (!(options & QQuickPixmap::Asynchronous)) {
            bool ok = false;
            PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingStarted>(url));
            d = createPixmapDataSync(this, engine, url, requestSize, &ok);
            if (ok) {
                PIXMAP_PROFILE(pixmapLoadingFinished(url, QSize(width(), height())));
                if (options & QQuickPixmap::Cache)
                    d->addToCache();
                return;
            }
            if (d) { // loadable, but encountered error while loading
                PIXMAP_PROFILE(pixmapStateChanged<QQuickProfiler::PixmapLoadingError>(url));
                return;
            }
        }

        if (!engine)
            return;

        d = new QQuickPixmapData(this, url, requestSize);
        if (options & QQuickPixmap::Cache)
            d->addToCache();

        QQuickPixmapReader::readerMutex.lock();
        d->reply = QQuickPixmapReader::instance(engine)->getImage(d);
        QQuickPixmapReader::readerMutex.unlock();
    } else {
        d = *iter;
        d->addref();
        d->declarativePixmaps.insert(this);
    }
}

void QQuickPixmap::clear()
{
    if (d) {
        d->declarativePixmaps.remove(this);
        d->release();
        d = 0;
    }
}

void QQuickPixmap::clear(QObject *obj)
{
    if (d) {
        if (d->reply)
            QObject::disconnect(d->reply, 0, obj, 0);
        d->declarativePixmaps.remove(this);
        d->release();
        d = 0;
    }
}

bool QQuickPixmap::isCached(const QUrl &url, const QSize &requestSize)
{
    QQuickPixmapKey key = { &url, &requestSize };
    QQuickPixmapStore *store = pixmapStore();

    return store->m_cache.contains(key);
}

bool QQuickPixmap::connectFinished(QObject *object, const char *method)
{
    if (!d || !d->reply) {
        qWarning("QQuickPixmap: connectFinished() called when not loading.");
        return false;
    }

    return QObject::connect(d->reply, SIGNAL(finished()), object, method);
}

bool QQuickPixmap::connectFinished(QObject *object, int method)
{
    if (!d || !d->reply) {
        qWarning("QQuickPixmap: connectFinished() called when not loading.");
        return false;
    }

    return QMetaObject::connect(d->reply, QQuickPixmapReply::finishedIndex, object, method);
}

bool QQuickPixmap::connectDownloadProgress(QObject *object, const char *method)
{
    if (!d || !d->reply) {
        qWarning("QQuickPixmap: connectDownloadProgress() called when not loading.");
        return false;
    }

    return QObject::connect(d->reply, SIGNAL(downloadProgress(qint64,qint64)), object, method);
}

bool QQuickPixmap::connectDownloadProgress(QObject *object, int method)
{
    if (!d || !d->reply) {
        qWarning("QQuickPixmap: connectDownloadProgress() called when not loading.");
        return false;
    }

    return QMetaObject::connect(d->reply, QQuickPixmapReply::downloadProgressIndex, object, method);
}

QT_END_NAMESPACE

#include <qquickpixmapcache.moc>
