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

#ifndef QQMLENGINE_P_H
#define QQMLENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qqmlengine.h"

#include "qqmltypeloader_p.h"
#include "qqmlimport_p.h"
#include <private/qpodvector_p.h>
#include "qqml.h"
#include "qqmlvaluetype_p.h"
#include "qqmlcontext.h"
#include "qqmlcontext_p.h"
#include "qqmlexpression.h"
#include "qqmlproperty_p.h"
#include "qqmlpropertycache_p.h"
#include "qqmlmetatype_p.h"
#include "qqmldirparser_p.h"
#include <private/qintrusivelist_p.h>
#include <private/qrecyclepool_p.h>
#include <private/qfieldlist_p.h>

#include <QtCore/qlist.h>
#include <QtCore/qpair.h>
#include <QtCore/qstack.h>
#include <QtCore/qmutex.h>
#include <QtCore/qstring.h>
#include <QtCore/qthread.h>

#include <private/qobject_p.h>

#include <private/qv8engine_p.h>
#include <private/qjsengine_p.h>

QT_BEGIN_NAMESPACE

class QQmlContext;
class QQmlEngine;
class QQmlContextPrivate;
class QQmlExpression;
class QQmlImportDatabase;
class QNetworkReply;
class QNetworkAccessManager;
class QQmlNetworkAccessManagerFactory;
class QQmlAbstractBinding;
class QQmlTypeNameCache;
class QQmlComponentAttached;
class QQmlCleanup;
class QQmlDelayedError;
class QQuickWorkerScriptEngine;
class QQmlObjectCreator;
class QDir;
class QQmlIncubator;
class QQmlProfiler;

// This needs to be declared here so that the pool for it can live in QQmlEnginePrivate.
// The inline method definitions are in qqmljavascriptexpression_p.h
class QQmlJavaScriptExpressionGuard : public QQmlNotifierEndpoint
{
public:
    inline QQmlJavaScriptExpressionGuard(QQmlJavaScriptExpression *);

    static inline QQmlJavaScriptExpressionGuard *New(QQmlJavaScriptExpression *e,
                                                             QQmlEngine *engine);
    inline void Delete();

    QQmlJavaScriptExpression *expression;
    QQmlJavaScriptExpressionGuard *next;
};

class Q_QML_PRIVATE_EXPORT QQmlEnginePrivate : public QJSEnginePrivate
{
    Q_DECLARE_PUBLIC(QQmlEngine)
public:
    QQmlEnginePrivate(QQmlEngine *);
    ~QQmlEnginePrivate();

    void init();
    // No mutex protecting baseModulesUninitialized, because use outside QQmlEngine
    // is just qmlClearTypeRegistrations (which can't be called while an engine exists)
    static bool baseModulesUninitialized;

    class PropertyCapture {
    public:
        inline virtual ~PropertyCapture() {}
        virtual void captureProperty(QQmlNotifier *) = 0;
        virtual void captureProperty(QObject *, int, int) = 0;
    };

    PropertyCapture *propertyCapture;
    inline void captureProperty(QQmlNotifier *);
    inline void captureProperty(QObject *, int, int);

    QRecyclePool<QQmlJavaScriptExpressionGuard> jsExpressionGuardPool;

    QQmlContext *rootContext;
    bool isDebugging;
    bool useNewCompiler;
    QQmlProfiler *profiler;
    void enableProfiler();

    bool outputWarningsToStdErr;

    // Registered cleanup handlers
    QQmlCleanup *cleanup;

    // Bindings that have had errors during startup
    QQmlDelayedError *erroredBindings;
    int inProgressCreations;

    QV8Engine *v8engine() const { return q_func()->handle(); }
    QV4::ExecutionEngine *v4engine() const { return QV8Engine::getV4(q_func()->handle()); }

    QQuickWorkerScriptEngine *getWorkerScriptEngine();
    QQuickWorkerScriptEngine *workerScriptEngine;

    QUrl baseUrl;

    typedef QPair<QPointer<QObject>,int> FinalizeCallback;
    void registerFinalizeCallback(QObject *obj, int index);

    QQmlObjectCreator *activeObjectCreator;

    QNetworkAccessManager *createNetworkAccessManager(QObject *parent) const;
    QNetworkAccessManager *getNetworkAccessManager() const;
    mutable QNetworkAccessManager *networkAccessManager;
    mutable QQmlNetworkAccessManagerFactory *networkAccessManagerFactory;

    QHash<QString,QSharedPointer<QQmlImageProviderBase> > imageProviders;

    QQmlAbstractUrlInterceptor* urlInterceptor;

    int scarceResourcesRefCount;
    void referenceScarceResources();
    void dereferenceScarceResources();

    QQmlTypeLoader typeLoader;
    QQmlImportDatabase importDatabase;


    QString offlineStoragePath;

    mutable quint32 uniqueId;
    inline quint32 getUniqueId() const {
        return uniqueId++;
    }

    // Unfortunate workaround to avoid a circular dependency between
    // qqmlengine_p.h and qqmlincubator_p.h
    struct Incubator : public QSharedData {
        QIntrusiveListNode next;
        // Unfortunate workaround for MSVC
        QIntrusiveListNode nextWaitingFor;
    };
    QIntrusiveList<Incubator, &Incubator::next> incubatorList;
    unsigned int incubatorCount;
    QQmlIncubationController *incubationController;
    void incubate(QQmlIncubator &, QQmlContextData *);

    // These methods may be called from any thread
    inline bool isEngineThread() const;
    inline static bool isEngineThread(const QQmlEngine *);
    template<typename T>
    inline void deleteInEngineThread(T *);
    template<typename T>
    inline static void deleteInEngineThread(QQmlEngine *, T *);

    // These methods may be called from the loader thread
    inline QQmlPropertyCache *cache(QObject *obj);
    inline QQmlPropertyCache *cache(const QMetaObject *);
    inline QQmlPropertyCache *cache(QQmlType *, int, QQmlError &error);

    // These methods may be called from the loader thread
    bool isQObject(int);
    QObject *toQObject(const QVariant &, bool *ok = 0) const;
    QQmlMetaType::TypeCategory typeCategory(int) const;
    bool isList(int) const;
    int listType(int) const;
    QQmlMetaObject rawMetaObjectForType(int) const;
    QQmlMetaObject metaObjectForType(int) const;
    QQmlPropertyCache *propertyCacheForType(int);
    QQmlPropertyCache *rawPropertyCacheForType(int);
    void registerInternalCompositeType(QQmlCompiledData *);
    void unregisterInternalCompositeType(QQmlCompiledData *);

    bool isTypeLoaded(const QUrl &url) const;
    bool isScriptLoaded(const QUrl &url) const;

    inline void setDebugChangesCache(const QHash<QUrl, QByteArray> &changes);
    inline QHash<QUrl, QByteArray> debugChangesCache();

    void sendQuit();
    void warning(const QQmlError &);
    void warning(const QList<QQmlError> &);
    void warning(QQmlDelayedError *);
    static void warning(QQmlEngine *, const QQmlError &);
    static void warning(QQmlEngine *, const QList<QQmlError> &);
    static void warning(QQmlEngine *, QQmlDelayedError *);
    static void warning(QQmlEnginePrivate *, const QQmlError &);
    static void warning(QQmlEnginePrivate *, const QList<QQmlError> &);

    inline static QV8Engine *getV8Engine(QQmlEngine *e);
    inline static QV4::ExecutionEngine *getV4Engine(QQmlEngine *e);
    inline static QQmlEnginePrivate *get(QQmlEngine *e);
    inline static const QQmlEnginePrivate *get(const QQmlEngine *e);
    inline static QQmlEnginePrivate *get(QQmlContext *c);
    inline static QQmlEnginePrivate *get(QQmlContextData *c);
    inline static QQmlEngine *get(QQmlEnginePrivate *p);
    inline static QQmlEnginePrivate *get(QV4::ExecutionEngine *e);

    static void registerBaseTypes(const char *uri, int versionMajor, int versionMinor);
    static void registerQtQuick2Types(const char *uri, int versionMajor, int versionMinor);
    static void defineQtQuick2Module();

    static bool designerMode();
    static void activateDesignerMode();

    static bool qml_debugging_enabled;

    mutable QMutex networkAccessManagerMutex;
    mutable QMutex mutex;

private:
    // Locker locks the QQmlEnginePrivate data structures for read and write, if necessary.
    // Currently, locking is only necessary if the threaded loader is running concurrently.  If it is
    // either idle, or is running with the main thread blocked, no locking is necessary.  This way
    // we only pay for locking when we have to.
    // Consequently, this class should only be used to protect simple accesses or modifications of the
    // QQmlEnginePrivate structures or operations that can be guaranteed not to start activity
    // on the loader thread.
    // The Locker API is identical to QMutexLocker.  Locker reuses the QQmlEnginePrivate::mutex
    // QMutex instance and multiple Lockers are recursive in the same thread.
    class Locker
    {
    public:
        inline Locker(const QQmlEngine *);
        inline Locker(const QQmlEnginePrivate *);
        inline ~Locker();

        inline void unlock();
        inline void relock();

    private:
        const QQmlEnginePrivate *m_ep;
        quint32 m_locked:1;
    };

    // Must be called locked
    QQmlPropertyCache *createCache(const QMetaObject *);
    QQmlPropertyCache *createCache(QQmlType *, int, QQmlError &error);

    // These members must be protected by a QQmlEnginePrivate::Locker as they are required by
    // the threaded loader.  Only access them through their respective accessor methods.
    QHash<const QMetaObject *, QQmlPropertyCache *> propertyCache;
    QHash<QPair<QQmlType *, int>, QQmlPropertyCache *> typePropertyCache;
    QHash<int, int> m_qmlLists;
    QHash<int, QQmlCompiledData *> m_compositeTypes;
    QHash<QUrl, QByteArray> debugChangesHash;
    static bool s_designerMode;

    // These members is protected by the full QQmlEnginePrivate::mutex mutex
    struct Deletable { Deletable():next(0) {} virtual ~Deletable() {} Deletable *next; };
    QFieldList<Deletable, &Deletable::next> toDeleteInEngineThread;
    void doDeleteInEngineThread();
};

QQmlEnginePrivate::Locker::Locker(const QQmlEngine *e)
: m_ep(QQmlEnginePrivate::get(e))
{
    relock();
}

QQmlEnginePrivate::Locker::Locker(const QQmlEnginePrivate *e)
: m_ep(e), m_locked(false)
{
    relock();
}

QQmlEnginePrivate::Locker::~Locker()
{
    unlock();
}

void QQmlEnginePrivate::Locker::unlock()
{
    if (m_locked) {
        m_ep->mutex.unlock();
        m_locked = false;
    }
}

void QQmlEnginePrivate::Locker::relock()
{
    Q_ASSERT(!m_locked);
    if (m_ep->typeLoader.isConcurrent()) {
        m_ep->mutex.lock();
        m_locked = true;
    }
}

/*!
Returns true if the calling thread is the QQmlEngine thread.
*/
bool QQmlEnginePrivate::isEngineThread() const
{
    Q_Q(const QQmlEngine);
    return QThread::currentThread() == q->thread();
}

/*!
Returns true if the calling thread is the QQmlEngine \a engine thread.
*/
bool QQmlEnginePrivate::isEngineThread(const QQmlEngine *engine)
{
    Q_ASSERT(engine);
    return QQmlEnginePrivate::get(engine)->isEngineThread();
}

/*!
Delete \a value in the engine thread.  If the calling thread is the engine
thread, \a value will be deleted immediately.

This method should be used for *any* type that has resources that need to
be freed in the engine thread.  This is generally types that use V8 handles.
As there is some small overhead in checking the current thread, it is best
practice to check if any V8 handles actually need to be freed and delete
the instance directly if not.
*/
template<typename T>
void QQmlEnginePrivate::deleteInEngineThread(T *value)
{
    Q_Q(QQmlEngine);

    Q_ASSERT(value);
    if (isEngineThread()) {
        delete value;
    } else {
        struct I : public Deletable {
            I(T *value) : value(value) {}
            ~I() { delete value; }
            T *value;
        };
        I *i = new I(value);
        mutex.lock();
        bool wasEmpty = toDeleteInEngineThread.isEmpty();
        toDeleteInEngineThread.append(i);
        mutex.unlock();
        if (wasEmpty)
            QCoreApplication::postEvent(q, new QEvent(QEvent::User));
    }
}

/*!
Delete \a value in the \a engine thread.  If the calling thread is the engine
thread, \a value will be deleted immediately.
*/
template<typename T>
void QQmlEnginePrivate::deleteInEngineThread(QQmlEngine *engine, T *value)
{
    Q_ASSERT(engine);
    QQmlEnginePrivate::get(engine)->deleteInEngineThread<T>(value);
}

/*!
Returns a QQmlPropertyCache for \a obj if one is available.

If \a obj is null, being deleted or contains a dynamic meta object 0
is returned.

The returned cache is not referenced, so if it is to be stored, call addref().

XXX thread There is a potential future race condition in this and all the cache()
functions.  As the QQmlPropertyCache is returned unreferenced, when called
from the loader thread, it is possible that the cache will have been dereferenced
and deleted before the loader thread has a chance to use or reference it.  This
can't currently happen as the cache holds a reference to the
QQmlPropertyCache until the QQmlEngine is destroyed.
*/
QQmlPropertyCache *QQmlEnginePrivate::cache(QObject *obj)
{
    if (!obj || QObjectPrivate::get(obj)->metaObject || QObjectPrivate::get(obj)->wasDeleted)
        return 0;

    Locker locker(this);
    const QMetaObject *mo = obj->metaObject();
    QQmlPropertyCache *rv = propertyCache.value(mo);
    if (!rv) rv = createCache(mo);
    return rv;
}

/*!
Returns a QQmlPropertyCache for \a metaObject.

As the cache is persisted for the life of the engine, \a metaObject must be
a static "compile time" meta-object, or a meta-object that is otherwise known to
exist for the lifetime of the QQmlEngine.

The returned cache is not referenced, so if it is to be stored, call addref().
*/
QQmlPropertyCache *QQmlEnginePrivate::cache(const QMetaObject *metaObject)
{
    Q_ASSERT(metaObject);

    Locker locker(this);
    QQmlPropertyCache *rv = propertyCache.value(metaObject);
    if (!rv) rv = createCache(metaObject);
    return rv;
}

/*!
Returns a QQmlPropertyCache for \a type with \a minorVersion.

The returned cache is not referenced, so if it is to be stored, call addref().
*/
QQmlPropertyCache *QQmlEnginePrivate::cache(QQmlType *type, int minorVersion, QQmlError &error)
{
    Q_ASSERT(type);

    if (minorVersion == -1 || !type->containsRevisionedAttributes())
        return cache(type->metaObject());

    Locker locker(this);
    QQmlPropertyCache *rv = typePropertyCache.value(qMakePair(type, minorVersion));
    if (!rv) rv = createCache(type, minorVersion, error);
    return rv;
}

QV8Engine *QQmlEnginePrivate::getV8Engine(QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->d_func()->v8engine();
}

QV4::ExecutionEngine *QQmlEnginePrivate::getV4Engine(QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->d_func()->v4engine();
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->d_func();
}

const QQmlEnginePrivate *QQmlEnginePrivate::get(const QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->d_func();
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QQmlContext *c)
{
    return (c && c->engine()) ? QQmlEnginePrivate::get(c->engine()) : 0;
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QQmlContextData *c)
{
    return (c && c->engine) ? QQmlEnginePrivate::get(c->engine) : 0;
}

QQmlEngine *QQmlEnginePrivate::get(QQmlEnginePrivate *p)
{
    Q_ASSERT(p);

    return p->q_func();
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QV4::ExecutionEngine *e)
{
    if (!e->v8Engine)
        return 0;
    QQmlEngine *qmlEngine = e->v8Engine->engine();
    if (!qmlEngine)
        return 0;
    return get(qmlEngine);
}

void QQmlEnginePrivate::captureProperty(QQmlNotifier *n)
{
    if (propertyCapture)
        propertyCapture->captureProperty(n);
}

void QQmlEnginePrivate::captureProperty(QObject *o, int c, int n)
{
    if (propertyCapture)
        propertyCapture->captureProperty(o, c, n);
}

void QQmlEnginePrivate::setDebugChangesCache(const QHash<QUrl, QByteArray> &changes)
{
    Locker locker(this);
    foreach (const QUrl &key, changes.keys())
        debugChangesHash.insert(key, changes.value(key));
}

QHash<QUrl, QByteArray> QQmlEnginePrivate::debugChangesCache()
{
    Locker locker(this);
    return debugChangesHash;
}

QT_END_NAMESPACE

#endif // QQMLENGINE_P_H
