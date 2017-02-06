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

#include "qqmldelayedcallqueue_p.h"
#include <private/qv8engine_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmljavascriptexpression_p.h>
#include <private/qv4value_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <private/qqmlcontextwrapper_p.h>

#include <QQmlError>

QT_BEGIN_NAMESPACE

//
// struct QQmlDelayedCallQueue::DelayedFunctionCall
//

void QQmlDelayedCallQueue::DelayedFunctionCall::execute(QV4::ExecutionEngine *engine) const
{
    if (!m_guarded ||
            (!m_objectGuard.isNull() &&
             !QQmlData::wasDeleted(m_objectGuard) &&
             QQmlData::get(m_objectGuard) &&
             !QQmlData::get(m_objectGuard)->isQueuedForDeletion)) {

        QV4::Scope scope(engine);

        QV4::ArrayObject *array = m_args.as<QV4::ArrayObject>();
        const int argCount = array ? array->getLength() : 0;
        QV4::ScopedCallData callData(scope, argCount);
        callData->thisObject = QV4::Encode::undefined();

        for (int i = 0; i < argCount; i++) {
            callData->args[i] = array->getIndexed(i);
        }

        const QV4::FunctionObject *callback = m_function.as<QV4::FunctionObject>();
        Q_ASSERT(callback);
        callback->call(scope, callData);

        if (scope.engine->hasException) {
            QQmlError error = scope.engine->catchExceptionAsQmlError();
            error.setDescription(error.description() + QLatin1String(" (exception occurred during delayed function evaluation)"));
            QQmlEnginePrivate::warning(QQmlEnginePrivate::get(scope.engine->qmlEngine()), error);
        }
    }
}

//
// class QQmlDelayedCallQueue
//

QQmlDelayedCallQueue::QQmlDelayedCallQueue()
    : QObject(0), m_engine(0), m_callbackOutstanding(false)
{
}

QQmlDelayedCallQueue::~QQmlDelayedCallQueue()
{
}

void QQmlDelayedCallQueue::init(QV4::ExecutionEngine* engine)
{
    m_engine = engine;

    const QMetaObject &metaObject = QQmlDelayedCallQueue::staticMetaObject;
    int methodIndex = metaObject.indexOfSlot("ticked()");
    m_tickedMethod = metaObject.method(methodIndex);
}

QV4::ReturnedValue QQmlDelayedCallQueue::addUniquelyAndExecuteLater(QV4::CallContext *ctx)
{
    const QV4::CallData *callData = ctx->d()->callData;

    if (callData->argc == 0)
        V4THROW_ERROR("Qt.callLater: no arguments given");

    const QV4::FunctionObject *func = callData->args[0].as<QV4::FunctionObject>();

    if (!func)
        V4THROW_ERROR("Qt.callLater: first argument not a function or signal");

    QPair<QObject *, int> functionData = QV4::QObjectMethod::extractQtMethod(func);

    QVector<DelayedFunctionCall>::Iterator iter;
    if (functionData.second != -1) {
        // This is a QObject function wrapper
        iter = m_delayedFunctionCalls.begin();
        while (iter != m_delayedFunctionCalls.end()) {
            DelayedFunctionCall& dfc = *iter;
            QPair<QObject *, int> storedFunctionData = QV4::QObjectMethod::extractQtMethod(dfc.m_function.as<QV4::FunctionObject>());
            if (storedFunctionData == functionData) {
                break; // Already stored!
            }
            ++iter;
        }
    } else {
        // This is a JavaScript function (dynamic slot on VMEMO)
        iter = m_delayedFunctionCalls.begin();
        while (iter != m_delayedFunctionCalls.end()) {
            DelayedFunctionCall& dfc = *iter;
            if (callData->argument(0) == dfc.m_function.value()) {
                break; // Already stored!
            }
            ++iter;
        }
    }

    const bool functionAlreadyStored = (iter != m_delayedFunctionCalls.end());
    if (functionAlreadyStored) {
        DelayedFunctionCall dfc = *iter;
        m_delayedFunctionCalls.erase(iter);
        m_delayedFunctionCalls.append(dfc);
    } else {
        m_delayedFunctionCalls.append(QV4::PersistentValue(m_engine, callData->argument(0)));
    }

    DelayedFunctionCall& dfc = m_delayedFunctionCalls.last();
    if (dfc.m_objectGuard.isNull()) {
        if (functionData.second != -1) {
            // if it's a qobject function wrapper, guard against qobject deletion
            dfc.m_objectGuard = QQmlGuard<QObject>(functionData.first);
            dfc.m_guarded = true;
        } else if (func->scope()->type == QV4::Heap::ExecutionContext::Type_QmlContext) {
            QV4::QmlContext::Data *g = static_cast<QV4::QmlContext::Data *>(func->scope());
            Q_ASSERT(g->qml->scopeObject);
            dfc.m_objectGuard = QQmlGuard<QObject>(g->qml->scopeObject);
            dfc.m_guarded = true;
        }
    }
    storeAnyArguments(dfc, callData, 1, m_engine);

    if (!m_callbackOutstanding) {
        m_tickedMethod.invoke(this, Qt::QueuedConnection);
        m_callbackOutstanding = true;
    }
    return QV4::Encode::undefined();
}

void QQmlDelayedCallQueue::storeAnyArguments(DelayedFunctionCall &dfc, const QV4::CallData *callData, int offset, QV4::ExecutionEngine *engine)
{
    const int length = callData->argc - offset;
    if (length == 0) {
        dfc.m_args.clear();
        return;
    }
    QV4::Scope scope(engine);
    QV4::ScopedArrayObject array(scope, engine->newArrayObject(length));
    int i = 0;
    for (int j = offset; j < callData->argc; ++i, ++j) {
        array->putIndexed(i, callData->args[j]);
    }
    dfc.m_args.set(engine, array);
}

void QQmlDelayedCallQueue::executeAllExpired_Later()
{
    // Make a local copy of the list and clear m_delayedFunctionCalls
    // This ensures correct behavior in the case of recursive calls to Qt.callLater()
    QVector<DelayedFunctionCall> delayedCalls = m_delayedFunctionCalls;
    m_delayedFunctionCalls.clear();

    QVector<DelayedFunctionCall>::Iterator iter = delayedCalls.begin();
    while (iter != delayedCalls.end()) {
        DelayedFunctionCall& dfc = *iter;
        dfc.execute(m_engine);
        ++iter;
    }
}

void QQmlDelayedCallQueue::ticked()
{
    m_callbackOutstanding = false;
    executeAllExpired_Later();
}

QT_END_NAMESPACE
