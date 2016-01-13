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

#include "qqmljavascriptexpression_p.h"

#include <private/qqmlexpression_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qv4value_inl_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4script_p.h>
#include <private/qv4errorobject_p.h>
#include <private/qv4scopedvalue_p.h>

QT_BEGIN_NAMESPACE

bool QQmlDelayedError::addError(QQmlEnginePrivate *e)
{
    if (!e) return false;

    if (e->inProgressCreations == 0) return false; // Not in construction

    if (prevError) return true; // Already in error chain

    prevError = &e->erroredBindings;
    nextError = e->erroredBindings;
    e->erroredBindings = this;
    if (nextError) nextError->prevError = &nextError;

    return true;
}

void QQmlDelayedError::setErrorLocation(const QUrl &url, quint16 line, quint16 column)
{
    m_error.setUrl(url);
    m_error.setLine(line);
    m_error.setColumn(column);
}

void QQmlDelayedError::setErrorDescription(const QString &description)
{
    m_error.setDescription(description);
}

void QQmlDelayedError::setErrorObject(QObject *object)
{
    m_error.setObject(object);
}

void QQmlDelayedError::catchJavaScriptException(QV4::ExecutionContext *context)
{
    m_error = QV4::ExecutionEngine::catchExceptionAsQmlError(context);
}


QQmlJavaScriptExpression::QQmlJavaScriptExpression(VTable *v)
: m_vtable(v)
{
}

QQmlJavaScriptExpression::~QQmlJavaScriptExpression()
{
    clearGuards();
    if (m_scopeObject.isT2()) // notify DeleteWatcher of our deletion.
        m_scopeObject.asT2()->_s = 0;
}

void QQmlJavaScriptExpression::setNotifyOnValueChanged(bool v)
{
    activeGuards.setFlagValue(v);
    if (!v) clearGuards();
}

void QQmlJavaScriptExpression::resetNotifyOnValueChanged()
{
    clearGuards();
}

QV4::ReturnedValue QQmlJavaScriptExpression::evaluate(QQmlContextData *context,
                                   const QV4::ValueRef function, bool *isUndefined)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(context->engine);
    QV4::Scope scope(v4);
    QV4::ScopedCallData callData(scope, 0);

    return evaluate(context, function, callData, isUndefined);
}

QV4::ReturnedValue QQmlJavaScriptExpression::evaluate(QQmlContextData *context,
                                   const QV4::ValueRef function,
                                   QV4::CallData *callData,
                                   bool *isUndefined)
{
    Q_ASSERT(context && context->engine);

    if (function->isUndefined()) {
        if (isUndefined)
            *isUndefined = true;
        return QV4::Encode::undefined();
    }

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context->engine);

    // All code that follows must check with watcher before it accesses data members
    // incase we have been deleted.
    DeleteWatcher watcher(this);

    Q_ASSERT(notifyOnValueChanged() || activeGuards.isEmpty());
    GuardCapture capture(context->engine, this, &watcher);

    QQmlEnginePrivate::PropertyCapture *lastPropertyCapture = ep->propertyCapture;
    ep->propertyCapture = notifyOnValueChanged()?&capture:0;


    if (notifyOnValueChanged())
        capture.guards.copyAndClearPrepend(activeGuards);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::Scope scope(v4);
    QV4::ScopedValue result(scope, QV4::Primitive::undefinedValue());
    QV4::ExecutionContext *ctx = v4->currentContext();
    callData->thisObject = v4->globalObject;
    if (scopeObject()) {
        QV4::ScopedValue value(scope, QV4::QObjectWrapper::wrap(ctx->d()->engine, scopeObject()));
        if (value->isObject())
            callData->thisObject = value;
    }

    result = function->asFunctionObject()->call(callData);
    if (scope.hasException()) {
        if (watcher.wasDeleted())
            ctx->catchException(); // ignore exception
        else
            delayedError()->catchJavaScriptException(ctx);
        if (isUndefined)
            *isUndefined = true;
    } else {
        if (isUndefined)
            *isUndefined = result->isUndefined();

        if (!watcher.wasDeleted() && hasDelayedError())
            delayedError()->clearError();
    }

    if (capture.errorString) {
        for (int ii = 0; ii < capture.errorString->count(); ++ii)
            qWarning("%s", qPrintable(capture.errorString->at(ii)));
        delete capture.errorString;
        capture.errorString = 0;
    }

    while (Guard *g = capture.guards.takeFirst())
        g->Delete();

    ep->propertyCapture = lastPropertyCapture;

    return result.asReturnedValue();
}

void QQmlJavaScriptExpression::GuardCapture::captureProperty(QQmlNotifier *n)
{
    if (watcher->wasDeleted())
        return;

    Q_ASSERT(expression);
    // Try and find a matching guard
    while (!guards.isEmpty() && !guards.first()->isConnected(n))
        guards.takeFirst()->Delete();

    Guard *g = 0;
    if (!guards.isEmpty()) {
        g = guards.takeFirst();
        g->cancelNotify();
        Q_ASSERT(g->isConnected(n));
    } else {
        g = Guard::New(expression, engine);
        g->connect(n);
    }

    expression->activeGuards.prepend(g);
}

/*! \internal

    \a n is in the signal index range (see QObjectPrivate::signalIndex()).
*/
void QQmlJavaScriptExpression::GuardCapture::captureProperty(QObject *o, int c, int n)
{
    if (watcher->wasDeleted())
        return;

    Q_ASSERT(expression);
    if (n == -1) {
        if (!errorString) {
            errorString = new QStringList;
            QString preamble = QLatin1String("QQmlExpression: Expression ") +
                    expression->m_vtable->expressionIdentifier(expression) +
                    QLatin1String(" depends on non-NOTIFYable properties:");
            errorString->append(preamble);
        }

        const QMetaObject *metaObj = o->metaObject();
        QMetaProperty metaProp = metaObj->property(c);

        QString error = QLatin1String("    ") +
                QString::fromUtf8(metaObj->className()) +
                QLatin1String("::") +
                QString::fromUtf8(metaProp.name());
        errorString->append(error);
    } else {

        // Try and find a matching guard
        while (!guards.isEmpty() && !guards.first()->isConnected(o, n))
            guards.takeFirst()->Delete();

        Guard *g = 0;
        if (!guards.isEmpty()) {
            g = guards.takeFirst();
            g->cancelNotify();
            Q_ASSERT(g->isConnected(o, n));
        } else {
            g = Guard::New(expression, engine);
            g->connect(o, n, engine);
        }

        expression->activeGuards.prepend(g);
    }
}

void QQmlJavaScriptExpression::clearError()
{
    if (m_vtable.hasValue()) {
        m_vtable.value().clearError();
        m_vtable.value().removeError();
    }
}

QQmlError QQmlJavaScriptExpression::error(QQmlEngine *engine) const
{
    Q_UNUSED(engine);

    if (m_vtable.hasValue())
        return m_vtable.constValue()->error();
    else
        return QQmlError();
}

QQmlDelayedError *QQmlJavaScriptExpression::delayedError()
{
    return &m_vtable.value();
}

QV4::ReturnedValue
QQmlJavaScriptExpression::evalFunction(QQmlContextData *ctxt, QObject *scopeObject,
                                       const QString &code, const QString &filename, quint16 line,
                                       QV4::PersistentValue *qmlscope)
{
    QQmlEngine *engine = ctxt->engine;
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::ExecutionContext *ctx = v4->currentContext();
    QV4::Scope scope(v4);

    QV4::ScopedObject qmlScopeObject(scope, QV4::QmlContextWrapper::qmlScope(ep->v8engine(), ctxt, scopeObject));
    QV4::Script script(v4, qmlScopeObject, code, filename, line);
    QV4::ScopedValue result(scope);
    script.parse();
    if (!v4->hasException)
        result = script.run();
    if (v4->hasException) {
        QQmlError error = QV4::ExecutionEngine::catchExceptionAsQmlError(ctx);
        if (error.description().isEmpty())
            error.setDescription(QLatin1String("Exception occurred during function evaluation"));
        if (error.line() == -1)
            error.setLine(line);
        if (error.url().isEmpty())
            error.setUrl(QUrl::fromLocalFile(filename));
        error.setObject(scopeObject);
        ep->warning(error);
        return QV4::Encode::undefined();
    }
    if (qmlscope)
        *qmlscope = qmlScopeObject;
    return result.asReturnedValue();
}

QV4::ReturnedValue QQmlJavaScriptExpression::qmlBinding(QQmlContextData *ctxt, QObject *qmlScope,
                                                       const QString &code, const QString &filename, quint16 line,
                                                       QV4::PersistentValue *qmlscope)
{
    QQmlEngine *engine = ctxt->engine;
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::ExecutionContext *ctx = v4->currentContext();
    QV4::Scope scope(v4);

    QV4::ScopedObject qmlScopeObject(scope, QV4::QmlContextWrapper::qmlScope(ep->v8engine(), ctxt, qmlScope));
    QV4::Script script(v4, qmlScopeObject, code, filename, line);
    QV4::ScopedValue result(scope);
    script.parse();
    if (!v4->hasException)
        result = script.qmlBinding();
    if (v4->hasException) {
        QQmlError error = QV4::ExecutionEngine::catchExceptionAsQmlError(ctx);
        if (error.description().isEmpty())
            error.setDescription(QLatin1String("Exception occurred during function evaluation"));
        if (error.line() == -1)
            error.setLine(line);
        if (error.url().isEmpty())
            error.setUrl(QUrl::fromLocalFile(filename));
        error.setObject(qmlScope);
        ep->warning(error);
        return QV4::Encode::undefined();
    }
    if (qmlscope)
        *qmlscope = qmlScopeObject;
    return result.asReturnedValue();
}


void QQmlJavaScriptExpression::clearGuards()
{
    while (Guard *g = activeGuards.takeFirst())
        g->Delete();
}

void QQmlJavaScriptExpressionGuard_callback(QQmlNotifierEndpoint *e, void **)
{
    QQmlJavaScriptExpression *expression =
        static_cast<QQmlJavaScriptExpressionGuard *>(e)->expression;

    expression->m_vtable->expressionChanged(expression);
}

QT_END_NAMESPACE
