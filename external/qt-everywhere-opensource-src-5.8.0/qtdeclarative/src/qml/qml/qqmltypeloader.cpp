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

#include "qqmltypeloader_p.h"
#include "qqmlabstracturlinterceptor.h"
#include "qqmlcontextwrapper_p.h"
#include "qqmlexpression_p.h"

#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlthread_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlprofiler_p.h>
#include <private/qqmlmemoryprofiler_p.h>
#include <private/qqmltypecompiler_p.h>
#include <private/qqmlpropertyvalidator_p.h>
#include <private/qqmlpropertycachecreator_p.h>
#include <private/qdeferredcleanup_p.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtQml/qqmlfile.h>
#include <QtCore/qdiriterator.h>
#include <QtQml/qqmlcomponent.h>
#include <QtCore/qwaitcondition.h>
#include <QtCore/qloggingcategory.h>
#include <QtQml/qqmlextensioninterface.h>

#include <functional>

#if defined (Q_OS_UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined (QT_LINUXBASE)
// LSB doesn't declare NAME_MAX. Use SYMLINK_MAX instead, which seems to
// always be identical to NAME_MAX
#ifndef NAME_MAX
#  define NAME_MAX _POSIX_SYMLINK_MAX
#endif

// LSB has a broken version of qOffsetOf that can't be used at compile time
// https://lsbbugs.linuxfoundation.org/show_bug.cgi?id=3462
#undef qOffsetOf
#define qOffsetOf(TYPE, MEMBER) __builtin_qOffsetOf (TYPE, MEMBER)
#endif

// #define DATABLOB_DEBUG

#ifdef DATABLOB_DEBUG

#define ASSERT_MAINTHREAD() do { if (m_thread->isThisThread()) qFatal("QQmlTypeLoader: Caller not in main thread"); } while (false)
#define ASSERT_LOADTHREAD() do { if (!m_thread->isThisThread()) qFatal("QQmlTypeLoader: Caller not in load thread"); } while (false)
#define ASSERT_CALLBACK() do { if (!m_typeLoader || !m_typeLoader->m_thread->isThisThread()) qFatal("QQmlDataBlob: An API call was made outside a callback"); } while (false)

#else

#define ASSERT_MAINTHREAD()
#define ASSERT_LOADTHREAD()
#define ASSERT_CALLBACK()

#endif

DEFINE_BOOL_CONFIG_OPTION(dumpErrors, QML_DUMP_ERRORS);
DEFINE_BOOL_CONFIG_OPTION(disableDiskCache, QML_DISABLE_DISK_CACHE);
DEFINE_BOOL_CONFIG_OPTION(forceDiskCache, QML_FORCE_DISK_CACHE);

Q_DECLARE_LOGGING_CATEGORY(DBG_DISK_CACHE)
Q_LOGGING_CATEGORY(DBG_DISK_CACHE, "qt.qml.diskcache")

QT_BEGIN_NAMESPACE

namespace {

    template<typename LockType>
    struct LockHolder
    {
        LockType& lock;
        LockHolder(LockType *l) : lock(*l) { lock.lock(); }
        ~LockHolder() { lock.unlock(); }
    };
}

#if QT_CONFIG(qml_network)
// This is a lame object that we need to ensure that slots connected to
// QNetworkReply get called in the correct thread (the loader thread).
// As QQmlTypeLoader lives in the main thread, and we can't use
// Qt::DirectConnection connections from a QNetworkReply (because then
// sender() wont work), we need to insert this object in the middle.
class QQmlTypeLoaderNetworkReplyProxy : public QObject
{
    Q_OBJECT
public:
    QQmlTypeLoaderNetworkReplyProxy(QQmlTypeLoader *l);

public slots:
    void finished();
    void downloadProgress(qint64, qint64);
    void manualFinished(QNetworkReply*);

private:
    QQmlTypeLoader *l;
};
#endif // qml_network

class QQmlTypeLoaderThread : public QQmlThread
{
    typedef QQmlTypeLoaderThread This;

public:
    QQmlTypeLoaderThread(QQmlTypeLoader *loader);
#if QT_CONFIG(qml_network)
    QNetworkAccessManager *networkAccessManager() const;
    QQmlTypeLoaderNetworkReplyProxy *networkReplyProxy() const;
#endif // qml_network
    void load(QQmlDataBlob *b);
    void loadAsync(QQmlDataBlob *b);
    void loadWithStaticData(QQmlDataBlob *b, const QByteArray &);
    void loadWithStaticDataAsync(QQmlDataBlob *b, const QByteArray &);
    void loadWithCachedUnit(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit);
    void loadWithCachedUnitAsync(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit);
    void callCompleted(QQmlDataBlob *b);
    void callDownloadProgressChanged(QQmlDataBlob *b, qreal p);
    void initializeEngine(QQmlExtensionInterface *, const char *);

protected:
    virtual void shutdownThread();

private:
    void loadThread(QQmlDataBlob *b);
    void loadWithStaticDataThread(QQmlDataBlob *b, const QByteArray &);
    void loadWithCachedUnitThread(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit);
    void callCompletedMain(QQmlDataBlob *b);
    void callDownloadProgressChangedMain(QQmlDataBlob *b, qreal p);
    void initializeEngineMain(QQmlExtensionInterface *iface, const char *uri);

    QQmlTypeLoader *m_loader;
#if QT_CONFIG(qml_network)
    mutable QNetworkAccessManager *m_networkAccessManager;
    mutable QQmlTypeLoaderNetworkReplyProxy *m_networkReplyProxy;
#endif // qml_network
};

#if QT_CONFIG(qml_network)
QQmlTypeLoaderNetworkReplyProxy::QQmlTypeLoaderNetworkReplyProxy(QQmlTypeLoader *l)
: l(l)
{
}

void QQmlTypeLoaderNetworkReplyProxy::finished()
{
    Q_ASSERT(sender());
    Q_ASSERT(qobject_cast<QNetworkReply *>(sender()));
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    l->networkReplyFinished(reply);
}

void QQmlTypeLoaderNetworkReplyProxy::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_ASSERT(sender());
    Q_ASSERT(qobject_cast<QNetworkReply *>(sender()));
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    l->networkReplyProgress(reply, bytesReceived, bytesTotal);
}

// This function is for when you want to shortcut the signals and call directly
void QQmlTypeLoaderNetworkReplyProxy::manualFinished(QNetworkReply *reply)
{
    qint64 replySize = reply->size();
    l->networkReplyProgress(reply, replySize, replySize);
    l->networkReplyFinished(reply);
}
#endif // qml_network

/*!
\class QQmlDataBlob
\brief The QQmlDataBlob encapsulates a data request that can be issued to a QQmlTypeLoader.
\internal

QQmlDataBlob's are loaded by a QQmlTypeLoader.  The user creates the QQmlDataBlob
and then calls QQmlTypeLoader::load() or QQmlTypeLoader::loadWithStaticData() to load it.
The QQmlTypeLoader invokes callbacks on the QQmlDataBlob as data becomes available.
*/

/*!
\enum QQmlDataBlob::Status

This enum describes the status of the data blob.

\list
\li Null The blob has not yet been loaded by a QQmlTypeLoader
\li Loading The blob is loading network data.  The QQmlDataBlob::setData() callback has not yet been
invoked or has not yet returned.
\li WaitingForDependencies The blob is waiting for dependencies to be done before continueing.  This status
only occurs after the QQmlDataBlob::setData() callback has been made, and when the blob has outstanding
dependencies.
\li Complete The blob's data has been loaded and all dependencies are done.
\li Error An error has been set on this blob.
\endlist
*/

/*!
\enum QQmlDataBlob::Type

This enum describes the type of the data blob.

\list
\li QmlFile This is a QQmlTypeData
\li JavaScriptFile This is a QQmlScriptData
\li QmldirFile This is a QQmlQmldirData
\endlist
*/

/*!
Create a new QQmlDataBlob for \a url and of the provided \a type.
*/
QQmlDataBlob::QQmlDataBlob(const QUrl &url, Type type, QQmlTypeLoader *manager)
: m_typeLoader(manager), m_type(type), m_url(url), m_finalUrl(url), m_redirectCount(0),
  m_inCallback(false), m_isDone(false)
{
    //Set here because we need to get the engine from the manager
    if (m_typeLoader->engine() && m_typeLoader->engine()->urlInterceptor())
        m_url = m_typeLoader->engine()->urlInterceptor()->intercept(m_url,
                    (QQmlAbstractUrlInterceptor::DataType)m_type);
}

/*!  \internal */
QQmlDataBlob::~QQmlDataBlob()
{
    Q_ASSERT(m_waitingOnMe.isEmpty());

    cancelAllWaitingFor();
}

/*!
  Must be called before loading can occur.
*/
void QQmlDataBlob::startLoading()
{
    Q_ASSERT(status() == QQmlDataBlob::Null);
    m_data.setStatus(QQmlDataBlob::Loading);
}

/*!
Returns the type provided to the constructor.
*/
QQmlDataBlob::Type QQmlDataBlob::type() const
{
    return m_type;
}

/*!
Returns the blob's status.
*/
QQmlDataBlob::Status QQmlDataBlob::status() const
{
    return m_data.status();
}

/*!
Returns true if the status is Null.
*/
bool QQmlDataBlob::isNull() const
{
    return status() == Null;
}

/*!
Returns true if the status is Loading.
*/
bool QQmlDataBlob::isLoading() const
{
    return status() == Loading;
}

/*!
Returns true if the status is WaitingForDependencies.
*/
bool QQmlDataBlob::isWaiting() const
{
    return status() == WaitingForDependencies;
}

/*!
Returns true if the status is Complete.
*/
bool QQmlDataBlob::isComplete() const
{
    return status() == Complete;
}

/*!
Returns true if the status is Error.
*/
bool QQmlDataBlob::isError() const
{
    return status() == Error;
}

/*!
Returns true if the status is Complete or Error.
*/
bool QQmlDataBlob::isCompleteOrError() const
{
    Status s = status();
    return s == Error || s == Complete;
}

/*!
Returns the data download progress from 0 to 1.
*/
qreal QQmlDataBlob::progress() const
{
    quint8 p = m_data.progress();
    if (p == 0xFF) return 1.;
    else return qreal(p) / qreal(0xFF);
}

/*!
Returns the blob url passed to the constructor.  If a network redirect
happens while fetching the data, this url remains the same.

\sa finalUrl()
*/
QUrl QQmlDataBlob::url() const
{
    return m_url;
}

/*!
Returns the final url of the data.  Initially this is the same as
url(), but if a network redirect happens while fetching the data, this url
is updated to reflect the new location.

May only be called from the load thread, or after the blob isCompleteOrError().
*/
QUrl QQmlDataBlob::finalUrl() const
{
    Q_ASSERT(isCompleteOrError() || (m_typeLoader && m_typeLoader->m_thread->isThisThread()));
    return m_finalUrl;
}

/*!
Returns the finalUrl() as a string.
*/
QString QQmlDataBlob::finalUrlString() const
{
    Q_ASSERT(isCompleteOrError() || (m_typeLoader && m_typeLoader->m_thread->isThisThread()));
    if (m_finalUrlString.isEmpty())
        m_finalUrlString = m_finalUrl.toString();

    return m_finalUrlString;
}

/*!
Return the errors on this blob.

May only be called from the load thread, or after the blob isCompleteOrError().
*/
QList<QQmlError> QQmlDataBlob::errors() const
{
    Q_ASSERT(isCompleteOrError() || (m_typeLoader && m_typeLoader->m_thread->isThisThread()));
    return m_errors;
}

/*!
Mark this blob as having \a errors.

All outstanding dependencies will be cancelled.  Requests to add new dependencies
will be ignored.  Entry into the Error state is irreversable.

The setError() method may only be called from within a QQmlDataBlob callback.
*/
void QQmlDataBlob::setError(const QQmlError &errors)
{
    ASSERT_CALLBACK();

    QList<QQmlError> l;
    l << errors;
    setError(l);
}

/*!
\overload
*/
void QQmlDataBlob::setError(const QList<QQmlError> &errors)
{
    ASSERT_CALLBACK();

    Q_ASSERT(status() != Error);
    Q_ASSERT(m_errors.isEmpty());

    m_errors = errors; // Must be set before the m_data fence
    m_data.setStatus(Error);

    if (dumpErrors()) {
        qWarning().nospace() << "Errors for " << m_finalUrl.toString();
        for (int ii = 0; ii < errors.count(); ++ii)
            qWarning().nospace() << "    " << qPrintable(errors.at(ii).toString());
    }
    cancelAllWaitingFor();

    if (!m_inCallback)
        tryDone();
}

void QQmlDataBlob::setError(const QQmlCompileError &error)
{
    QQmlError e;
    e.setColumn(error.location.column);
    e.setLine(error.location.line);
    e.setDescription(error.description);
    e.setUrl(url());
    setError(e);
}

void QQmlDataBlob::setError(const QVector<QQmlCompileError> &errors)
{
    QList<QQmlError> finalErrors;
    finalErrors.reserve(errors.count());
    for (const QQmlCompileError &error: errors) {
        QQmlError e;
        e.setColumn(error.location.column);
        e.setLine(error.location.line);
        e.setDescription(error.description);
        e.setUrl(url());
        finalErrors << e;
    }
    setError(finalErrors);
}

void QQmlDataBlob::setError(const QString &description)
{
    QQmlError e;
    e.setDescription(description);
    e.setUrl(finalUrl());
    setError(e);
}

/*!
Wait for \a blob to become complete or to error.  If \a blob is already
complete or in error, or this blob is already complete, this has no effect.

The setError() method may only be called from within a QQmlDataBlob callback.
*/
void QQmlDataBlob::addDependency(QQmlDataBlob *blob)
{
    ASSERT_CALLBACK();

    Q_ASSERT(status() != Null);

    if (!blob ||
        blob->status() == Error || blob->status() == Complete ||
        status() == Error || status() == Complete || m_isDone ||
        m_waitingFor.contains(blob))
        return;

    blob->addref();

    m_data.setStatus(WaitingForDependencies);

    m_waitingFor.append(blob);
    blob->m_waitingOnMe.append(this);
}

/*!
\fn void QQmlDataBlob::dataReceived(const Data &data)

Invoked when data for the blob is received.  Implementors should use this callback
to determine a blob's dependencies.  Within this callback you may call setError()
or addDependency().
*/

/*!
Invoked once data has either been received or a network error occurred, and all
dependencies are complete.

You can set an error in this method, but you cannot add new dependencies.  Implementors
should use this callback to finalize processing of data.

The default implementation does nothing.

XXX Rename processData() or some such to avoid confusion between done() (processing thread)
and completed() (main thread)
*/
void QQmlDataBlob::done()
{
}

#if QT_CONFIG(qml_network)
/*!
Invoked if there is a network error while fetching this blob.

The default implementation sets an appropriate QQmlError.
*/
void QQmlDataBlob::networkError(QNetworkReply::NetworkError networkError)
{
    Q_UNUSED(networkError);

    QQmlError error;
    error.setUrl(m_finalUrl);

    const char *errorString = 0;
    switch (networkError) {
        default:
            errorString = "Network error";
            break;
        case QNetworkReply::ConnectionRefusedError:
            errorString = "Connection refused";
            break;
        case QNetworkReply::RemoteHostClosedError:
            errorString = "Remote host closed the connection";
            break;
        case QNetworkReply::HostNotFoundError:
            errorString = "Host not found";
            break;
        case QNetworkReply::TimeoutError:
            errorString = "Timeout";
            break;
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::ProxyAuthenticationRequiredError:
        case QNetworkReply::UnknownProxyError:
            errorString = "Proxy error";
            break;
        case QNetworkReply::ContentAccessDenied:
            errorString = "Access denied";
            break;
        case QNetworkReply::ContentNotFoundError:
            errorString = "File not found";
            break;
        case QNetworkReply::AuthenticationRequiredError:
            errorString = "Authentication required";
            break;
    };

    error.setDescription(QLatin1String(errorString));

    setError(error);
}
#endif // qml_network

/*!
Called if \a blob, which was previously waited for, has an error.

The default implementation does nothing.
*/
void QQmlDataBlob::dependencyError(QQmlDataBlob *blob)
{
    Q_UNUSED(blob);
}

/*!
Called if \a blob, which was previously waited for, has completed.

The default implementation does nothing.
*/
void QQmlDataBlob::dependencyComplete(QQmlDataBlob *blob)
{
    Q_UNUSED(blob);
}

/*!
Called when all blobs waited for have completed.  This occurs regardless of
whether they are in error, or complete state.

The default implementation does nothing.
*/
void QQmlDataBlob::allDependenciesDone()
{
}

/*!
Called when the download progress of this blob changes.  \a progress goes
from 0 to 1.

This callback is only invoked if an asynchronous load for this blob is
made.  An asynchronous load is one in which the Asynchronous mode is
specified explicitly, or one that is implicitly delayed due to a network
operation.

The default implementation does nothing.
*/
void QQmlDataBlob::downloadProgressChanged(qreal progress)
{
    Q_UNUSED(progress);
}

/*!
Invoked on the main thread sometime after done() was called on the load thread.

You cannot modify the blobs state at all in this callback and cannot depend on the
order or timeliness of these callbacks.  Implementors should use this callback to notify
dependencies on the main thread that the blob is done and not a lot else.

This callback is only invoked if an asynchronous load for this blob is
made.  An asynchronous load is one in which the Asynchronous mode is
specified explicitly, or one that is implicitly delayed due to a network
operation.

The default implementation does nothing.
*/
void QQmlDataBlob::completed()
{
}


void QQmlDataBlob::tryDone()
{
    if (status() != Loading && m_waitingFor.isEmpty() && !m_isDone) {
        m_isDone = true;
        addref();

#ifdef DATABLOB_DEBUG
        qWarning("QQmlDataBlob::done() %s", qPrintable(url().toString()));
#endif
        done();

        if (status() != Error)
            m_data.setStatus(Complete);

        notifyAllWaitingOnMe();

        // Locking is not required here, as anyone expecting callbacks must
        // already be protected against the blob being completed (as set above);
#ifdef DATABLOB_DEBUG
        qWarning("QQmlDataBlob: Dispatching completed");
#endif
        m_typeLoader->m_thread->callCompleted(this);

        release();
    }
}

void QQmlDataBlob::cancelAllWaitingFor()
{
    while (m_waitingFor.count()) {
        QQmlDataBlob *blob = m_waitingFor.takeLast();

        Q_ASSERT(blob->m_waitingOnMe.contains(this));

        blob->m_waitingOnMe.removeOne(this);

        blob->release();
    }
}

void QQmlDataBlob::notifyAllWaitingOnMe()
{
    while (m_waitingOnMe.count()) {
        QQmlDataBlob *blob = m_waitingOnMe.takeLast();

        Q_ASSERT(blob->m_waitingFor.contains(this));

        blob->notifyComplete(this);
    }
}

void QQmlDataBlob::notifyComplete(QQmlDataBlob *blob)
{
    Q_ASSERT(m_waitingFor.contains(blob));
    Q_ASSERT(blob->status() == Error || blob->status() == Complete);
    QQmlCompilingProfiler prof(QQmlEnginePrivate::get(typeLoader()->engine())->profiler,
                               blob);

    m_inCallback = true;

    m_waitingFor.removeOne(blob);

    if (blob->status() == Error) {
        dependencyError(blob);
    } else if (blob->status() == Complete) {
        dependencyComplete(blob);
    }

    blob->release();

    if (!isError() && m_waitingFor.isEmpty())
        allDependenciesDone();

    m_inCallback = false;

    tryDone();
}

#define TD_STATUS_MASK 0x0000FFFF
#define TD_STATUS_SHIFT 0
#define TD_PROGRESS_MASK 0x00FF0000
#define TD_PROGRESS_SHIFT 16
#define TD_ASYNC_MASK 0x80000000

QQmlDataBlob::ThreadData::ThreadData()
: _p(0)
{
}

QQmlDataBlob::Status QQmlDataBlob::ThreadData::status() const
{
    return QQmlDataBlob::Status((_p.load() & TD_STATUS_MASK) >> TD_STATUS_SHIFT);
}

void QQmlDataBlob::ThreadData::setStatus(QQmlDataBlob::Status status)
{
    while (true) {
        int d = _p.load();
        int nd = (d & ~TD_STATUS_MASK) | ((status << TD_STATUS_SHIFT) & TD_STATUS_MASK);
        if (d == nd || _p.testAndSetOrdered(d, nd)) return;
    }
}

bool QQmlDataBlob::ThreadData::isAsync() const
{
    return _p.load() & TD_ASYNC_MASK;
}

void QQmlDataBlob::ThreadData::setIsAsync(bool v)
{
    while (true) {
        int d = _p.load();
        int nd = (d & ~TD_ASYNC_MASK) | (v?TD_ASYNC_MASK:0);
        if (d == nd || _p.testAndSetOrdered(d, nd)) return;
    }
}

quint8 QQmlDataBlob::ThreadData::progress() const
{
    return quint8((_p.load() & TD_PROGRESS_MASK) >> TD_PROGRESS_SHIFT);
}

void QQmlDataBlob::ThreadData::setProgress(quint8 v)
{
    while (true) {
        int d = _p.load();
        int nd = (d & ~TD_PROGRESS_MASK) | ((v << TD_PROGRESS_SHIFT) & TD_PROGRESS_MASK);
        if (d == nd || _p.testAndSetOrdered(d, nd)) return;
    }
}

QQmlTypeLoaderThread::QQmlTypeLoaderThread(QQmlTypeLoader *loader)
: m_loader(loader)
#if QT_CONFIG(qml_network)
, m_networkAccessManager(0), m_networkReplyProxy(0)
#endif // qml_network
{
    // Do that after initializing all the members.
    startup();
}

#if QT_CONFIG(qml_network)
QNetworkAccessManager *QQmlTypeLoaderThread::networkAccessManager() const
{
    Q_ASSERT(isThisThread());
    if (!m_networkAccessManager) {
        m_networkAccessManager = QQmlEnginePrivate::get(m_loader->engine())->createNetworkAccessManager(0);
        m_networkReplyProxy = new QQmlTypeLoaderNetworkReplyProxy(m_loader);
    }

    return m_networkAccessManager;
}

QQmlTypeLoaderNetworkReplyProxy *QQmlTypeLoaderThread::networkReplyProxy() const
{
    Q_ASSERT(isThisThread());
    Q_ASSERT(m_networkReplyProxy); // Must call networkAccessManager() first
    return m_networkReplyProxy;
}
#endif // qml_network

void QQmlTypeLoaderThread::load(QQmlDataBlob *b)
{
    b->addref();
    callMethodInThread(&This::loadThread, b);
}

void QQmlTypeLoaderThread::loadAsync(QQmlDataBlob *b)
{
    b->addref();
    postMethodToThread(&This::loadThread, b);
}

void QQmlTypeLoaderThread::loadWithStaticData(QQmlDataBlob *b, const QByteArray &d)
{
    b->addref();
    callMethodInThread(&This::loadWithStaticDataThread, b, d);
}

void QQmlTypeLoaderThread::loadWithStaticDataAsync(QQmlDataBlob *b, const QByteArray &d)
{
    b->addref();
    postMethodToThread(&This::loadWithStaticDataThread, b, d);
}

void QQmlTypeLoaderThread::loadWithCachedUnit(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit)
{
    b->addref();
    callMethodInThread(&This::loadWithCachedUnitThread, b, unit);
}

void QQmlTypeLoaderThread::loadWithCachedUnitAsync(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit)
{
    b->addref();
    postMethodToThread(&This::loadWithCachedUnitThread, b, unit);
}

void QQmlTypeLoaderThread::callCompleted(QQmlDataBlob *b)
{
    b->addref();
    postMethodToMain(&This::callCompletedMain, b);
}

void QQmlTypeLoaderThread::callDownloadProgressChanged(QQmlDataBlob *b, qreal p)
{
    b->addref();
    postMethodToMain(&This::callDownloadProgressChangedMain, b, p);
}

void QQmlTypeLoaderThread::initializeEngine(QQmlExtensionInterface *iface,
                                                    const char *uri)
{
    callMethodInMain(&This::initializeEngineMain, iface, uri);
}

void QQmlTypeLoaderThread::shutdownThread()
{
#if QT_CONFIG(qml_network)
    delete m_networkAccessManager;
    m_networkAccessManager = 0;
    delete m_networkReplyProxy;
    m_networkReplyProxy = 0;
#endif // qml_network
}

void QQmlTypeLoaderThread::loadThread(QQmlDataBlob *b)
{
    m_loader->loadThread(b);
    b->release();
}

void QQmlTypeLoaderThread::loadWithStaticDataThread(QQmlDataBlob *b, const QByteArray &d)
{
    m_loader->loadWithStaticDataThread(b, d);
    b->release();
}

void QQmlTypeLoaderThread::loadWithCachedUnitThread(QQmlDataBlob *b, const QQmlPrivate::CachedQmlUnit *unit)
{
    m_loader->loadWithCachedUnitThread(b, unit);
    b->release();
}

void QQmlTypeLoaderThread::callCompletedMain(QQmlDataBlob *b)
{
    QML_MEMORY_SCOPE_URL(b->url());
#ifdef DATABLOB_DEBUG
    qWarning("QQmlTypeLoaderThread: %s completed() callback", qPrintable(b->url().toString()));
#endif
    b->completed();
    b->release();
}

void QQmlTypeLoaderThread::callDownloadProgressChangedMain(QQmlDataBlob *b, qreal p)
{
#ifdef DATABLOB_DEBUG
    qWarning("QQmlTypeLoaderThread: %s downloadProgressChanged(%f) callback",
             qPrintable(b->url().toString()), p);
#endif
    b->downloadProgressChanged(p);
    b->release();
}

void QQmlTypeLoaderThread::initializeEngineMain(QQmlExtensionInterface *iface,
                                                        const char *uri)
{
    Q_ASSERT(m_loader->engine()->thread() == QThread::currentThread());
    iface->initializeEngine(m_loader->engine(), uri);
}

/*!
\class QQmlTypeLoader
\brief The QQmlTypeLoader class abstracts loading files and their dependencies over the network.
\internal

The QQmlTypeLoader class is provided for the exclusive use of the QQmlTypeLoader class.

Clients create QQmlDataBlob instances and submit them to the QQmlTypeLoader class
through the QQmlTypeLoader::load() or QQmlTypeLoader::loadWithStaticData() methods.
The loader then fetches the data over the network or from the local file system in an efficient way.
QQmlDataBlob is an abstract class, so should always be specialized.

Once data is received, the QQmlDataBlob::dataReceived() method is invoked on the blob.  The
derived class should use this callback to process the received data.  Processing of the data can
result in an error being set (QQmlDataBlob::setError()), or one or more dependencies being
created (QQmlDataBlob::addDependency()).  Dependencies are other QQmlDataBlob's that
are required before processing can fully complete.

To complete processing, the QQmlDataBlob::done() callback is invoked.  done() is called when
one of these three preconditions are met.

\list 1
\li The QQmlDataBlob has no dependencies.
\li The QQmlDataBlob has an error set.
\li All the QQmlDataBlob's dependencies are themselves "done()".
\endlist

Thus QQmlDataBlob::done() will always eventually be called, even if the blob has an error set.
*/

void QQmlTypeLoader::invalidate()
{
    if (m_thread) {
        shutdownThread();
        delete m_thread;
        m_thread = 0;
    }

#if QT_CONFIG(qml_network)
    // Need to delete the network replies after
    // the loader thread is shutdown as it could be
    // getting new replies while we clear them
    for (NetworkReplies::Iterator iter = m_networkReplies.begin(); iter != m_networkReplies.end(); ++iter)
        (*iter)->release();
    m_networkReplies.clear();
#endif // qml_network
}

void QQmlTypeLoader::lock()
{
    m_thread->lock();
}

void QQmlTypeLoader::unlock()
{
    m_thread->unlock();
}

struct PlainLoader {
    void loadThread(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->loadThread(blob);
    }
    void load(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->load(blob);
    }
    void loadAsync(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->loadAsync(blob);
    }
};

struct StaticLoader {
    const QByteArray &data;
    StaticLoader(const QByteArray &data) : data(data) {}

    void loadThread(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->loadWithStaticDataThread(blob, data);
    }
    void load(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->loadWithStaticData(blob, data);
    }
    void loadAsync(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->loadWithStaticDataAsync(blob, data);
    }
};

struct CachedLoader {
    const QQmlPrivate::CachedQmlUnit *unit;
    CachedLoader(const QQmlPrivate::CachedQmlUnit *unit) :  unit(unit) {}

    void loadThread(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->loadWithCachedUnitThread(blob, unit);
    }
    void load(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->loadWithCachedUnit(blob, unit);
    }
    void loadAsync(QQmlTypeLoader *loader, QQmlDataBlob *blob) const
    {
        loader->m_thread->loadWithCachedUnitAsync(blob, unit);
    }
};

template<typename Loader>
void QQmlTypeLoader::doLoad(const Loader &loader, QQmlDataBlob *blob, Mode mode)
{
#ifdef DATABLOB_DEBUG
    qWarning("QQmlTypeLoader::doLoad(%s): %s thread", qPrintable(blob->m_url.toString()),
             m_thread->isThisThread()?"Compile":"Engine");
#endif
    blob->startLoading();

    if (m_thread->isThisThread()) {
        unlock();
        loader.loadThread(this, blob);
        lock();
    } else if (mode == Asynchronous) {
        blob->m_data.setIsAsync(true);
        unlock();
        loader.loadAsync(this, blob);
        lock();
    } else {
        unlock();
        loader.load(this, blob);
        lock();
        if (mode == PreferSynchronous) {
            if (!blob->isCompleteOrError())
                blob->m_data.setIsAsync(true);
        } else {
            Q_ASSERT(mode == Synchronous);
            while (!blob->isCompleteOrError()) {
                unlock();
                m_thread->waitForNextMessage();
                lock();
            }
        }
    }
}

/*!
Load the provided \a blob from the network or filesystem.

The loader must be locked.
*/
void QQmlTypeLoader::load(QQmlDataBlob *blob, Mode mode)
{
    doLoad(PlainLoader(), blob, mode);
}

/*!
Load the provided \a blob with \a data.  The blob's URL is not used by the data loader in this case.

The loader must be locked.
*/
void QQmlTypeLoader::loadWithStaticData(QQmlDataBlob *blob, const QByteArray &data, Mode mode)
{
    doLoad(StaticLoader(data), blob, mode);
}

void QQmlTypeLoader::loadWithCachedUnit(QQmlDataBlob *blob, const QQmlPrivate::CachedQmlUnit *unit, Mode mode)
{
    doLoad(CachedLoader(unit), blob, mode);
}

void QQmlTypeLoader::loadWithStaticDataThread(QQmlDataBlob *blob, const QByteArray &data)
{
    ASSERT_LOADTHREAD();

    setData(blob, data);
}

void QQmlTypeLoader::loadWithCachedUnitThread(QQmlDataBlob *blob, const QQmlPrivate::CachedQmlUnit *unit)
{
    ASSERT_LOADTHREAD();

    setCachedUnit(blob, unit);
}

void QQmlTypeLoader::loadThread(QQmlDataBlob *blob)
{
    ASSERT_LOADTHREAD();

    // Don't continue loading if we've been shutdown
    if (m_thread->isShutdown()) {
        QQmlError error;
        error.setDescription(QLatin1String("Interrupted by shutdown"));
        blob->setError(error);
        return;
    }

    if (blob->m_url.isEmpty()) {
        QQmlError error;
        error.setDescription(QLatin1String("Invalid null URL"));
        blob->setError(error);
        return;
    }

    QML_MEMORY_SCOPE_URL(blob->m_url);

    if (QQmlFile::isSynchronous(blob->m_url)) {
        const QString fileName = QQmlFile::urlToLocalFileOrQrc(blob->m_url);
        if (!QQml_isFileCaseCorrect(fileName)) {
            blob->setError(QLatin1String("File name case mismatch"));
            return;
        }

        blob->m_data.setProgress(0xFF);
        if (blob->m_data.isAsync())
            m_thread->callDownloadProgressChanged(blob, 1.);

        setData(blob, fileName);

    } else {
#if QT_CONFIG(qml_network)
        QNetworkReply *reply = m_thread->networkAccessManager()->get(QNetworkRequest(blob->m_url));
        QQmlTypeLoaderNetworkReplyProxy *nrp = m_thread->networkReplyProxy();
        blob->addref();
        m_networkReplies.insert(reply, blob);

        if (reply->isFinished()) {
            nrp->manualFinished(reply);
        } else {
            QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
                             nrp, SLOT(downloadProgress(qint64,qint64)));
            QObject::connect(reply, SIGNAL(finished()),
                             nrp, SLOT(finished()));
        }

#ifdef DATABLOB_DEBUG
        qWarning("QQmlDataBlob: requested %s", qPrintable(blob->url().toString()));
#endif // DATABLOB_DEBUG
#endif // qml_network
    }
}

#define DATALOADER_MAXIMUM_REDIRECT_RECURSION 16
#define TYPELOADER_MINIMUM_TRIM_THRESHOLD 64

#if QT_CONFIG(qml_network)
void QQmlTypeLoader::networkReplyFinished(QNetworkReply *reply)
{
    Q_ASSERT(m_thread->isThisThread());

    reply->deleteLater();

    QQmlDataBlob *blob = m_networkReplies.take(reply);

    Q_ASSERT(blob);

    blob->m_redirectCount++;

    if (blob->m_redirectCount < DATALOADER_MAXIMUM_REDIRECT_RECURSION) {
        QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = reply->url().resolved(redirect.toUrl());
            blob->m_finalUrl = url;

            QNetworkReply *reply = m_thread->networkAccessManager()->get(QNetworkRequest(url));
            QObject *nrp = m_thread->networkReplyProxy();
            QObject::connect(reply, SIGNAL(finished()), nrp, SLOT(finished()));
            m_networkReplies.insert(reply, blob);
#ifdef DATABLOB_DEBUG
            qWarning("QQmlDataBlob: redirected to %s", qPrintable(blob->m_finalUrl.toString()));
#endif
            return;
        }
    }

    if (reply->error()) {
        blob->networkError(reply->error());
    } else {
        QByteArray data = reply->readAll();
        setData(blob, data);
    }

    blob->release();
}

void QQmlTypeLoader::networkReplyProgress(QNetworkReply *reply,
                                                  qint64 bytesReceived, qint64 bytesTotal)
{
    Q_ASSERT(m_thread->isThisThread());

    QQmlDataBlob *blob = m_networkReplies.value(reply);

    Q_ASSERT(blob);

    if (bytesTotal != 0) {
        quint8 progress = 0xFF * (qreal(bytesReceived) / qreal(bytesTotal));
        blob->m_data.setProgress(progress);
        if (blob->m_data.isAsync())
            m_thread->callDownloadProgressChanged(blob, blob->m_data.progress());
    }
}
#endif // qml_network

/*!
Return the QQmlEngine associated with this loader
*/
QQmlEngine *QQmlTypeLoader::engine() const
{
    return m_engine;
}

/*!
Call the initializeEngine() method on \a iface.  Used by QQmlImportDatabase to ensure it
gets called in the correct thread.
*/
void QQmlTypeLoader::initializeEngine(QQmlExtensionInterface *iface,
                                              const char *uri)
{
    Q_ASSERT(m_thread->isThisThread() || engine()->thread() == QThread::currentThread());

    if (m_thread->isThisThread()) {
        m_thread->initializeEngine(iface, uri);
    } else {
        Q_ASSERT(engine()->thread() == QThread::currentThread());
        iface->initializeEngine(engine(), uri);
    }
}


void QQmlTypeLoader::setData(QQmlDataBlob *blob, const QByteArray &data)
{
    QML_MEMORY_SCOPE_URL(blob->url());
    QQmlDataBlob::Data d;
    d.d = &data;
    setData(blob, d);
}

void QQmlTypeLoader::setData(QQmlDataBlob *blob, const QString &fileName)
{
    QML_MEMORY_SCOPE_URL(blob->url());
    QQmlDataBlob::Data d;
    d.d = &fileName;
    setData(blob, d);
}

void QQmlTypeLoader::setData(QQmlDataBlob *blob, const QQmlDataBlob::Data &d)
{
    QML_MEMORY_SCOPE_URL(blob->url());
    QQmlCompilingProfiler prof(QQmlEnginePrivate::get(engine())->profiler, blob);

    blob->m_inCallback = true;

    blob->dataReceived(d);

    if (!blob->isError() && !blob->isWaiting())
        blob->allDependenciesDone();

    if (blob->status() != QQmlDataBlob::Error)
        blob->m_data.setStatus(QQmlDataBlob::WaitingForDependencies);

    blob->m_inCallback = false;

    blob->tryDone();
}

void QQmlTypeLoader::setCachedUnit(QQmlDataBlob *blob, const QQmlPrivate::CachedQmlUnit *unit)
{
    QML_MEMORY_SCOPE_URL(blob->url());
    QQmlCompilingProfiler prof(QQmlEnginePrivate::get(engine())->profiler, blob);

    blob->m_inCallback = true;

    blob->initializeFromCachedUnit(unit);

    if (!blob->isError() && !blob->isWaiting())
        blob->allDependenciesDone();

    if (blob->status() != QQmlDataBlob::Error)
        blob->m_data.setStatus(QQmlDataBlob::WaitingForDependencies);

    blob->m_inCallback = false;

    blob->tryDone();
}

void QQmlTypeLoader::shutdownThread()
{
    if (m_thread && !m_thread->isShutdown())
        m_thread->shutdown();
}

QQmlTypeLoader::Blob::Blob(const QUrl &url, QQmlDataBlob::Type type, QQmlTypeLoader *loader)
  : QQmlDataBlob(url, type, loader), m_importCache(loader)
{
}

QQmlTypeLoader::Blob::~Blob()
{
    for (int ii = 0; ii < m_qmldirs.count(); ++ii)
        m_qmldirs.at(ii)->release();
}

bool QQmlTypeLoader::Blob::fetchQmldir(const QUrl &url, const QV4::CompiledData::Import *import, int priority, QList<QQmlError> *errors)
{
    QQmlQmldirData *data = typeLoader()->getQmldir(url);

    data->setImport(import);
    data->setPriority(priority);

    if (data->status() == Error) {
        // This qmldir must not exist - which is not an error
        data->release();
        return true;
    } else if (data->status() == Complete) {
        // This data is already available
        return qmldirDataAvailable(data, errors);
    }

    // Wait for this data to become available
    addDependency(data);
    return true;
}

bool QQmlTypeLoader::Blob::updateQmldir(QQmlQmldirData *data, const QV4::CompiledData::Import *import, QList<QQmlError> *errors)
{
    QString qmldirIdentifier = data->url().toString();
    QString qmldirUrl = qmldirIdentifier.left(qmldirIdentifier.lastIndexOf(QLatin1Char('/')) + 1);

    typeLoader()->setQmldirContent(qmldirIdentifier, data->content());

    if (!m_importCache.updateQmldirContent(typeLoader()->importDatabase(), stringAt(import->uriIndex), stringAt(import->qualifierIndex), qmldirIdentifier, qmldirUrl, errors))
        return false;

    QHash<const QV4::CompiledData::Import *, int>::iterator it = m_unresolvedImports.find(import);
    if (it != m_unresolvedImports.end()) {
        *it = data->priority();
    }

    // Release this reference at destruction
    m_qmldirs << data;

    const QString &importQualifier = stringAt(import->qualifierIndex);
    if (!importQualifier.isEmpty()) {
        // Does this library contain any qualified scripts?
        QUrl libraryUrl(qmldirUrl);
        const QmldirContent *qmldir = typeLoader()->qmldirContent(qmldirIdentifier);
        foreach (const QQmlDirParser::Script &script, qmldir->scripts()) {
            QUrl scriptUrl = libraryUrl.resolved(QUrl(script.fileName));
            QQmlScriptBlob *blob = typeLoader()->getScript(scriptUrl);
            addDependency(blob);

            scriptImported(blob, import->location, script.nameSpace, importQualifier);
        }
    }

    return true;
}

bool QQmlTypeLoader::Blob::addImport(const QV4::CompiledData::Import *import, QList<QQmlError> *errors)
{
    Q_ASSERT(errors);

    QQmlImportDatabase *importDatabase = typeLoader()->importDatabase();

    const QString &importUri = stringAt(import->uriIndex);
    const QString &importQualifier = stringAt(import->qualifierIndex);
    if (import->type == QV4::CompiledData::Import::ImportScript) {
        QUrl scriptUrl = finalUrl().resolved(QUrl(importUri));
        QQmlScriptBlob *blob = typeLoader()->getScript(scriptUrl);
        addDependency(blob);

        scriptImported(blob, import->location, importQualifier, QString());
    } else if (import->type == QV4::CompiledData::Import::ImportLibrary) {
        QString qmldirFilePath;
        QString qmldirUrl;

        if (QQmlMetaType::isLockedModule(importUri, import->majorVersion)) {
            //Locked modules are checked first, to save on filesystem checks
            if (!m_importCache.addLibraryImport(importDatabase, importUri, importQualifier, import->majorVersion,
                                          import->minorVersion, QString(), QString(), false, errors))
                return false;

        } else if (m_importCache.locateQmldir(importDatabase, importUri, import->majorVersion, import->minorVersion,
                                 &qmldirFilePath, &qmldirUrl)) {
            // This is a local library import
            if (!m_importCache.addLibraryImport(importDatabase, importUri, importQualifier, import->majorVersion,
                                          import->minorVersion, qmldirFilePath, qmldirUrl, false, errors))
                return false;

            if (!importQualifier.isEmpty()) {
                // Does this library contain any qualified scripts?
                QUrl libraryUrl(qmldirUrl);
                const QmldirContent *qmldir = typeLoader()->qmldirContent(qmldirFilePath);
                foreach (const QQmlDirParser::Script &script, qmldir->scripts()) {
                    QUrl scriptUrl = libraryUrl.resolved(QUrl(script.fileName));
                    QQmlScriptBlob *blob = typeLoader()->getScript(scriptUrl);
                    addDependency(blob);

                    scriptImported(blob, import->location, script.nameSpace, importQualifier);
                }
            }
        } else {
            // Is this a module?
            if (QQmlMetaType::isAnyModule(importUri)) {
                if (!m_importCache.addLibraryImport(importDatabase, importUri, importQualifier, import->majorVersion,
                                              import->minorVersion, QString(), QString(), false, errors))
                    return false;
            } else {
                // We haven't yet resolved this import
                m_unresolvedImports.insert(import, 0);

                // Query any network import paths for this library
                QStringList remotePathList = importDatabase->importPathList(QQmlImportDatabase::Remote);
                if (!remotePathList.isEmpty()) {
                    // Add this library and request the possible locations for it
                    if (!m_importCache.addLibraryImport(importDatabase, importUri, importQualifier, import->majorVersion,
                                                  import->minorVersion, QString(), QString(), true, errors))
                        return false;

                    // Probe for all possible locations
                    int priority = 0;
                    const QStringList qmlDirPaths = QQmlImports::completeQmldirPaths(importUri, remotePathList, import->majorVersion, import->minorVersion);
                    for (const QString &qmldirPath : qmlDirPaths) {
                        if (!fetchQmldir(QUrl(qmldirPath), import, ++priority, errors))
                            return false;
                    }
                }
            }
        }
    } else {
        Q_ASSERT(import->type == QV4::CompiledData::Import::ImportFile);

        bool incomplete = false;

        QUrl qmldirUrl = finalUrl().resolved(QUrl(importUri + QLatin1String("/qmldir")));
        if (!QQmlImports::isLocal(qmldirUrl)) {
            // This is a remote file; the import is currently incomplete
            incomplete = true;
        }

        if (!m_importCache.addFileImport(importDatabase, importUri, importQualifier, import->majorVersion,
                                   import->minorVersion, incomplete, errors))
            return false;

        if (incomplete) {
            if (!fetchQmldir(qmldirUrl, import, 1, errors))
                return false;
        }
    }

    return true;
}

void QQmlTypeLoader::Blob::dependencyError(QQmlDataBlob *blob)
{
    if (blob->type() == QQmlDataBlob::QmldirFile) {
        QQmlQmldirData *data = static_cast<QQmlQmldirData *>(blob);
        data->release();
    }
}

void QQmlTypeLoader::Blob::dependencyComplete(QQmlDataBlob *blob)
{
    if (blob->type() == QQmlDataBlob::QmldirFile) {
        QQmlQmldirData *data = static_cast<QQmlQmldirData *>(blob);

        const QV4::CompiledData::Import *import = data->import();

        QList<QQmlError> errors;
        if (!qmldirDataAvailable(data, &errors)) {
            Q_ASSERT(errors.size());
            QQmlError error(errors.takeFirst());
            error.setUrl(m_importCache.baseUrl());
            error.setLine(import->location.line);
            error.setColumn(import->location.column);
            errors.prepend(error); // put it back on the list after filling out information.
            setError(errors);
        }
    }
}

bool QQmlTypeLoader::Blob::isDebugging() const
{
    return QV8Engine::getV4(typeLoader()->engine())->debugger() != 0;
}

bool QQmlTypeLoader::Blob::qmldirDataAvailable(QQmlQmldirData *data, QList<QQmlError> *errors)
{
    bool resolve = true;

    const QV4::CompiledData::Import *import = data->import();
    data->setImport(0);

    int priority = data->priority();
    data->setPriority(0);

    if (import) {
        // Do we need to resolve this import?
        QHash<const QV4::CompiledData::Import *, int>::iterator it = m_unresolvedImports.find(import);
        if (it != m_unresolvedImports.end()) {
            resolve = (*it == 0) || (*it > priority);
        }

        if (resolve) {
            // This is the (current) best resolution for this import
            if (!updateQmldir(data, import, errors)) {
                data->release();
                return false;
            }

            *it = priority;
            return true;
        }
    }

    data->release();
    return true;
}


QQmlTypeLoader::QmldirContent::QmldirContent()
{
}

bool QQmlTypeLoader::QmldirContent::hasError() const
{
    return m_parser.hasError();
}

QList<QQmlError> QQmlTypeLoader::QmldirContent::errors(const QString &uri) const
{
    return m_parser.errors(uri);
}

QString QQmlTypeLoader::QmldirContent::typeNamespace() const
{
    return m_parser.typeNamespace();
}

void QQmlTypeLoader::QmldirContent::setContent(const QString &location, const QString &content)
{
    m_location = location;
    m_parser.parse(content);
}

void QQmlTypeLoader::QmldirContent::setError(const QQmlError &error)
{
    m_parser.setError(error);
}

QQmlDirComponents QQmlTypeLoader::QmldirContent::components() const
{
    return m_parser.components();
}

QQmlDirScripts QQmlTypeLoader::QmldirContent::scripts() const
{
    return m_parser.scripts();
}

QQmlDirPlugins QQmlTypeLoader::QmldirContent::plugins() const
{
    return m_parser.plugins();
}

QString QQmlTypeLoader::QmldirContent::pluginLocation() const
{
    return m_location;
}

bool QQmlTypeLoader::QmldirContent::designerSupported() const
{
    return m_parser.designerSupported();
}

/*!
Constructs a new type loader that uses the given \a engine.
*/
QQmlTypeLoader::QQmlTypeLoader(QQmlEngine *engine)
    : m_engine(engine), m_thread(new QQmlTypeLoaderThread(this)),
      m_typeCacheTrimThreshold(TYPELOADER_MINIMUM_TRIM_THRESHOLD)
{
}

/*!
Destroys the type loader, first clearing the cache of any information about
loaded files.
*/
QQmlTypeLoader::~QQmlTypeLoader()
{
    // Stop the loader thread before releasing resources
    shutdownThread();

    clearCache();

    invalidate();
}

QQmlImportDatabase *QQmlTypeLoader::importDatabase()
{
    return &QQmlEnginePrivate::get(engine())->importDatabase;
}

/*!
Returns a QQmlTypeData for the specified \a url.  The QQmlTypeData may be cached.
*/
QQmlTypeData *QQmlTypeLoader::getType(const QUrl &url, Mode mode)
{
    Q_ASSERT(!url.isRelative() &&
            (QQmlFile::urlToLocalFileOrQrc(url).isEmpty() ||
             !QDir::isRelativePath(QQmlFile::urlToLocalFileOrQrc(url))));

    LockHolder<QQmlTypeLoader> holder(this);

    QQmlTypeData *typeData = m_typeCache.value(url);

    if (!typeData) {
        // Trim before adding the new type, so that we don't immediately trim it away
        if (m_typeCache.size() >= m_typeCacheTrimThreshold)
            trimCache();

        typeData = new QQmlTypeData(url, this);
        // TODO: if (compiledData == 0), is it safe to omit this insertion?
        m_typeCache.insert(url, typeData);
        if (const QQmlPrivate::CachedQmlUnit *cachedUnit = QQmlMetaType::findCachedCompilationUnit(typeData->url())) {
            QQmlTypeLoader::loadWithCachedUnit(typeData, cachedUnit, mode);
        } else {
            QQmlTypeLoader::load(typeData, mode);
        }
    } else if ((mode == PreferSynchronous || mode == Synchronous) && QQmlFile::isSynchronous(url)) {
        // this was started Asynchronous, but we need to force Synchronous
        // completion now (if at all possible with this type of URL).

        if (!m_thread->isThisThread()) {
            // this only works when called directly from the UI thread, but not
            // when recursively called on the QML thread via resolveTypes()

            while (!typeData->isCompleteOrError()) {
                unlock();
                m_thread->waitForNextMessage();
                lock();
            }
        }
    }

    typeData->addref();

    return typeData;
}

/*!
Returns a QQmlTypeData for the given \a data with the provided base \a url.  The
QQmlTypeData will not be cached.
*/
QQmlTypeData *QQmlTypeLoader::getType(const QByteArray &data, const QUrl &url, Mode mode)
{
    LockHolder<QQmlTypeLoader> holder(this);

    QQmlTypeData *typeData = new QQmlTypeData(url, this);
    QQmlTypeLoader::loadWithStaticData(typeData, data, mode);

    return typeData;
}

/*!
Return a QQmlScriptBlob for \a url.  The QQmlScriptData may be cached.
*/
QQmlScriptBlob *QQmlTypeLoader::getScript(const QUrl &url)
{
    Q_ASSERT(!url.isRelative() &&
            (QQmlFile::urlToLocalFileOrQrc(url).isEmpty() ||
             !QDir::isRelativePath(QQmlFile::urlToLocalFileOrQrc(url))));

    LockHolder<QQmlTypeLoader> holder(this);

    QQmlScriptBlob *scriptBlob = m_scriptCache.value(url);

    if (!scriptBlob) {
        scriptBlob = new QQmlScriptBlob(url, this);
        m_scriptCache.insert(url, scriptBlob);

        if (const QQmlPrivate::CachedQmlUnit *cachedUnit = QQmlMetaType::findCachedCompilationUnit(scriptBlob->url())) {
            QQmlTypeLoader::loadWithCachedUnit(scriptBlob, cachedUnit);
        } else {
            QQmlTypeLoader::load(scriptBlob);
        }
    }

    scriptBlob->addref();

    return scriptBlob;
}

/*!
Returns a QQmlQmldirData for \a url.  The QQmlQmldirData may be cached.
*/
QQmlQmldirData *QQmlTypeLoader::getQmldir(const QUrl &url)
{
    Q_ASSERT(!url.isRelative() &&
            (QQmlFile::urlToLocalFileOrQrc(url).isEmpty() ||
             !QDir::isRelativePath(QQmlFile::urlToLocalFileOrQrc(url))));

    LockHolder<QQmlTypeLoader> holder(this);

    QQmlQmldirData *qmldirData = m_qmldirCache.value(url);

    if (!qmldirData) {
        qmldirData = new QQmlQmldirData(url, this);
        m_qmldirCache.insert(url, qmldirData);
        QQmlTypeLoader::load(qmldirData);
    }

    qmldirData->addref();

    return qmldirData;
}

// #### Qt 6: Remove this function, it exists only for binary compatibility.
/*!
 * \internal
 */
bool QQmlEngine::addNamedBundle(const QString &name, const QString &fileName)
{
    Q_UNUSED(name)
    Q_UNUSED(fileName)
    return false;
}

/*!
Returns the absolute filename of path via a directory cache.
Returns a empty string if the path does not exist.

Why a directory cache?  QML checks for files in many paths with
invalid directories.  By caching whether a directory exists
we avoid many stats.  We also cache the files' existence in the
directory, for the same reason.
*/
QString QQmlTypeLoader::absoluteFilePath(const QString &path)
{
    if (path.isEmpty())
        return QString();
    if (path.at(0) == QLatin1Char(':')) {
        // qrc resource
        QFileInfo fileInfo(path);
        return fileInfo.isFile() ? fileInfo.absoluteFilePath() : QString();
    } else if (path.count() > 3 && path.at(3) == QLatin1Char(':') &&
               path.startsWith(QLatin1String("qrc"), Qt::CaseInsensitive)) {
        // qrc resource url
        QFileInfo fileInfo(QQmlFile::urlToLocalFileOrQrc(path));
        return fileInfo.isFile() ? fileInfo.absoluteFilePath() : QString();
    }
#if defined(Q_OS_ANDROID)
    else if (path.count() > 7 && path.at(6) == QLatin1Char(':') && path.at(7) == QLatin1Char('/') &&
           path.startsWith(QLatin1String("assets"), Qt::CaseInsensitive)) {
        // assets resource url
        QFileInfo fileInfo(QQmlFile::urlToLocalFileOrQrc(path));
        return fileInfo.isFile() ? fileInfo.absoluteFilePath() : QString();
    }
#endif

    int lastSlash = path.lastIndexOf(QLatin1Char('/'));
    QStringRef dirPath(&path, 0, lastSlash);

    StringSet **fileSet = m_importDirCache.value(QHashedStringRef(dirPath.constData(), dirPath.length()));
    if (!fileSet) {
        QHashedString dirPathString(dirPath.toString());
        bool exists = QDir(dirPathString).exists();
        QStringHash<bool> *files = exists ? new QStringHash<bool> : 0;
        m_importDirCache.insert(dirPathString, files);
        fileSet = m_importDirCache.value(dirPathString);
    }
    if (!(*fileSet))
        return QString();

    QString absoluteFilePath;
    QHashedStringRef fileName(path.constData()+lastSlash+1, path.length()-lastSlash-1);

    bool *value = (*fileSet)->value(fileName);
    if (value) {
        if (*value)
            absoluteFilePath = path;
    } else {
        bool exists = false;
#ifdef Q_OS_UNIX
        struct stat statBuf;
        // XXX Avoid encoding entire path. Should store encoded dirpath in cache
        if (::stat(QFile::encodeName(path).constData(), &statBuf) == 0)
            exists = S_ISREG(statBuf.st_mode);
#else
        exists = QFile::exists(path);
#endif
        (*fileSet)->insert(fileName.toString(), exists);
        if (exists)
            absoluteFilePath = path;
    }

    if (absoluteFilePath.length() > 2 && absoluteFilePath.at(0) != QLatin1Char('/') && absoluteFilePath.at(1) != QLatin1Char(':'))
        absoluteFilePath = QFileInfo(absoluteFilePath).absoluteFilePath();

    return absoluteFilePath;
}


/*!
Returns true if the path is a directory via a directory cache.  Cache is
shared with absoluteFilePath().
*/
bool QQmlTypeLoader::directoryExists(const QString &path)
{
    if (path.isEmpty())
        return false;

    bool isResource = path.at(0) == QLatin1Char(':');
#if defined(Q_OS_ANDROID)
    isResource = isResource || path.startsWith(QLatin1String("assets:/"));
#endif

    if (isResource) {
        // qrc resource
        QFileInfo fileInfo(path);
        return fileInfo.exists() && fileInfo.isDir();
    }

    int length = path.length();
    if (path.endsWith(QLatin1Char('/')))
        --length;
    QStringRef dirPath(&path, 0, length);

    StringSet **fileSet = m_importDirCache.value(QHashedStringRef(dirPath.constData(), dirPath.length()));
    if (!fileSet) {
        QHashedString dirPathString(dirPath.toString());
        bool exists = QDir(dirPathString).exists();
        QStringHash<bool> *files = exists ? new QStringHash<bool> : 0;
        m_importDirCache.insert(dirPathString, files);
        fileSet = m_importDirCache.value(dirPathString);
    }

    return (*fileSet);
}


/*!
Return a QmldirContent for absoluteFilePath.  The QmldirContent may be cached.

\a filePath is a local file path.

It can also be a remote path for a remote directory import, but it will have been cached by now in this case.
*/
const QQmlTypeLoader::QmldirContent *QQmlTypeLoader::qmldirContent(const QString &filePathIn)
{
    QUrl url(filePathIn); //May already contain http scheme
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
        return *(m_importQmlDirCache.value(filePathIn)); //Can't load the remote here, but should be cached
    else
        url = QUrl::fromLocalFile(filePathIn);
    if (engine() && engine()->urlInterceptor())
        url = engine()->urlInterceptor()->intercept(url, QQmlAbstractUrlInterceptor::QmldirFile);
    Q_ASSERT(url.scheme() == QLatin1String("file"));
    QString filePath;
    if (url.scheme() == QLatin1String("file"))
        filePath = url.toLocalFile();
    else
        filePath = url.path();

    QmldirContent *qmldir;
    QmldirContent **val = m_importQmlDirCache.value(filePath);
    if (!val) {
        qmldir = new QmldirContent;

#define ERROR(description) { QQmlError e; e.setDescription(description); qmldir->setError(e); }
#define NOT_READABLE_ERROR QString(QLatin1String("module \"$$URI$$\" definition \"%1\" not readable"))
#define CASE_MISMATCH_ERROR QString(QLatin1String("cannot load module \"$$URI$$\": File name case mismatch for \"%1\""))

        QFile file(filePath);
        if (!QQml_isFileCaseCorrect(filePath)) {
            ERROR(CASE_MISMATCH_ERROR.arg(filePath));
        } else if (file.open(QFile::ReadOnly)) {
            QByteArray data = file.readAll();
            qmldir->setContent(filePath, QString::fromUtf8(data));
        } else {
            ERROR(NOT_READABLE_ERROR.arg(filePath));
        }

#undef ERROR
#undef NOT_READABLE_ERROR
#undef CASE_MISMATCH_ERROR

        m_importQmlDirCache.insert(filePath, qmldir);
    } else {
        qmldir = *val;
    }

    return qmldir;
}

void QQmlTypeLoader::setQmldirContent(const QString &url, const QString &content)
{
    QmldirContent *qmldir;
    QmldirContent **val = m_importQmlDirCache.value(url);
    if (val) {
        qmldir = *val;
    } else {
        qmldir = new QmldirContent;
        m_importQmlDirCache.insert(url, qmldir);
    }

    qmldir->setContent(url, content);
}

/*!
Clears cached information about loaded files, including any type data, scripts
and qmldir information.
*/
void QQmlTypeLoader::clearCache()
{
    for (TypeCache::Iterator iter = m_typeCache.begin(), end = m_typeCache.end(); iter != end; ++iter)
        (*iter)->release();
    for (ScriptCache::Iterator iter = m_scriptCache.begin(), end = m_scriptCache.end(); iter != end; ++iter)
        (*iter)->release();
    for (QmldirCache::Iterator iter = m_qmldirCache.begin(), end = m_qmldirCache.end(); iter != end; ++iter)
        (*iter)->release();
    qDeleteAll(m_importDirCache);
    qDeleteAll(m_importQmlDirCache);

    m_typeCache.clear();
    m_typeCacheTrimThreshold = TYPELOADER_MINIMUM_TRIM_THRESHOLD;
    m_scriptCache.clear();
    m_qmldirCache.clear();
    m_importDirCache.clear();
    m_importQmlDirCache.clear();
}

void QQmlTypeLoader::updateTypeCacheTrimThreshold()
{
    int size = m_typeCache.size();
    if (size > m_typeCacheTrimThreshold)
        m_typeCacheTrimThreshold = size * 2;
    if (size < m_typeCacheTrimThreshold / 2)
        m_typeCacheTrimThreshold = qMax(size * 2, TYPELOADER_MINIMUM_TRIM_THRESHOLD);
}

void QQmlTypeLoader::trimCache()
{
    while (true) {
        QList<TypeCache::Iterator> unneededTypes;
        for (TypeCache::Iterator iter = m_typeCache.begin(), end = m_typeCache.end(); iter != end; ++iter)  {
            QQmlTypeData *typeData = iter.value();

            // typeData->m_compiledData may be set early on in the proccess of loading a file, so
            // it's important to check the general loading status of the typeData before making any
            // other decisions.
            if (typeData->count() == 1 && (typeData->isError() || typeData->isComplete())
                    && (!typeData->m_compiledData || typeData->m_compiledData->count() == 1)) {
                // There are no live objects of this type
                unneededTypes.append(iter);
            }
        }

        if (unneededTypes.isEmpty())
            break;

        while (!unneededTypes.isEmpty()) {
            TypeCache::Iterator iter = unneededTypes.takeLast();

            iter.value()->release();
            m_typeCache.erase(iter);
        }
    }

    updateTypeCacheTrimThreshold();

    // TODO: release any scripts which are no longer referenced by any types
}

bool QQmlTypeLoader::isTypeLoaded(const QUrl &url) const
{
    LockHolder<QQmlTypeLoader> holder(const_cast<QQmlTypeLoader *>(this));
    return m_typeCache.contains(url);
}

bool QQmlTypeLoader::isScriptLoaded(const QUrl &url) const
{
    LockHolder<QQmlTypeLoader> holder(const_cast<QQmlTypeLoader *>(this));
    return m_scriptCache.contains(url);
}

QQmlTypeData::TypeDataCallback::~TypeDataCallback()
{
}

QQmlTypeData::QQmlTypeData(const QUrl &url, QQmlTypeLoader *manager)
: QQmlTypeLoader::Blob(url, QmlFile, manager),
   m_typesResolved(false), m_implicitImportLoaded(false)
{

}

QQmlTypeData::~QQmlTypeData()
{
    for (int ii = 0; ii < m_scripts.count(); ++ii)
        m_scripts.at(ii).script->release();
    for (int ii = 0; ii < m_compositeSingletons.count(); ++ii) {
        if (QQmlTypeData *tdata = m_compositeSingletons.at(ii).typeData)
            tdata->release();
    }
    for (auto it = m_resolvedTypes.constBegin(), end = m_resolvedTypes.constEnd();
         it != end; ++it) {
        if (QQmlTypeData *tdata = it->typeData)
            tdata->release();
    }
}

const QList<QQmlTypeData::ScriptReference> &QQmlTypeData::resolvedScripts() const
{
    return m_scripts;
}

QV4::CompiledData::CompilationUnit *QQmlTypeData::compilationUnit() const
{
    return m_compiledData.data();
}

void QQmlTypeData::registerCallback(TypeDataCallback *callback)
{
    Q_ASSERT(!m_callbacks.contains(callback));
    m_callbacks.append(callback);
}

void QQmlTypeData::unregisterCallback(TypeDataCallback *callback)
{
    Q_ASSERT(m_callbacks.contains(callback));
    m_callbacks.removeOne(callback);
    Q_ASSERT(!m_callbacks.contains(callback));
}

bool QQmlTypeData::tryLoadFromDiskCache()
{
    if (disableDiskCache() && !forceDiskCache())
        return false;

    if (isDebugging())
        return false;

    QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(typeLoader()->engine());
    if (!v4)
        return false;

    QQmlRefPointer<QV4::CompiledData::CompilationUnit> unit = v4->iselFactory->createUnitForLoading();
    {
        QString error;
        if (!unit->loadFromDisk(url(), v4->iselFactory.data(), &error)) {
            qCDebug(DBG_DISK_CACHE) << "Error loading" << url().toString() << "from disk cache:" << error;
            return false;
        }
    }

    m_compiledData = unit;

    for (int i = 0, count = m_compiledData->objectCount(); i < count; ++i)
        m_typeReferences.collectFromObject(m_compiledData->objectAt(i));

    m_importCache.setBaseUrl(finalUrl(), finalUrlString());

    // For remote URLs, we don't delay the loading of the implicit import
    // because the loading probably requires an asynchronous fetch of the
    // qmldir (so we can't load it just in time).
    if (!finalUrl().scheme().isEmpty()) {
        QUrl qmldirUrl = finalUrl().resolved(QUrl(QLatin1String("qmldir")));
        if (!QQmlImports::isLocal(qmldirUrl)) {
            if (!loadImplicitImport())
                return false;

            // find the implicit import
            for (quint32 i = 0; i < m_compiledData->data->nImports; ++i) {
                const QV4::CompiledData::Import *import = m_compiledData->data->importAt(i);
                if (m_compiledData->stringAt(import->uriIndex) == QLatin1String(".")
                    && import->qualifierIndex == 0
                    && import->majorVersion == -1
                    && import->minorVersion == -1) {
                    QList<QQmlError> errors;
                    if (!fetchQmldir(qmldirUrl, import, 1, &errors)) {
                        setError(errors);
                        return false;
                    }
                    break;
                }
            }
        }
    }

    for (int i = 0, count = m_compiledData->data->nImports; i < count; ++i) {
        const QV4::CompiledData::Import *import = m_compiledData->data->importAt(i);
        QList<QQmlError> errors;
        if (!addImport(import, &errors)) {
            Q_ASSERT(errors.size());
            QQmlError error(errors.takeFirst());
            error.setUrl(m_importCache.baseUrl());
            error.setLine(import->location.line);
            error.setColumn(import->location.column);
            errors.prepend(error); // put it back on the list after filling out information.
            setError(errors);
            return false;
        }
    }

    return true;
}

void QQmlTypeData::createTypeAndPropertyCaches(const QQmlRefPointer<QQmlTypeNameCache> &importCache,
                                                const QV4::CompiledData::ResolvedTypeReferenceMap &resolvedTypeCache)
{
    Q_ASSERT(m_compiledData);
    m_compiledData->importCache = importCache;
    m_compiledData->resolvedTypes = resolvedTypeCache;

    QQmlEnginePrivate * const engine = QQmlEnginePrivate::get(typeLoader()->engine());

    {
        QQmlPropertyCacheCreator<QV4::CompiledData::CompilationUnit> propertyCacheCreator(&m_compiledData->propertyCaches, engine, m_compiledData, &m_importCache);
        QQmlCompileError error = propertyCacheCreator.buildMetaObjects();
        if (error.isSet()) {
            setError(error);
            return;
        }
    }

    QQmlPropertyCacheAliasCreator<QV4::CompiledData::CompilationUnit> aliasCreator(&m_compiledData->propertyCaches, m_compiledData);
    aliasCreator.appendAliasPropertiesToMetaObjects();
}

void QQmlTypeData::done()
{
    QDeferredCleanup cleanup([this]{
        m_document.reset();
        m_typeReferences.clear();
        if (isError())
            m_compiledData = nullptr;
    });

    if (isError())
        return;

    // Check all script dependencies for errors
    for (int ii = 0; ii < m_scripts.count(); ++ii) {
        const ScriptReference &script = m_scripts.at(ii);
        Q_ASSERT(script.script->isCompleteOrError());
        if (script.script->isError()) {
            QList<QQmlError> errors = script.script->errors();
            QQmlError error;
            error.setUrl(finalUrl());
            error.setLine(script.location.line);
            error.setColumn(script.location.column);
            error.setDescription(QQmlTypeLoader::tr("Script %1 unavailable").arg(script.script->url().toString()));
            errors.prepend(error);
            setError(errors);
            return;
        }
    }

    // Check all type dependencies for errors
    for (auto it = m_resolvedTypes.constBegin(), end = m_resolvedTypes.constEnd(); it != end;
         ++it) {
        const TypeReference &type = *it;
        Q_ASSERT(!type.typeData || type.typeData->isCompleteOrError());
        if (type.typeData && type.typeData->isError()) {
            const QString typeName = stringAt(it.key());

            QList<QQmlError> errors = type.typeData->errors();
            QQmlError error;
            error.setUrl(finalUrl());
            error.setLine(type.location.line);
            error.setColumn(type.location.column);
            error.setDescription(QQmlTypeLoader::tr("Type %1 unavailable").arg(typeName));
            errors.prepend(error);
            setError(errors);
            return;
        }
    }

    // Check all composite singleton type dependencies for errors
    for (int ii = 0; ii < m_compositeSingletons.count(); ++ii) {
        const TypeReference &type = m_compositeSingletons.at(ii);
        Q_ASSERT(!type.typeData || type.typeData->isCompleteOrError());
        if (type.typeData && type.typeData->isError()) {
            QString typeName = type.type->qmlTypeName();

            QList<QQmlError> errors = type.typeData->errors();
            QQmlError error;
            error.setUrl(finalUrl());
            error.setLine(type.location.line);
            error.setColumn(type.location.column);
            error.setDescription(QQmlTypeLoader::tr("Type %1 unavailable").arg(typeName));
            errors.prepend(error);
            setError(errors);
            return;
        }
    }

    QQmlRefPointer<QQmlTypeNameCache> importCache;
    QV4::CompiledData::ResolvedTypeReferenceMap resolvedTypeCache;
    {
        QQmlCompileError error = buildTypeResolutionCaches(&importCache, &resolvedTypeCache);
        if (error.isSet()) {
            setError(error);
            return;
        }
    }

    QQmlEngine *const engine = typeLoader()->engine();

    // verify if any dependencies changed if we're using a cache
    if (m_document.isNull() && !m_compiledData->verifyChecksum(engine, resolvedTypeCache)) {
        qCDebug(DBG_DISK_CACHE) << "Checksum mismatch for cached version of" << m_compiledData->url().toString();
        if (!loadFromSource())
            return;
        m_backupSourceCode.clear();
        m_compiledData = nullptr;
    }

    if (!m_document.isNull()) {
        // Compile component
        compile(importCache, resolvedTypeCache);
    } else {
        createTypeAndPropertyCaches(importCache, resolvedTypeCache);
    }

    if (isError())
        return;

    {
        QQmlEnginePrivate *const enginePrivate = QQmlEnginePrivate::get(engine);
        {
        // Sanity check property bindings
            QQmlPropertyValidator validator(enginePrivate, m_importCache, m_compiledData);
            QVector<QQmlCompileError> errors = validator.validate();
            if (!errors.isEmpty()) {
                setError(errors);
                return;
            }
        }

        m_compiledData->finalize(enginePrivate);
    }

    {
        QQmlType *type = QQmlMetaType::qmlType(finalUrl(), true);
        if (m_compiledData && m_compiledData->data->flags & QV4::CompiledData::Unit::IsSingleton) {
            if (!type) {
                QQmlError error;
                error.setDescription(QQmlTypeLoader::tr("No matching type found, pragma Singleton files cannot be used by QQmlComponent."));
                setError(error);
                return;
            } else if (!type->isCompositeSingleton()) {
                QQmlError error;
                error.setDescription(QQmlTypeLoader::tr("pragma Singleton used with a non composite singleton type %1").arg(type->qmlTypeName()));
                setError(error);
                return;
            }
        } else {
            // If the type is CompositeSingleton but there was no pragma Singleton in the
            // QML file, lets report an error.
            if (type && type->isCompositeSingleton()) {
                QString typeName = type->qmlTypeName();
                setError(QQmlTypeLoader::tr("qmldir defines type as singleton, but no pragma Singleton found in type %1.").arg(typeName));
                return;
            }
        }
    }

    {
        // Collect imported scripts
        m_compiledData->dependentScripts.reserve(m_scripts.count());
        for (int scriptIndex = 0; scriptIndex < m_scripts.count(); ++scriptIndex) {
            const QQmlTypeData::ScriptReference &script = m_scripts.at(scriptIndex);

            QStringRef qualifier(&script.qualifier);
            QString enclosingNamespace;

            const int lastDotIndex = qualifier.lastIndexOf(QLatin1Char('.'));
            if (lastDotIndex != -1) {
                enclosingNamespace = qualifier.left(lastDotIndex).toString();
                qualifier = qualifier.mid(lastDotIndex+1);
            }

            m_compiledData->importCache->add(qualifier.toString(), scriptIndex, enclosingNamespace);
            QQmlScriptData *scriptData = script.script->scriptData();
            scriptData->addref();
            m_compiledData->dependentScripts << scriptData;
        }
    }
}

void QQmlTypeData::completed()
{
    // Notify callbacks
    while (!m_callbacks.isEmpty()) {
        TypeDataCallback *callback = m_callbacks.takeFirst();
        callback->typeDataReady(this);
    }
}

bool QQmlTypeData::loadImplicitImport()
{
    m_implicitImportLoaded = true; // Even if we hit an error, count as loaded (we'd just keep hitting the error)

    m_importCache.setBaseUrl(finalUrl(), finalUrlString());

    QQmlImportDatabase *importDatabase = typeLoader()->importDatabase();
    // For local urls, add an implicit import "." as most overridden lookup.
    // This will also trigger the loading of the qmldir and the import of any native
    // types from available plugins.
    QList<QQmlError> implicitImportErrors;
    m_importCache.addImplicitImport(importDatabase, &implicitImportErrors);

    if (!implicitImportErrors.isEmpty()) {
        setError(implicitImportErrors);
        return false;
    }

    return true;
}

void QQmlTypeData::dataReceived(const Data &data)
{
    QString error;
    m_backupSourceCode = data.readAll(&error, &m_sourceTimeStamp);
    // if we failed to read the source code, process it _after_ we've tried
    // to use the disk cache, in order to support scenarios where the source
    // was removed deliberately.

    if (tryLoadFromDiskCache())
        return;

    if (isError())
        return;

    if (!error.isEmpty()) {
        setError(error);
        return;
    }

    if (!loadFromSource())
        return;

    continueLoadFromIR();
}

void QQmlTypeData::initializeFromCachedUnit(const QQmlPrivate::CachedQmlUnit *unit)
{
    m_document.reset(new QmlIR::Document(isDebugging()));
    unit->loadIR(m_document.data(), unit);
    continueLoadFromIR();
}

bool QQmlTypeData::loadFromSource()
{
    QString code = QString::fromUtf8(m_backupSourceCode);
    m_document.reset(new QmlIR::Document(isDebugging()));
    m_document->jsModule.sourceTimeStamp = m_sourceTimeStamp;
    QQmlEngine *qmlEngine = typeLoader()->engine();
    QmlIR::IRBuilder compiler(QV8Engine::get(qmlEngine)->illegalNames());
    if (!compiler.generateFromQml(code, finalUrlString(), m_document.data())) {
        QList<QQmlError> errors;
        errors.reserve(compiler.errors.count());
        foreach (const QQmlJS::DiagnosticMessage &msg, compiler.errors) {
            QQmlError e;
            e.setUrl(finalUrl());
            e.setLine(msg.loc.startLine);
            e.setColumn(msg.loc.startColumn);
            e.setDescription(msg.message);
            errors << e;
        }
        setError(errors);
        return false;
    }
    return true;
}

void QQmlTypeData::continueLoadFromIR()
{
    m_typeReferences.collectFromObjects(m_document->objects.constBegin(), m_document->objects.constEnd());
    m_importCache.setBaseUrl(finalUrl(), finalUrlString());

    // For remote URLs, we don't delay the loading of the implicit import
    // because the loading probably requires an asynchronous fetch of the
    // qmldir (so we can't load it just in time).
    if (!finalUrl().scheme().isEmpty()) {
        QUrl qmldirUrl = finalUrl().resolved(QUrl(QLatin1String("qmldir")));
        if (!QQmlImports::isLocal(qmldirUrl)) {
            if (!loadImplicitImport())
                return;
            // This qmldir is for the implicit import
            QQmlJS::MemoryPool *pool = m_document->jsParserEngine.pool();
            auto implicitImport = pool->New<QV4::CompiledData::Import>();
            implicitImport->uriIndex = m_document->registerString(QLatin1String("."));
            implicitImport->qualifierIndex = 0; // empty string
            implicitImport->majorVersion = -1;
            implicitImport->minorVersion = -1;
            QList<QQmlError> errors;

            if (!fetchQmldir(qmldirUrl, implicitImport, 1, &errors)) {
                setError(errors);
                return;
            }
        }
    }

    QList<QQmlError> errors;

    foreach (const QV4::CompiledData::Import *import, m_document->imports) {
        if (!addImport(import, &errors)) {
            Q_ASSERT(errors.size());
            QQmlError error(errors.takeFirst());
            error.setUrl(m_importCache.baseUrl());
            error.setLine(import->location.line);
            error.setColumn(import->location.column);
            errors.prepend(error); // put it back on the list after filling out information.
            setError(errors);
            return;
        }
    }
}

void QQmlTypeData::allDependenciesDone()
{
    if (!m_typesResolved) {
        // Check that all imports were resolved
        QList<QQmlError> errors;
        QHash<const QV4::CompiledData::Import *, int>::const_iterator it = m_unresolvedImports.constBegin(), end = m_unresolvedImports.constEnd();
        for ( ; it != end; ++it) {
            if (*it == 0) {
                // This import was not resolved
                for (auto keyIt = m_unresolvedImports.keyBegin(),
                     keyEnd = m_unresolvedImports.keyEnd();
                     keyIt != keyEnd; ++keyIt) {
                    const QV4::CompiledData::Import *import = *keyIt;
                    QQmlError error;
                    error.setDescription(QQmlTypeLoader::tr("module \"%1\" is not installed").arg(stringAt(import->uriIndex)));
                    error.setUrl(m_importCache.baseUrl());
                    error.setLine(import->location.line);
                    error.setColumn(import->location.column);
                    errors.prepend(error);
                }
            }
        }
        if (errors.size()) {
            setError(errors);
            return;
        }

        resolveTypes();
        m_typesResolved = true;
    }
}

void QQmlTypeData::downloadProgressChanged(qreal p)
{
    for (int ii = 0; ii < m_callbacks.count(); ++ii) {
        TypeDataCallback *callback = m_callbacks.at(ii);
        callback->typeDataProgress(this, p);
    }
}

QString QQmlTypeData::stringAt(int index) const
{
    if (m_compiledData)
        return m_compiledData->stringAt(index);
    return m_document->jsGenerator.stringTable.stringForIndex(index);
}

void QQmlTypeData::compile(const QQmlRefPointer<QQmlTypeNameCache> &importCache, const QV4::CompiledData::ResolvedTypeReferenceMap &resolvedTypeCache)
{
    Q_ASSERT(m_compiledData.isNull());

    QQmlEnginePrivate * const enginePrivate = QQmlEnginePrivate::get(typeLoader()->engine());
    QQmlTypeCompiler compiler(enginePrivate, this, m_document.data(), importCache, resolvedTypeCache);
    m_compiledData = compiler.compile();
    if (!m_compiledData) {
        setError(compiler.compilationErrors());
        return;
    }

    const bool trySaveToDisk = (!disableDiskCache() || forceDiskCache()) && !m_document->jsModule.debugMode;
    if (trySaveToDisk) {
        QString errorString;
        if (m_compiledData->saveToDisk(url(), &errorString)) {
            QString error;
            if (!m_compiledData->loadFromDisk(url(), enginePrivate->v4engine()->iselFactory.data(), &error)) {
                // ignore error, keep using the in-memory compilation unit.
            }
        } else {
            qCDebug(DBG_DISK_CACHE) << "Error saving cached version of" << m_compiledData->url().toString() << "to disk:" << errorString;
        }
    }
}

void QQmlTypeData::resolveTypes()
{
    // Add any imported scripts to our resolved set
    foreach (const QQmlImports::ScriptReference &script, m_importCache.resolvedScripts())
    {
        QQmlScriptBlob *blob = typeLoader()->getScript(script.location);
        addDependency(blob);

        ScriptReference ref;
        //ref.location = ...
        if (!script.qualifier.isEmpty())
        {
            ref.qualifier = script.qualifier + QLatin1Char('.') + script.nameSpace;
            // Add a reference to the enclosing namespace
            m_namespaces.insert(script.qualifier);
        } else {
            ref.qualifier = script.nameSpace;
        }

        ref.script = blob;
        m_scripts << ref;
    }

    // Lets handle resolved composite singleton types
    foreach (const QQmlImports::CompositeSingletonReference &csRef, m_importCache.resolvedCompositeSingletons()) {
        TypeReference ref;
        QString typeName;
        if (!csRef.prefix.isEmpty()) {
            typeName = csRef.prefix + QLatin1Char('.') + csRef.typeName;
            // Add a reference to the enclosing namespace
            m_namespaces.insert(csRef.prefix);
        } else {
            typeName = csRef.typeName;
        }

        int majorVersion = csRef.majorVersion > -1 ? csRef.majorVersion : -1;
        int minorVersion = csRef.minorVersion > -1 ? csRef.minorVersion : -1;

        if (!resolveType(typeName, majorVersion, minorVersion, ref))
            return;

        if (ref.type->isCompositeSingleton()) {
            ref.typeData = typeLoader()->getType(ref.type->sourceUrl());
            addDependency(ref.typeData);
            ref.prefix = csRef.prefix;

            m_compositeSingletons << ref;
        }
    }

    for (QV4::CompiledData::TypeReferenceMap::ConstIterator unresolvedRef = m_typeReferences.constBegin(), end = m_typeReferences.constEnd();
         unresolvedRef != end; ++unresolvedRef) {

        TypeReference ref; // resolved reference

        const bool reportErrors = unresolvedRef->errorWhenNotFound;

        int majorVersion = -1;
        int minorVersion = -1;
        QQmlImportNamespace *typeNamespace = 0;
        QList<QQmlError> errors;

        const QString name = stringAt(unresolvedRef.key());
        bool typeFound = m_importCache.resolveType(name, &ref.type,
                &majorVersion, &minorVersion, &typeNamespace, &errors);
        if (!typeNamespace && !typeFound && !m_implicitImportLoaded) {
            // Lazy loading of implicit import
            if (loadImplicitImport()) {
                // Try again to find the type
                errors.clear();
                typeFound = m_importCache.resolveType(name, &ref.type,
                    &majorVersion, &minorVersion, &typeNamespace, &errors);
            } else {
                return; //loadImplicitImport() hit an error, and called setError already
            }
        }

        if ((!typeFound || typeNamespace) && reportErrors) {
            // Known to not be a type:
            //  - known to be a namespace (Namespace {})
            //  - type with unknown namespace (UnknownNamespace.SomeType {})
            QQmlError error;
            if (typeNamespace) {
                error.setDescription(QQmlTypeLoader::tr("Namespace %1 cannot be used as a type").arg(name));
            } else {
                if (errors.size()) {
                    error = errors.takeFirst();
                } else {
                    // this should not be possible!
                    // Description should come from error provided by addImport() function.
                    error.setDescription(QQmlTypeLoader::tr("Unreported error adding script import to import database"));
                }
                error.setUrl(m_importCache.baseUrl());
                error.setDescription(QQmlTypeLoader::tr("%1 %2").arg(name).arg(error.description()));
            }

            error.setLine(unresolvedRef->location.line);
            error.setColumn(unresolvedRef->location.column);

            errors.prepend(error);
            setError(errors);
            return;
        }

        if (ref.type && ref.type->isComposite()) {
            ref.typeData = typeLoader()->getType(ref.type->sourceUrl());
            addDependency(ref.typeData);
        }
        ref.majorVersion = majorVersion;
        ref.minorVersion = minorVersion;

        ref.location.line = unresolvedRef->location.line;
        ref.location.column = unresolvedRef->location.column;

        ref.needsCreation = unresolvedRef->needsCreation;

        m_resolvedTypes.insert(unresolvedRef.key(), ref);
    }
}

QQmlCompileError QQmlTypeData::buildTypeResolutionCaches(
        QQmlRefPointer<QQmlTypeNameCache> *importCache,
        QV4::CompiledData::ResolvedTypeReferenceMap *resolvedTypeCache
        ) const
{
    importCache->adopt(new QQmlTypeNameCache);

    for (const QString &ns: m_namespaces)
        (*importCache)->add(ns);

    // Add any Composite Singletons that were used to the import cache
    for (const QQmlTypeData::TypeReference &singleton: m_compositeSingletons)
        (*importCache)->add(singleton.type->qmlTypeName(), singleton.type->sourceUrl(), singleton.prefix);

    m_importCache.populateCache(*importCache);

    QQmlEnginePrivate * const engine = QQmlEnginePrivate::get(typeLoader()->engine());

    for (auto resolvedType = m_resolvedTypes.constBegin(), end = m_resolvedTypes.constEnd(); resolvedType != end; ++resolvedType) {
        QScopedPointer<QV4::CompiledData::ResolvedTypeReference> ref(new QV4::CompiledData::ResolvedTypeReference);
        QQmlType *qmlType = resolvedType->type;
        if (resolvedType->typeData) {
            if (resolvedType->needsCreation && qmlType->isCompositeSingleton()) {
                return QQmlCompileError(resolvedType->location, tr("Composite Singleton Type %1 is not creatable.").arg(qmlType->qmlTypeName()));
            }
            ref->compilationUnit = resolvedType->typeData->compilationUnit();
        } else if (qmlType) {
            ref->type = qmlType;
            Q_ASSERT(ref->type);

            if (resolvedType->needsCreation && !ref->type->isCreatable()) {
                QString reason = ref->type->noCreationReason();
                if (reason.isEmpty())
                    reason = tr("Element is not creatable.");
                return QQmlCompileError(resolvedType->location, reason);
            }

            if (ref->type->containsRevisionedAttributes()) {
                ref->typePropertyCache = engine->cache(ref->type,
                                                       resolvedType->minorVersion);
            }
        }
        ref->majorVersion = resolvedType->majorVersion;
        ref->minorVersion = resolvedType->minorVersion;
        ref->doDynamicTypeCheck();
        resolvedTypeCache->insert(resolvedType.key(), ref.take());
    }
    QQmlCompileError noError;
    return noError;
}

bool QQmlTypeData::resolveType(const QString &typeName, int &majorVersion, int &minorVersion, TypeReference &ref)
{
    QQmlImportNamespace *typeNamespace = 0;
    QList<QQmlError> errors;

    bool typeFound = m_importCache.resolveType(typeName, &ref.type,
                                          &majorVersion, &minorVersion, &typeNamespace, &errors);
    if (!typeNamespace && !typeFound && !m_implicitImportLoaded) {
        // Lazy loading of implicit import
        if (loadImplicitImport()) {
            // Try again to find the type
            errors.clear();
            typeFound = m_importCache.resolveType(typeName, &ref.type,
                                              &majorVersion, &minorVersion, &typeNamespace, &errors);
        } else {
            return false; //loadImplicitImport() hit an error, and called setError already
        }
    }

    if (!typeFound || typeNamespace) {
        // Known to not be a type:
        //  - known to be a namespace (Namespace {})
        //  - type with unknown namespace (UnknownNamespace.SomeType {})
        QQmlError error;
        if (typeNamespace) {
            error.setDescription(QQmlTypeLoader::tr("Namespace %1 cannot be used as a type").arg(typeName));
        } else {
            if (errors.size()) {
                error = errors.takeFirst();
            } else {
                // this should not be possible!
                // Description should come from error provided by addImport() function.
                error.setDescription(QQmlTypeLoader::tr("Unreported error adding script import to import database"));
            }
            error.setUrl(m_importCache.baseUrl());
            error.setDescription(QQmlTypeLoader::tr("%1 %2").arg(typeName).arg(error.description()));
        }

        errors.prepend(error);
        setError(errors);
        return false;
    }

    return true;
}

void QQmlTypeData::scriptImported(QQmlScriptBlob *blob, const QV4::CompiledData::Location &location, const QString &qualifier, const QString &/*nameSpace*/)
{
    ScriptReference ref;
    ref.script = blob;
    ref.location = location;
    ref.qualifier = qualifier;

    m_scripts << ref;
}

QQmlScriptData::QQmlScriptData()
    : importCache(0)
    , m_loaded(false)
    , m_program(0)
{
}

QQmlScriptData::~QQmlScriptData()
{
    delete m_program;
}

void QQmlScriptData::initialize(QQmlEngine *engine)
{
    Q_ASSERT(!m_program);
    Q_ASSERT(engine);
    Q_ASSERT(!hasEngine());

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
    QV8Engine *v8engine = ep->v8engine();
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8engine);

    m_program = new QV4::Script(v4, 0, m_precompiledScript);

    addToEngine(engine);

    addref();
}

QV4::ReturnedValue QQmlScriptData::scriptValueForContext(QQmlContextData *parentCtxt)
{
    if (m_loaded)
        return m_value.value();

    Q_ASSERT(parentCtxt && parentCtxt->engine);
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(parentCtxt->engine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(parentCtxt->engine);
    QV4::Scope scope(v4);

    bool shared = m_precompiledScript->data->flags & QV4::CompiledData::Unit::IsSharedLibrary;

    QQmlContextData *effectiveCtxt = parentCtxt;
    if (shared)
        effectiveCtxt = 0;

    // Create the script context if required
    QQmlContextData *ctxt = new QQmlContextData;
    ctxt->isInternal = true;
    ctxt->isJSContext = true;
    if (shared)
        ctxt->isPragmaLibraryContext = true;
    else
        ctxt->isPragmaLibraryContext = parentCtxt->isPragmaLibraryContext;
    ctxt->baseUrl = url;
    ctxt->baseUrlString = urlString;

    // For backward compatibility, if there are no imports, we need to use the
    // imports from the parent context.  See QTBUG-17518.
    if (!importCache->isEmpty()) {
        ctxt->imports = importCache;
    } else if (effectiveCtxt) {
        ctxt->imports = effectiveCtxt->imports;
        ctxt->importedScripts = effectiveCtxt->importedScripts;
    }

    if (effectiveCtxt) {
        ctxt->setParent(effectiveCtxt, true);
    } else {
        ctxt->engine = parentCtxt->engine; // Fix for QTBUG-21620
    }

    QV4::ScopedObject scriptsArray(scope);
    if (ctxt->importedScripts.isNullOrUndefined()) {
        scriptsArray = v4->newArrayObject(scripts.count());
        ctxt->importedScripts.set(v4, scriptsArray);
    } else {
        scriptsArray = ctxt->importedScripts.valueRef();
    }
    QV4::ScopedValue v(scope);
    for (int ii = 0; ii < scripts.count(); ++ii)
        scriptsArray->putIndexed(ii, (v = scripts.at(ii)->scriptData()->scriptValueForContext(ctxt)));

    if (!hasEngine())
        initialize(parentCtxt->engine);

    if (!m_program) {
        if (shared)
            m_loaded = true;
        ctxt->destroy();
        return QV4::Encode::undefined();
    }

    QV4::Scoped<QV4::QmlContext> qmlContext(scope, v4->rootContext()->newQmlContext(ctxt, 0));
    qmlContext->takeContextOwnership();

    m_program->qmlContext.set(scope.engine, qmlContext);
    m_program->run();
    if (scope.engine->hasException) {
        QQmlError error = scope.engine->catchExceptionAsQmlError();
        if (error.isValid())
            ep->warning(error);
    }

    QV4::ScopedValue retval(scope, qmlContext->d()->qml);
    if (shared) {
        m_value.set(scope.engine, retval);
        m_loaded = true;
    }

    return retval->asReturnedValue();
}

void QQmlScriptData::clear()
{
    if (importCache) {
        importCache->release();
        importCache = 0;
    }

    for (int ii = 0; ii < scripts.count(); ++ii)
        scripts.at(ii)->release();
    scripts.clear();

    // An addref() was made when the QQmlCleanup was added to the engine.
    release();
}

QQmlScriptBlob::QQmlScriptBlob(const QUrl &url, QQmlTypeLoader *loader)
: QQmlTypeLoader::Blob(url, JavaScriptFile, loader), m_scriptData(0)
{
}

QQmlScriptBlob::~QQmlScriptBlob()
{
    if (m_scriptData) {
        m_scriptData->release();
        m_scriptData = 0;
    }
}

QQmlScriptData *QQmlScriptBlob::scriptData() const
{
    return m_scriptData;
}

struct EmptyCompilationUnit : public QV4::CompiledData::CompilationUnit
{
    virtual void linkBackendToEngine(QV4::ExecutionEngine *) {}
};

void QQmlScriptBlob::dataReceived(const Data &data)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(m_typeLoader->engine());

    if (!disableDiskCache() || forceDiskCache()) {
        QQmlRefPointer<QV4::CompiledData::CompilationUnit> unit = v4->iselFactory->createUnitForLoading();
        QString error;
        if (unit->loadFromDisk(url(), v4->iselFactory.data(), &error)) {
            initializeFromCompilationUnit(unit);
            return;
        } else {
            qCDebug(DBG_DISK_CACHE()) << "Error loading" << url().toString() << "from disk cache:" << error;
        }
    }


    QmlIR::Document irUnit(isDebugging());

    QString error;
    QString source = QString::fromUtf8(data.readAll(&error, &irUnit.jsModule.sourceTimeStamp));
    if (!error.isEmpty()) {
        setError(error);
        return;
    }

    QmlIR::ScriptDirectivesCollector collector(&irUnit.jsParserEngine, &irUnit.jsGenerator);

    QList<QQmlError> errors;
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> unit = QV4::Script::precompile(&irUnit.jsModule, &irUnit.jsGenerator, v4, finalUrl(), source, &errors, &collector);
    // No need to addref on unit, it's initial refcount is 1
    source.clear();
    if (!errors.isEmpty()) {
        setError(errors);
        return;
    }
    if (!unit) {
        unit.adopt(new EmptyCompilationUnit);
    }
    irUnit.javaScriptCompilationUnit = unit;
    irUnit.imports = collector.imports;
    if (collector.hasPragmaLibrary)
        irUnit.jsModule.unitFlags |= QV4::CompiledData::Unit::IsSharedLibrary;

    QmlIR::QmlUnitGenerator qmlGenerator;
    QV4::CompiledData::ResolvedTypeReferenceMap emptyDependencies;
    QV4::CompiledData::Unit *unitData = qmlGenerator.generate(irUnit, m_typeLoader->engine(), emptyDependencies);
    Q_ASSERT(!unit->data);
    // The js unit owns the data and will free the qml unit.
    unit->data = unitData;

    if (!disableDiskCache() || forceDiskCache()) {
        QString errorString;
        if (!unit->saveToDisk(url(), &errorString)) {
            qCDebug(DBG_DISK_CACHE()) << "Error saving cached version of" << unit->url().toString() << "to disk:" << errorString;
        }
    }

    initializeFromCompilationUnit(unit);
}

void QQmlScriptBlob::initializeFromCachedUnit(const QQmlPrivate::CachedQmlUnit *unit)
{
    initializeFromCompilationUnit(unit->createCompilationUnit());
}

void QQmlScriptBlob::done()
{
    if (isError())
        return;

    // Check all script dependencies for errors
    for (int ii = 0; ii < m_scripts.count(); ++ii) {
        const ScriptReference &script = m_scripts.at(ii);
        Q_ASSERT(script.script->isCompleteOrError());
        if (script.script->isError()) {
            QList<QQmlError> errors = script.script->errors();
            QQmlError error;
            error.setUrl(finalUrl());
            error.setLine(script.location.line);
            error.setColumn(script.location.column);
            error.setDescription(QQmlTypeLoader::tr("Script %1 unavailable").arg(script.script->url().toString()));
            errors.prepend(error);
            setError(errors);
            return;
        }
    }

    m_scriptData->importCache = new QQmlTypeNameCache();

    QSet<QString> ns;

    for (int scriptIndex = 0; scriptIndex < m_scripts.count(); ++scriptIndex) {
        const ScriptReference &script = m_scripts.at(scriptIndex);

        m_scriptData->scripts.append(script.script);

        if (!script.nameSpace.isNull()) {
            if (!ns.contains(script.nameSpace)) {
                ns.insert(script.nameSpace);
                m_scriptData->importCache->add(script.nameSpace);
            }
        }
        m_scriptData->importCache->add(script.qualifier, scriptIndex, script.nameSpace);
    }

    m_importCache.populateCache(m_scriptData->importCache);
}

QString QQmlScriptBlob::stringAt(int index) const
{
    return m_scriptData->m_precompiledScript->data->stringAt(index);
}

void QQmlScriptBlob::scriptImported(QQmlScriptBlob *blob, const QV4::CompiledData::Location &location, const QString &qualifier, const QString &nameSpace)
{
    ScriptReference ref;
    ref.script = blob;
    ref.location = location;
    ref.qualifier = qualifier;
    ref.nameSpace = nameSpace;

    m_scripts << ref;
}

void QQmlScriptBlob::initializeFromCompilationUnit(QV4::CompiledData::CompilationUnit *unit)
{
    Q_ASSERT(!m_scriptData);
    m_scriptData = new QQmlScriptData();
    m_scriptData->url = finalUrl();
    m_scriptData->urlString = finalUrlString();
    m_scriptData->m_precompiledScript = unit;

    m_importCache.setBaseUrl(finalUrl(), finalUrlString());

    Q_ASSERT(m_scriptData->m_precompiledScript->data->flags & QV4::CompiledData::Unit::IsQml);
    const QV4::CompiledData::Unit *qmlUnit = m_scriptData->m_precompiledScript->data;

    QList<QQmlError> errors;
    for (quint32 i = 0; i < qmlUnit->nImports; ++i) {
        const QV4::CompiledData::Import *import = qmlUnit->importAt(i);
        if (!addImport(import, &errors)) {
           Q_ASSERT(errors.size());
            QQmlError error(errors.takeFirst());
            error.setUrl(m_importCache.baseUrl());
            error.setLine(import->location.line);
            error.setColumn(import->location.column);
            errors.prepend(error); // put it back on the list after filling out information.
            setError(errors);
            return;
        }
    }
}

QQmlQmldirData::QQmlQmldirData(const QUrl &url, QQmlTypeLoader *loader)
: QQmlTypeLoader::Blob(url, QmldirFile, loader), m_import(0), m_priority(0)
{
}

const QString &QQmlQmldirData::content() const
{
    return m_content;
}

const QV4::CompiledData::Import *QQmlQmldirData::import() const
{
    return m_import;
}

void QQmlQmldirData::setImport(const QV4::CompiledData::Import *import)
{
    m_import = import;
}

int QQmlQmldirData::priority() const
{
    return m_priority;
}

void QQmlQmldirData::setPriority(int priority)
{
    m_priority = priority;
}

void QQmlQmldirData::dataReceived(const Data &data)
{
    QString error;
    m_content = QString::fromUtf8(data.readAll(&error));
    if (!error.isEmpty()) {
        setError(error);
        return;
    }
}

void QQmlQmldirData::initializeFromCachedUnit(const QQmlPrivate::CachedQmlUnit *)
{
    Q_UNIMPLEMENTED();
}

QByteArray QQmlDataBlob::Data::readAll(QString *error, qint64 *sourceTimeStamp) const
{
    Q_ASSERT(!d.isNull());
    error->clear();
    if (d.isT1()) {
        if (sourceTimeStamp)
            *sourceTimeStamp = 0;
        return *d.asT1();
    }
    QFile f(*d.asT2());
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return QByteArray();
    }
    if (sourceTimeStamp) {
        QDateTime timeStamp = QFileInfo(f).lastModified();
        // Files from the resource system do not have any time stamps, so fall back to the application
        // executable.
        if (!timeStamp.isValid())
            timeStamp = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
        *sourceTimeStamp = timeStamp.toMSecsSinceEpoch();
    }
    QByteArray data(f.size(), Qt::Uninitialized);
    if (f.read(data.data(), data.length()) != data.length()) {
        *error = f.errorString();
        return QByteArray();
    }
    return data;
}

QT_END_NAMESPACE

#include "qqmltypeloader.moc"
