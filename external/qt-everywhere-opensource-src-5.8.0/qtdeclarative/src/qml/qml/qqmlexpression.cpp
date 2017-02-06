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

#include "qqmlexpression.h"
#include "qqmlexpression_p.h"

#include "qqmlglobal_p.h"
#include "qqmlengine_p.h"
#include "qqmlcontext_p.h"
#include "qqmlscriptstring_p.h"
#include "qqmlbinding_p.h"
#include <private/qv8engine_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QQmlExpressionPrivate::QQmlExpressionPrivate()
: QQmlJavaScriptExpression(),
  expressionFunctionValid(true),
  line(0), column(0)
{
}

QQmlExpressionPrivate::~QQmlExpressionPrivate()
{
}

void QQmlExpressionPrivate::init(QQmlContextData *ctxt, const QString &expr, QObject *me)
{
    expression = expr;

    QQmlJavaScriptExpression::setContext(ctxt);
    setScopeObject(me);
    expressionFunctionValid = false;
}

void QQmlExpressionPrivate::init(QQmlContextData *ctxt, QV4::Function *runtimeFunction, QObject *me)
{
    expressionFunctionValid = true;
    QV4::ExecutionEngine *engine = QQmlEnginePrivate::getV4Engine(ctxt->engine);
    m_function.set(engine, QV4::FunctionObject::createQmlFunction(ctxt, me, runtimeFunction));

    QQmlJavaScriptExpression::setContext(ctxt);
    setScopeObject(me);
}

/*!
    \class QQmlExpression
    \since 5.0
    \inmodule QtQml
    \brief The QQmlExpression class evaluates JavaScript in a QML context.

    For example, given a file \c main.qml like this:

    \qml
    import QtQuick 2.0

    Item {
        width: 200; height: 200
    }
    \endqml

    The following code evaluates a JavaScript expression in the context of the
    above QML:

    \code
    QQmlEngine *engine = new QQmlEngine;
    QQmlComponent component(engine, QUrl::fromLocalFile("main.qml"));

    QObject *myObject = component.create();
    QQmlExpression *expr = new QQmlExpression(engine->rootContext(), myObject, "width * 2");
    int result = expr->evaluate().toInt();  // result = 400
    \endcode

    Note that the \l {Qt Quick 1} version is called QDeclarativeExpression.
*/

/*!
    Create an invalid QQmlExpression.

    As the expression will not have an associated QQmlContext, this will be a
    null expression object and its value will always be an invalid QVariant.
 */
QQmlExpression::QQmlExpression()
: QObject(*new QQmlExpressionPrivate, 0)
{
}

/*!
    Create a QQmlExpression object that is a child of \a parent.

    The \a script provides the expression to be evaluated, the context to evaluate it in,
    and the scope object to evaluate it with. If provided, \a ctxt and \a scope will override
    the context and scope object provided by \a script.

    \sa QQmlScriptString
*/
QQmlExpression::QQmlExpression(const QQmlScriptString &script, QQmlContext *ctxt, QObject *scope, QObject *parent)
: QObject(*new QQmlExpressionPrivate, parent)
{
    Q_D(QQmlExpression);
    if (ctxt && !ctxt->isValid())
        return;

    const QQmlScriptStringPrivate *scriptPrivate = script.d.data();
    if (!ctxt && (!scriptPrivate->context || !scriptPrivate->context->isValid()))
        return;

    QQmlContextData *evalCtxtData = QQmlContextData::get(ctxt ? ctxt : scriptPrivate->context);
    QObject *scopeObject = scope ? scope : scriptPrivate->scope;
    QV4::Function *runtimeFunction = 0;

    if (scriptPrivate->context) {
        QQmlContextData *ctxtdata = QQmlContextData::get(scriptPrivate->context);
        QQmlEnginePrivate *engine = QQmlEnginePrivate::get(scriptPrivate->context->engine());
        if (engine && ctxtdata && !ctxtdata->urlString().isEmpty() && ctxtdata->typeCompilationUnit) {
            d->url = ctxtdata->urlString();
            d->line = scriptPrivate->lineNumber;
            d->column = scriptPrivate->columnNumber;

            if (scriptPrivate->bindingId != QQmlBinding::Invalid)
                runtimeFunction = ctxtdata->typeCompilationUnit->runtimeFunctions.at(scriptPrivate->bindingId);
        }
    }

    if (runtimeFunction) {
        d->expression = scriptPrivate->script;
        d->init(evalCtxtData, runtimeFunction, scopeObject);
    } else
        d->init(evalCtxtData, scriptPrivate->script, scopeObject);
}

/*!
    Create a QQmlExpression object that is a child of \a parent.

    The \a expression JavaScript will be executed in the \a ctxt QQmlContext.
    If specified, the \a scope object's properties will also be in scope during
    the expression's execution.
*/
QQmlExpression::QQmlExpression(QQmlContext *ctxt,
                                               QObject *scope,
                                               const QString &expression,
                                               QObject *parent)
: QObject(*new QQmlExpressionPrivate, parent)
{
    Q_D(QQmlExpression);
    d->init(QQmlContextData::get(ctxt), expression, scope);
}

/*!
    \internal
*/
QQmlExpression::QQmlExpression(QQmlContextData *ctxt, QObject *scope,
                                               const QString &expression)
: QObject(*new QQmlExpressionPrivate, 0)
{
    Q_D(QQmlExpression);
    d->init(ctxt, expression, scope);
}

/*!
    Destroy the QQmlExpression instance.
*/
QQmlExpression::~QQmlExpression()
{
}

/*!
    Returns the QQmlEngine this expression is associated with, or 0 if there
    is no association or the QQmlEngine has been destroyed.
*/
QQmlEngine *QQmlExpression::engine() const
{
    Q_D(const QQmlExpression);
    return d->context()?d->context()->engine:0;
}

/*!
    Returns the QQmlContext this expression is associated with, or 0 if there
    is no association or the QQmlContext has been destroyed.
*/
QQmlContext *QQmlExpression::context() const
{
    Q_D(const QQmlExpression);
    QQmlContextData *data = d->context();
    return data?data->asQQmlContext():0;
}

/*!
    Returns the expression string.
*/
QString QQmlExpression::expression() const
{
    Q_D(const QQmlExpression);
    return d->expression;
}

/*!
    Set the expression to \a expression.
*/
void QQmlExpression::setExpression(const QString &expression)
{
    Q_D(QQmlExpression);

    d->resetNotifyOnValueChanged();
    d->expression = expression;
    d->expressionFunctionValid = false;
}

// Must be called with a valid handle scope
void QQmlExpressionPrivate::v4value(bool *isUndefined, QV4::Scope &scope)
{
    if (!expressionFunctionValid) {
        createQmlBinding(context(), scopeObject(), expression, url, line);
        expressionFunctionValid = true;
    }

    QV4::ScopedCallData callData(scope);
    evaluate(callData, isUndefined, scope);
}

QVariant QQmlExpressionPrivate::value(bool *isUndefined)
{
    Q_Q(QQmlExpression);

    if (!context() || !context()->isValid()) {
        qWarning("QQmlExpression: Attempted to evaluate an expression in an invalid context");
        return QVariant();
    }

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(q->engine());
    QVariant rv;

    ep->referenceScarceResources(); // "hold" scarce resources in memory during evaluation.

    {
        QV4::Scope scope(QV8Engine::getV4(ep->v8engine()));
        v4value(isUndefined, scope);
        if (!hasError())
            rv = scope.engine->toVariant(scope.result, -1);
    }

    ep->dereferenceScarceResources(); // "release" scarce resources if top-level expression evaluation is complete.

    return rv;
}

/*!
    Evaulates the expression, returning the result of the evaluation,
    or an invalid QVariant if the expression is invalid or has an error.

    \a valueIsUndefined is set to true if the expression resulted in an
    undefined value.

    \sa hasError(), error()
*/
QVariant QQmlExpression::evaluate(bool *valueIsUndefined)
{
    Q_D(QQmlExpression);
    return d->value(valueIsUndefined);
}

/*!
Returns true if the valueChanged() signal is emitted when the expression's evaluated
value changes.
*/
bool QQmlExpression::notifyOnValueChanged() const
{
    Q_D(const QQmlExpression);
    return d->notifyOnValueChanged();
}

/*!
  Sets whether the valueChanged() signal is emitted when the
  expression's evaluated value changes.

  If \a notifyOnChange is true, the QQmlExpression will
  monitor properties involved in the expression's evaluation, and emit
  QQmlExpression::valueChanged() if they have changed.  This
  allows an application to ensure that any value associated with the
  result of the expression remains up to date.

  If \a notifyOnChange is false (default), the QQmlExpression
  will not montitor properties involved in the expression's
  evaluation, and QQmlExpression::valueChanged() will never be
  emitted.  This is more efficient if an application wants a "one off"
  evaluation of the expression.
*/
void QQmlExpression::setNotifyOnValueChanged(bool notifyOnChange)
{
    Q_D(QQmlExpression);
    d->setNotifyOnValueChanged(notifyOnChange);
}

/*!
    Returns the source file URL for this expression.  The source location must
    have been previously set by calling setSourceLocation().
*/
QString QQmlExpression::sourceFile() const
{
    Q_D(const QQmlExpression);
    return d->url;
}

/*!
    Returns the source file line number for this expression.  The source location
    must have been previously set by calling setSourceLocation().
*/
int QQmlExpression::lineNumber() const
{
    Q_D(const QQmlExpression);
    return qmlSourceCoordinate(d->line);
}

/*!
    Returns the source file column number for this expression.  The source location
    must have been previously set by calling setSourceLocation().
*/
int QQmlExpression::columnNumber() const
{
    Q_D(const QQmlExpression);
    return qmlSourceCoordinate(d->column);
}

/*!
    Set the location of this expression to \a line and \a column of \a url. This information
    is used by the script engine.
*/
void QQmlExpression::setSourceLocation(const QString &url, int line, int column)
{
    Q_D(QQmlExpression);
    d->url = url;
    d->line = qmlSourceCoordinate(line);
    d->column = qmlSourceCoordinate(column);
}

/*!
    Returns the expression's scope object, if provided, otherwise 0.

    In addition to data provided by the expression's QQmlContext, the scope
    object's properties are also in scope during the expression's evaluation.
*/
QObject *QQmlExpression::scopeObject() const
{
    Q_D(const QQmlExpression);
    return d->scopeObject();
}

/*!
    Returns true if the last call to evaluate() resulted in an error,
    otherwise false.

    \sa error(), clearError()
*/
bool QQmlExpression::hasError() const
{
    Q_D(const QQmlExpression);
    return d->hasError();
}

/*!
    Clear any expression errors.  Calls to hasError() following this will
    return false.

    \sa hasError(), error()
*/
void QQmlExpression::clearError()
{
    Q_D(QQmlExpression);
    d->clearError();
}

/*!
    Return any error from the last call to evaluate().  If there was no error,
    this returns an invalid QQmlError instance.

    \sa hasError(), clearError()
*/

QQmlError QQmlExpression::error() const
{
    Q_D(const QQmlExpression);
    return d->error(engine());
}

/*!
    \fn void QQmlExpression::valueChanged()

    Emitted each time the expression value changes from the last time it was
    evaluated.  The expression must have been evaluated at least once (by
    calling QQmlExpression::evaluate()) before this signal will be emitted.
*/

void QQmlExpressionPrivate::expressionChanged()
{
    Q_Q(QQmlExpression);
    emit q->valueChanged();
}

QString QQmlExpressionPrivate::expressionIdentifier()
{
    return QLatin1Char('"') + expression + QLatin1Char('"');
}

QT_END_NAMESPACE

#include <moc_qqmlexpression.cpp>
