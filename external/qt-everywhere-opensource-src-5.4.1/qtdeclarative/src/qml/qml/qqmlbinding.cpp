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

#include "qqmlbinding_p.h"

#include "qqml.h"
#include "qqmlcontext.h"
#include "qqmlinfo.h"
#include "qqmlcompiler_p.h"
#include "qqmldata_p.h"
#include <private/qqmlprofiler_p.h>
#include <private/qqmltrace_p.h>
#include <private/qqmlexpression_p.h>
#include <private/qqmlscriptstring_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlbuiltinfunctions_p.h>

#include <QVariant>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

// Used in qqmlabstractbinding.cpp
QQmlAbstractBinding::VTable QQmlBinding_vtable = {
    QQmlAbstractBinding::default_destroy<QQmlBinding>,
    QQmlBinding::expression,
    QQmlBinding::propertyIndex,
    QQmlBinding::object,
    QQmlBinding::setEnabled,
    QQmlBinding::update,
    QQmlBinding::retargetBinding
};

QQmlBinding::Identifier QQmlBinding::Invalid = -1;

static QQmlJavaScriptExpression::VTable QQmlBinding_jsvtable = {
    QQmlBinding::expressionIdentifier,
    QQmlBinding::expressionChanged
};

QQmlBinding::QQmlBinding(const QString &str, QObject *obj, QQmlContext *ctxt)
: QQmlJavaScriptExpression(&QQmlBinding_jsvtable), QQmlAbstractBinding(Binding)
{
    setNotifyOnValueChanged(true);
    QQmlAbstractExpression::setContext(QQmlContextData::get(ctxt));
    setScopeObject(obj);

    v4function = qmlBinding(context(), obj, str, QString(), 0);
}

QQmlBinding::QQmlBinding(const QQmlScriptString &script, QObject *obj, QQmlContext *ctxt)
: QQmlJavaScriptExpression(&QQmlBinding_jsvtable), QQmlAbstractBinding(Binding)
{
    if (ctxt && !ctxt->isValid())
        return;

    const QQmlScriptStringPrivate *scriptPrivate = script.d.data();
    if (!ctxt && (!scriptPrivate->context || !scriptPrivate->context->isValid()))
        return;

    QString url;
    QV4::Function *runtimeFunction = 0;

    QQmlContextData *ctxtdata = QQmlContextData::get(scriptPrivate->context);
    QQmlEnginePrivate *engine = QQmlEnginePrivate::get(scriptPrivate->context->engine());
    if (engine && ctxtdata && !ctxtdata->url.isEmpty() && ctxtdata->typeCompilationUnit) {
        url = ctxtdata->url.toString();
        if (scriptPrivate->bindingId != QQmlBinding::Invalid)
            runtimeFunction = ctxtdata->typeCompilationUnit->runtimeFunctions.at(scriptPrivate->bindingId);
    }

    setNotifyOnValueChanged(true);
    QQmlAbstractExpression::setContext(QQmlContextData::get(ctxt ? ctxt : scriptPrivate->context));
    setScopeObject(obj ? obj : scriptPrivate->scope);

    if (runtimeFunction) {
        v4function = QV4::QmlBindingWrapper::createQmlCallableForFunction(ctxtdata, scopeObject(), runtimeFunction);
    } else {
        QString code = scriptPrivate->script;
        v4function = qmlBinding(context(), scopeObject(), code, url, scriptPrivate->lineNumber);
    }
}

QQmlBinding::QQmlBinding(const QString &str, QObject *obj, QQmlContextData *ctxt)
: QQmlJavaScriptExpression(&QQmlBinding_jsvtable), QQmlAbstractBinding(Binding)
{
    setNotifyOnValueChanged(true);
    QQmlAbstractExpression::setContext(ctxt);
    setScopeObject(obj);

    v4function = qmlBinding(ctxt, obj, str, QString(), 0);
}

QQmlBinding::QQmlBinding(const QString &str, QObject *obj,
                         QQmlContextData *ctxt,
                         const QString &url, quint16 lineNumber, quint16 columnNumber)
: QQmlJavaScriptExpression(&QQmlBinding_jsvtable), QQmlAbstractBinding(Binding)
{
    Q_UNUSED(columnNumber);
    setNotifyOnValueChanged(true);
    QQmlAbstractExpression::setContext(ctxt);
    setScopeObject(obj);

    v4function = qmlBinding(ctxt, obj, str, url, lineNumber);
}

QQmlBinding::QQmlBinding(const QV4::ValueRef functionPtr, QObject *obj, QQmlContextData *ctxt)
: QQmlJavaScriptExpression(&QQmlBinding_jsvtable), QQmlAbstractBinding(Binding)
{
    setNotifyOnValueChanged(true);
    QQmlAbstractExpression::setContext(ctxt);
    setScopeObject(obj);

    v4function = functionPtr;
}

QQmlBinding::~QQmlBinding()
{
}

void QQmlBinding::setNotifyOnValueChanged(bool v)
{
    QQmlJavaScriptExpression::setNotifyOnValueChanged(v);
}

void QQmlBinding::update(QQmlPropertyPrivate::WriteFlags flags)
{
    if (!enabledFlag() || !context() || !context()->isValid())
        return;

    // Check that the target has not been deleted
    if (QQmlData::wasDeleted(object()))
        return;

    QString url;
    quint16 lineNumber;
    quint16 columnNumber;

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context()->engine);
    QV4::Scope scope(ep->v4engine());
    QV4::ScopedFunctionObject f(scope, v4function.value());
    Q_ASSERT(f);
    if (f->bindingKeyFlag()) {
        Q_ASSERT(f->as<QV4::QQmlBindingFunction>());
        QQmlSourceLocation loc = static_cast<QV4::QQmlBindingFunction *>(f.getPointer())->d()->bindingLocation;
        url = loc.sourceFile;
        lineNumber = loc.line;
        columnNumber = loc.column;
    } else {
        QV4::Function *function = f->asFunctionObject()->function();
        Q_ASSERT(function);

        url = function->sourceFile();
        lineNumber = function->compiledFunction->location.line;
        columnNumber = function->compiledFunction->location.column;
    }

    int lineNo = qmlSourceCoordinate(lineNumber);
    int columnNo = qmlSourceCoordinate(columnNumber);

    QQmlTrace trace("General Binding Update");
    trace.addDetail("URL", url);
    trace.addDetail("Line", lineNo);
    trace.addDetail("Column", columnNo);

    if (!updatingFlag()) {
        QQmlBindingProfiler prof(ep->profiler, url, lineNo, columnNo);
        setUpdatingFlag(true);

        QQmlAbstractExpression::DeleteWatcher watcher(this);

        if (m_core.propType == qMetaTypeId<QQmlBinding *>()) {

            int idx = m_core.coreIndex;
            Q_ASSERT(idx != -1);

            QQmlBinding *t = this;
            int status = -1;
            void *a[] = { &t, 0, &status, &flags };
            QMetaObject::metacall(*m_coreObject, QMetaObject::WriteProperty, idx, a);

        } else {
            ep->referenceScarceResources();

            bool isUndefined = false;

            QV4::ScopedValue result(scope, QQmlJavaScriptExpression::evaluate(context(), f, &isUndefined));

            trace.event("writing binding result");

            bool needsErrorLocationData = false;
            if (!watcher.wasDeleted() && !hasError())
                needsErrorLocationData = !QQmlPropertyPrivate::writeBinding(*m_coreObject, m_core, context(),
                                                                    this, result, isUndefined, flags);

            if (!watcher.wasDeleted()) {

                if (needsErrorLocationData)
                    delayedError()->setErrorLocation(QUrl(url), lineNumber, columnNumber);

                if (hasError()) {
                    if (!delayedError()->addError(ep)) ep->warning(this->error(context()->engine));
                } else {
                    clearError();
                }

            }

            ep->dereferenceScarceResources();
        }

        if (!watcher.wasDeleted())
            setUpdatingFlag(false);
    } else {
        QQmlProperty p = property();
        QQmlAbstractBinding::printBindingLoopError(p);
    }
}

QVariant QQmlBinding::evaluate()
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context()->engine);
    QV4::Scope scope(ep->v4engine());
    ep->referenceScarceResources();

    bool isUndefined = false;

    QV4::ScopedValue f(scope, v4function.value());
    QV4::ScopedValue result(scope, QQmlJavaScriptExpression::evaluate(context(), f, &isUndefined));

    ep->dereferenceScarceResources();

    return ep->v8engine()->toVariant(result, qMetaTypeId<QList<QObject*> >());
}

QString QQmlBinding::expressionIdentifier(QQmlJavaScriptExpression *e)
{
    QQmlBinding *This = static_cast<QQmlBinding *>(e);

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(This->context()->engine);
    QV4::Scope scope(ep->v4engine());
    QV4::ScopedValue f(scope, This->v4function.value());
    QV4::Function *function = f->asFunctionObject()->function();

    QString url = function->sourceFile();
    quint16 lineNumber = function->compiledFunction->location.line;
    quint16 columnNumber = function->compiledFunction->location.column;

    return url + QLatin1Char(':') + QString::number(lineNumber) + QLatin1Char(':') + QString::number(columnNumber);
}

void QQmlBinding::expressionChanged(QQmlJavaScriptExpression *e)
{
    QQmlBinding *This = static_cast<QQmlBinding *>(e);
    This->update();
}

void QQmlBinding::refresh()
{
    update();
}

QString QQmlBinding::expression(const QQmlAbstractBinding *This)
{
    return static_cast<const QQmlBinding *>(This)->expression();
}

int QQmlBinding::propertyIndex(const QQmlAbstractBinding *This)
{
    return static_cast<const QQmlBinding *>(This)->propertyIndex();
}

QObject *QQmlBinding::object(const QQmlAbstractBinding *This)
{
    return static_cast<const QQmlBinding *>(This)->object();
}

void QQmlBinding::setEnabled(QQmlAbstractBinding *This, bool e, QQmlPropertyPrivate::WriteFlags f)
{
    static_cast<QQmlBinding *>(This)->setEnabled(e, f);
}

void QQmlBinding::update(QQmlAbstractBinding *This , QQmlPropertyPrivate::WriteFlags f)
{
    static_cast<QQmlBinding *>(This)->update(f);
}

void QQmlBinding::retargetBinding(QQmlAbstractBinding *This, QObject *o, int i)
{
    static_cast<QQmlBinding *>(This)->retargetBinding(o, i);
}

void QQmlBinding::setEnabled(bool e, QQmlPropertyPrivate::WriteFlags flags)
{
    setEnabledFlag(e);
    setNotifyOnValueChanged(e);

    if (e)
        update(flags);
}

QString QQmlBinding::expression() const
{
    QV4::Scope scope(QQmlEnginePrivate::get(context()->engine)->v4engine());
    QV4::ScopedValue v(scope, v4function.value());
    return v->toQStringNoThrow();
}

QObject *QQmlBinding::object() const
{
    if (m_coreObject.hasValue()) return m_coreObject.constValue()->target;
    else return *m_coreObject;
}

int QQmlBinding::propertyIndex() const
{
    if (m_coreObject.hasValue()) return m_coreObject.constValue()->targetProperty;
    else return m_core.encodedIndex();
}

void QQmlBinding::retargetBinding(QObject *t, int i)
{
    m_coreObject.value().target = t;
    m_coreObject.value().targetProperty = i;
}

void QQmlBinding::setTarget(const QQmlProperty &prop)
{
    setTarget(prop.object(), QQmlPropertyPrivate::get(prop)->core,
              QQmlPropertyPrivate::get(prop)->context);
}

void QQmlBinding::setTarget(QObject *object, const QQmlPropertyData &core, QQmlContextData *ctxt)
{
    m_coreObject = object;
    m_core = core;
    m_ctxt = ctxt;
}

QQmlProperty QQmlBinding::property() const
{
    return QQmlPropertyPrivate::restore(object(), m_core, *m_ctxt);
}

QT_END_NAMESPACE
