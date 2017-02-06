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

#ifndef QQMLV8ENGINE_P_H
#define QQMLV8ENGINE_P_H

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

#include <QtCore/qglobal.h>
#include <QtCore/qvariant.h>
#include <QtCore/qset.h>
#include <QtCore/qmutex.h>
#include <QtCore/qstack.h>
#include <QtCore/qstringlist.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThreadStorage>

#include <QtQml/qjsengine.h>
#include "private/qintrusivelist_p.h"


#include <private/qv4value_p.h>
#include <private/qv4identifier_p.h>
#include <private/qv4context_p.h>
#include <private/qqmldelayedcallqueue_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
    struct ArrayObject;
    struct ExecutionEngine;
    struct QObjectMethod;
}

#define V4THROW_ERROR(string) \
    return ctx->engine()->throwError(QString::fromUtf8(string));

#define V4THROW_TYPE(string) \
    return ctx->engine()->throwTypeError(QStringLiteral(string));

#define V4_DEFINE_EXTENSION(dataclass, datafunction) \
    static inline dataclass *datafunction(QV4::ExecutionEngine *engine) \
    { \
        static int extensionId = -1; \
        if (extensionId == -1) { \
            QV8Engine::registrationMutex()->lock(); \
            if (extensionId == -1) \
                extensionId = QV8Engine::registerExtension(); \
            QV8Engine::registrationMutex()->unlock(); \
        } \
        dataclass *rv = (dataclass *)engine->v8Engine->extensionData(extensionId); \
        if (!rv) { \
            rv = new dataclass(engine); \
            engine->v8Engine->setExtensionData(extensionId, rv); \
        } \
        return rv; \
    } \

// Used to allow a QObject method take and return raw V4 handles without having to expose
// 48 in the public API.
// Use like this:
//     class MyClass : public QObject {
//         Q_OBJECT
//         ...
//         Q_INVOKABLE void myMethod(QQmlV4Function*);
//     };
// The QQmlV8Function - and consequently the arguments and return value - only remains
// valid during the call.  If the return value isn't set within myMethod(), the will return
// undefined.

class QQmlV4Function
{
public:
    int length() const { return callData->argc; }
    QV4::ReturnedValue operator[](int idx) { return (idx < callData->argc ? callData->args[idx].asReturnedValue() : QV4::Encode::undefined()); }
    void setReturnValue(QV4::ReturnedValue rv) { *retVal = rv; }
    QV4::ExecutionEngine *v4engine() const { return e; }
private:
    friend struct QV4::QObjectMethod;
    QQmlV4Function();
    QQmlV4Function(const QQmlV4Function &);
    QQmlV4Function &operator=(const QQmlV4Function &);

    QQmlV4Function(QV4::CallData *callData, QV4::Value *retVal, QV4::ExecutionEngine *e)
        : callData(callData), retVal(retVal), e(e)
    {
        callData->thisObject = QV4::Encode::undefined();
    }

    QV4::CallData *callData;
    QV4::Value *retVal;
    QV4::ExecutionEngine *e;
};

class Q_QML_PRIVATE_EXPORT QQmlV4Handle
{
public:
    QQmlV4Handle() : d(QV4::Encode::undefined()) {}
    explicit QQmlV4Handle(const QV4::Value &v) : d(v.asReturnedValue()) {}
    explicit QQmlV4Handle(QV4::ReturnedValue v) : d(v) {}

    operator QV4::ReturnedValue() const { return d; }

private:
    quint64 d;
};

class QObject;
class QQmlEngine;
class QNetworkAccessManager;
class QQmlContextData;

class Q_QML_PRIVATE_EXPORT QV8Engine
{
    friend class QJSEngine;
public:
    static QV8Engine* get(QJSEngine* q) { Q_ASSERT(q); return q->handle(); }
//    static QJSEngine* get(QV8Engine* d) { Q_ASSERT(d); return d->q; }
    static QV4::ExecutionEngine *getV4(QJSEngine *q) { return q->handle()->m_v4Engine; }
    static QV4::ExecutionEngine *getV4(QV8Engine *d) { return d->m_v4Engine; }

    QV8Engine(QJSEngine* qq);
    virtual ~QV8Engine();

    // This enum should be in sync with QQmlEngine::ObjectOwnership
    enum ObjectOwnership { CppOwnership, JavaScriptOwnership };

    struct Deletable {
        virtual ~Deletable() {}
    };

    void initQmlGlobalObject();
    void setEngine(QQmlEngine *engine);
    QQmlEngine *engine() { return m_engine; }
    QJSEngine *publicEngine() { return q; }
    QV4::ReturnedValue global();
    QQmlDelayedCallQueue *delayedCallQueue() { return &m_delayedCallQueue; }

    void *xmlHttpRequestData() { return m_xmlHttpRequestData; }

    Deletable *listModelData() { return m_listModelData; }
    void setListModelData(Deletable *d) { if (m_listModelData) delete m_listModelData; m_listModelData = d; }

    void freezeObject(const QV4::Value &value);

#if QT_CONFIG(qml_network)
    // Return the network access manager for this engine.  By default this returns the network
    // access manager of the QQmlEngine.  It is overridden in the case of a threaded v8
    // instance (like in WorkerScript).
    virtual QNetworkAccessManager *networkAccessManager();
#endif

    // Return the list of illegal id names (the names of the properties on the global object)
    const QSet<QString> &illegalNames() const;

    static QMutex *registrationMutex();
    static int registerExtension();

    inline Deletable *extensionData(int) const;
    void setExtensionData(int, Deletable *);

public:
    // used for console.time(), console.timeEnd()
    void startTimer(const QString &timerName);
    qint64 stopTimer(const QString &timerName, bool *wasRunning);

    // used for console.count()
    int consoleCountHelper(const QString &file, quint16 line, quint16 column);

protected:
    QJSEngine* q;
    QQmlEngine *m_engine;
    QQmlDelayedCallQueue m_delayedCallQueue;

    QV4::ExecutionEngine *m_v4Engine;

    void *m_xmlHttpRequestData;

    QVector<Deletable *> m_extensionData;
    Deletable *m_listModelData;

    QSet<QString> m_illegalNames;

    QElapsedTimer m_time;
    QHash<QString, qint64> m_startedTimers;

    QHash<QString, quint32> m_consoleCount;

    void initializeGlobal();

private:
    Q_DISABLE_COPY(QV8Engine)
};

inline QV8Engine::Deletable *QV8Engine::extensionData(int index) const
{
    if (index < m_extensionData.count())
        return m_extensionData[index];
    else
        return 0;
}


QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlV4Handle)

#endif // QQMLV8ENGINE_P_H
