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

#include "qqmlcontextwrapper_p.h"
#include <private/qv8engine_p.h>

#include <private/qqmlengine_p.h>
#include <private/qqmlcontext_p.h>

#include <private/qv4engine_p.h>
#include <private/qv4value_inl_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4mm_p.h>
#include <private/qv4function_p.h>
#include <private/qv4compileddata_p.h>
#include <private/qqmltypewrapper_p.h>
#include <private/qqmllistwrapper_p.h>
#include <private/qjsvalue_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QmlContextWrapper);

QmlContextWrapper::Data::Data(QV8Engine *engine, QQmlContextData *context, QObject *scopeObject, bool ownsContext)
    : Object::Data(QV8Engine::getV4(engine))
    , readOnly(true)
    , ownsContext(ownsContext)
    , isNullWrapper(false)
    , context(context)
    , scopeObject(scopeObject)
{
    setVTable(staticVTable());
}

QmlContextWrapper::Data::~Data()
{
    if (context && ownsContext)
        context->destroy();
}

ReturnedValue QmlContextWrapper::qmlScope(QV8Engine *v8, QQmlContextData *ctxt, QObject *scope)
{
    ExecutionEngine *v4 = QV8Engine::getV4(v8);
    Scope valueScope(v4);

    Scoped<QmlContextWrapper> w(valueScope, v4->memoryManager->alloc<QmlContextWrapper>(v8, ctxt, scope));
    return w.asReturnedValue();
}

ReturnedValue QmlContextWrapper::urlScope(QV8Engine *v8, const QUrl &url)
{
    ExecutionEngine *v4 = QV8Engine::getV4(v8);
    Scope scope(v4);

    QQmlContextData *context = new QQmlContextData;
    context->url = url;
    context->isInternal = true;
    context->isJSContext = true;

    Scoped<QmlContextWrapper> w(scope, v4->memoryManager->alloc<QmlContextWrapper>(v8, context, (QObject*)0, true));
    w->d()->isNullWrapper = true;
    return w.asReturnedValue();
}

QQmlContextData *QmlContextWrapper::callingContext(ExecutionEngine *v4)
{
    Scope scope(v4);
    QV4::Scoped<QmlContextWrapper> c(scope, v4->qmlContextObject(), QV4::Scoped<QmlContextWrapper>::Cast);

    return !!c ? c->getContext() : 0;
}

QQmlContextData *QmlContextWrapper::getContext(const ValueRef value)
{
    QV4::ExecutionEngine *v4 = value->engine();
    if (!v4)
        return 0;

    Scope scope(v4);
    QV4::Scoped<QmlContextWrapper> c(scope, value);

    return c ? c->getContext() : 0;
}

void QmlContextWrapper::takeContextOwnership(const ValueRef qmlglobal)
{
    QV4::ExecutionEngine *v4 = qmlglobal->engine();
    Q_ASSERT(v4);

    Scope scope(v4);
    QV4::Scoped<QmlContextWrapper> c(scope, qmlglobal);
    Q_ASSERT(c);
    c->d()->ownsContext = true;
}


ReturnedValue QmlContextWrapper::get(Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<QmlContextWrapper>());
    QV4::ExecutionEngine *v4 = m->engine();
    QV4::Scope scope(v4);
    QmlContextWrapper *resource = static_cast<QmlContextWrapper *>(m);

    // In V8 the JS global object would come _before_ the QML global object,
    // so simulate that here.
    bool hasProp;
    QV4::ScopedValue result(scope, v4->globalObject->get(name, &hasProp));
    if (hasProp) {
        if (hasProperty)
            *hasProperty = hasProp;
        return result.asReturnedValue();
    }

    if (resource->d()->isNullWrapper)
        return Object::get(m, name, hasProperty);

    if (QV4::QmlContextWrapper::callingContext(v4) != resource->d()->context)
        return Object::get(m, name, hasProperty);

    result = Object::get(m, name, &hasProp);
    if (hasProp) {
        if (hasProperty)
            *hasProperty = hasProp;
        return result.asReturnedValue();
    }

    // Its possible we could delay the calculation of the "actual" context (in the case
    // of sub contexts) until it is definately needed.
    QQmlContextData *context = resource->getContext();
    QQmlContextData *expressionContext = context;

    if (!context) {
        if (hasProperty)
            *hasProperty = true;
        return result.asReturnedValue();
    }

    // Search type (attached property/enum/imported scripts) names
    // while (context) {
    //     Search context properties
    //     Search scope object
    //     Search context object
    //     context = context->parent
    // }

    QV8Engine *engine = v4->v8Engine;

    QObject *scopeObject = resource->getScopeObject();

    if (context->imports && name->startsWithUpper()) {
        // Search for attached properties, enums and imported scripts
        QQmlTypeNameCache::Result r = context->imports->query(name);

        if (r.isValid()) {
            if (hasProperty)
                *hasProperty = true;
            if (r.scriptIndex != -1) {
                QV4::ScopedObject scripts(scope, context->importedScripts);
                return scripts->getIndexed(r.scriptIndex);
            } else if (r.type) {
                return QmlTypeWrapper::create(engine, scopeObject, r.type);
            } else if (r.importNamespace) {
                return QmlTypeWrapper::create(engine, scopeObject, context->imports, r.importNamespace);
            }
            Q_ASSERT(!"Unreachable");
        }

        // Fall through
    }

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine->engine());

    while (context) {
        // Search context properties
        if (context->propertyNames.count()) {
            int propertyIdx = context->propertyNames.value(name);

            if (propertyIdx != -1) {

                if (propertyIdx < context->idValueCount) {

                    ep->captureProperty(&context->idValues[propertyIdx].bindings);
                    if (hasProperty)
                        *hasProperty = true;
                    return QV4::QObjectWrapper::wrap(v4, context->idValues[propertyIdx]);
                } else {

                    QQmlContextPrivate *cp = context->asQQmlContextPrivate();

                    ep->captureProperty(context->asQQmlContext(), -1,
                                        propertyIdx + cp->notifyIndex);

                    const QVariant &value = cp->propertyValues.at(propertyIdx);
                    if (hasProperty)
                        *hasProperty = true;
                    if (value.userType() == qMetaTypeId<QList<QObject*> >()) {
                        QQmlListProperty<QObject> prop(context->asQQmlContext(), (void*) qintptr(propertyIdx),
                                                               QQmlContextPrivate::context_count,
                                                               QQmlContextPrivate::context_at);
                        return QmlListWrapper::create(engine, prop, qMetaTypeId<QQmlListProperty<QObject> >());
                    } else {
                        return engine->fromVariant(cp->propertyValues.at(propertyIdx));
                    }
                }
            }
        }

        // Search scope object
        if (scopeObject) {
            bool hasProp = false;
            QV4::ScopedValue result(scope, QV4::QObjectWrapper::getQmlProperty(v4->currentContext(), context, scopeObject,
                                                                               name, QV4::QObjectWrapper::CheckRevision, &hasProp));
            if (hasProp) {
                if (hasProperty)
                    *hasProperty = true;
                return result.asReturnedValue();
            }
        }
        scopeObject = 0;


        // Search context object
        if (context->contextObject) {
            bool hasProp = false;
            result = QV4::QObjectWrapper::getQmlProperty(v4->currentContext(), context, context->contextObject, name, QV4::QObjectWrapper::CheckRevision, &hasProp);
            if (hasProp) {
                if (hasProperty)
                    *hasProperty = true;
                return result.asReturnedValue();
            }
        }

        context = context->parent;
    }

    expressionContext->unresolvedNames = true;

    return Primitive::undefinedValue().asReturnedValue();
}

void QmlContextWrapper::put(Managed *m, String *name, const ValueRef value)
{
    Q_ASSERT(m->as<QmlContextWrapper>());
    ExecutionEngine *v4 = m->engine();
    QV4::Scope scope(v4);
    if (scope.hasException())
        return;
    QV4::Scoped<QmlContextWrapper> wrapper(scope, static_cast<QmlContextWrapper *>(m));

    PropertyAttributes attrs;
    Property *pd  = wrapper->__getOwnProperty__(name, &attrs);
    if (pd) {
        wrapper->putValue(pd, attrs, value);
        return;
    }

    if (wrapper->d()->isNullWrapper) {
        if (wrapper && wrapper->d()->readOnly) {
            QString error = QLatin1String("Invalid write to global property \"") + name->toQString() +
                            QLatin1Char('"');
            Scoped<String> e(scope, v4->currentContext()->d()->engine->newString(error));
            v4->currentContext()->throwError(e);
            return;
        }

        Object::put(m, name, value);
        return;
    }

    // Its possible we could delay the calculation of the "actual" context (in the case
    // of sub contexts) until it is definately needed.
    QQmlContextData *context = wrapper->getContext();
    QQmlContextData *expressionContext = context;

    if (!context)
        return;

    // See QV8ContextWrapper::Getter for resolution order

    QObject *scopeObject = wrapper->getScopeObject();

    while (context) {
        // Search context properties
        if (context->propertyNames.count() && -1 != context->propertyNames.value(name))
            return;

        // Search scope object
        if (scopeObject &&
            QV4::QObjectWrapper::setQmlProperty(v4->currentContext(), context, scopeObject, name, QV4::QObjectWrapper::CheckRevision, value))
            return;
        scopeObject = 0;

        // Search context object
        if (context->contextObject &&
            QV4::QObjectWrapper::setQmlProperty(v4->currentContext(), context, context->contextObject, name, QV4::QObjectWrapper::CheckRevision, value))
            return;

        context = context->parent;
    }

    expressionContext->unresolvedNames = true;

    if (wrapper->d()->readOnly) {
        QString error = QLatin1String("Invalid write to global property \"") + name->toQString() +
                        QLatin1Char('"');
        v4->currentContext()->throwError(error);
        return;
    }

    Object::put(m, name, value);
}

void QmlContextWrapper::destroy(Managed *that)
{
    static_cast<QmlContextWrapper *>(that)->d()->~Data();
}

void QmlContextWrapper::markObjects(Managed *m, ExecutionEngine *engine)
{
    QmlContextWrapper *This = static_cast<QmlContextWrapper*>(m);
    if (This->d()->idObjectsWrapper)
        This->d()->idObjectsWrapper->mark(engine);
    Object::markObjects(m, engine);
}

void QmlContextWrapper::registerQmlDependencies(ExecutionEngine *engine, const CompiledData::Function *compiledFunction)
{
    // Let the caller check and avoid the function call :)
    Q_ASSERT(compiledFunction->hasQmlDependencies());

    QQmlEnginePrivate *ep = engine->v8Engine->engine() ? QQmlEnginePrivate::get(engine->v8Engine->engine()) : 0;
    if (!ep)
        return;
    QQmlEnginePrivate::PropertyCapture *capture = ep->propertyCapture;
    if (!capture)
        return;

    QV4::Scope scope(engine);
    QV4::Scoped<QmlContextWrapper> contextWrapper(scope, engine->qmlContextObject(), QV4::Scoped<QmlContextWrapper>::Cast);
    QQmlContextData *qmlContext = contextWrapper->getContext();

    const quint32 *idObjectDependency = compiledFunction->qmlIdObjectDependencyTable();
    const int idObjectDependencyCount = compiledFunction->nDependingIdObjects;
    for (int i = 0; i < idObjectDependencyCount; ++i, ++idObjectDependency) {
        Q_ASSERT(int(*idObjectDependency) < qmlContext->idValueCount);
        capture->captureProperty(&qmlContext->idValues[*idObjectDependency].bindings);
    }

    Q_ASSERT(qmlContext->contextObject);
    const quint32 *contextPropertyDependency = compiledFunction->qmlContextPropertiesDependencyTable();
    const int contextPropertyDependencyCount = compiledFunction->nDependingContextProperties;
    for (int i = 0; i < contextPropertyDependencyCount; ++i) {
        const int propertyIndex = *contextPropertyDependency++;
        const int notifyIndex = *contextPropertyDependency++;
        capture->captureProperty(qmlContext->contextObject, propertyIndex, notifyIndex);
    }

    QObject *scopeObject = contextWrapper->getScopeObject();
    const quint32 *scopePropertyDependency = compiledFunction->qmlScopePropertiesDependencyTable();
    const int scopePropertyDependencyCount = compiledFunction->nDependingScopeProperties;
    for (int i = 0; i < scopePropertyDependencyCount; ++i) {
        const int propertyIndex = *scopePropertyDependency++;
        const int notifyIndex = *scopePropertyDependency++;
        capture->captureProperty(scopeObject, propertyIndex, notifyIndex);
    }

}

ReturnedValue QmlContextWrapper::idObjectsArray()
{
    if (!d()->idObjectsWrapper) {
        ExecutionEngine *v4 = engine();
        Scope scope(v4);
        Scoped<QQmlIdObjectsArray> a(scope, v4->memoryManager->alloc<QQmlIdObjectsArray>(v4, this));
        d()->idObjectsWrapper = a.getPointer();
    }
    return d()->idObjectsWrapper->asReturnedValue();
}

ReturnedValue QmlContextWrapper::qmlSingletonWrapper(QV8Engine *v8, String *name)
{
    if (!d()->context->imports)
        return Encode::undefined();
    // Search for attached properties, enums and imported scripts
    QQmlTypeNameCache::Result r = d()->context->imports->query(name);

    Q_ASSERT(r.isValid());
    Q_ASSERT(r.type);
    Q_ASSERT(r.type->isSingleton());
    Q_ASSERT(v8);

    QQmlEngine *e = v8->engine();
    QQmlType::SingletonInstanceInfo *siinfo = r.type->singletonInstanceInfo();
    siinfo->init(e);

    if (QObject *qobjectSingleton = siinfo->qobjectApi(e))
        return QV4::QObjectWrapper::wrap(engine(), qobjectSingleton);
    return QJSValuePrivate::get(siinfo->scriptApi(e))->getValue(engine());
}

DEFINE_OBJECT_VTABLE(QQmlIdObjectsArray);

QQmlIdObjectsArray::Data::Data(ExecutionEngine *engine, QmlContextWrapper *contextWrapper)
    : Object::Data(engine)
    , contextWrapper(contextWrapper)
{
    setVTable(staticVTable());
}

ReturnedValue QQmlIdObjectsArray::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    QQmlIdObjectsArray *This = static_cast<QQmlIdObjectsArray*>(m);
    QQmlContextData *context = This->d()->contextWrapper->getContext();
    if (!context) {
        if (hasProperty)
            *hasProperty = false;
        return Encode::undefined();
    }
    if (index >= (uint)context->idValueCount) {
        if (hasProperty)
            *hasProperty = false;
        return Encode::undefined();
    }

    if (hasProperty)
        *hasProperty = true;

    ExecutionEngine *v4 = m->engine();
    QQmlEnginePrivate *ep = v4->v8Engine->engine() ? QQmlEnginePrivate::get(v4->v8Engine->engine()) : 0;
    if (ep)
        ep->captureProperty(&context->idValues[index].bindings);

    return QObjectWrapper::wrap(This->engine(), context->idValues[index].data());
}

void QQmlIdObjectsArray::markObjects(Managed *that, ExecutionEngine *engine)
{
    QQmlIdObjectsArray *This = static_cast<QQmlIdObjectsArray*>(that);
    This->d()->contextWrapper->mark(engine);
    Object::markObjects(that, engine);
}

QT_END_NAMESPACE
