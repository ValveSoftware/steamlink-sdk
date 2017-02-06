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

#include "qqmltypewrapper_p.h"
#include <private/qqmlcontextwrapper_p.h>
#include <private/qv8engine_p.h>

#include <private/qqmlengine_p.h>
#include <private/qqmlcontext_p.h>

#include <private/qjsvalue_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4qobjectwrapper_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QmlTypeWrapper);

void Heap::QmlTypeWrapper::init()
{
    Object::init();
    mode = IncludeEnums;
    object.init();
}

void Heap::QmlTypeWrapper::destroy()
{
    if (typeNamespace)
        typeNamespace->release();
    object.destroy();
    Object::destroy();
}

bool QmlTypeWrapper::isSingleton() const
{
    return d()->type && d()->type->isSingleton();
}

QObject* QmlTypeWrapper::singletonObject() const
{
    if (!isSingleton())
        return 0;

    QQmlEngine *e = engine()->qmlEngine();
    QQmlType::SingletonInstanceInfo *siinfo = d()->type->singletonInstanceInfo();
    siinfo->init(e);
    return siinfo->qobjectApi(e);
}

QVariant QmlTypeWrapper::toVariant() const
{
    if (d()->type && d()->type->isSingleton()) {
        QQmlEngine *e = engine()->qmlEngine();
        QQmlType::SingletonInstanceInfo *siinfo = d()->type->singletonInstanceInfo();
        siinfo->init(e); // note: this will also create QJSValue singleton which isn't strictly required.
        QObject *qobjectSingleton = siinfo->qobjectApi(e);
        if (qobjectSingleton) {
            return QVariant::fromValue<QObject*>(qobjectSingleton);
        }
    }

    // only QObject Singleton Type can be converted to a variant.
    return QVariant();
}


// Returns a type wrapper for type t on o.  This allows access of enums, and attached properties.
ReturnedValue QmlTypeWrapper::create(QV4::ExecutionEngine *engine, QObject *o, QQmlType *t,
                                     Heap::QmlTypeWrapper::TypeNameMode mode)
{
    Q_ASSERT(t);
    Scope scope(engine);

    Scoped<QmlTypeWrapper> w(scope, engine->memoryManager->allocObject<QmlTypeWrapper>());
    w->d()->mode = mode; w->d()->object = o; w->d()->type = t;
    return w.asReturnedValue();
}

// Returns a type wrapper for importNamespace (of t) on o.  This allows nested resolution of a type in a
// namespace.
ReturnedValue QmlTypeWrapper::create(QV4::ExecutionEngine *engine, QObject *o, QQmlTypeNameCache *t, const void *importNamespace,
                                     Heap::QmlTypeWrapper::TypeNameMode mode)
{
    Q_ASSERT(t);
    Q_ASSERT(importNamespace);
    Scope scope(engine);

    Scoped<QmlTypeWrapper> w(scope, engine->memoryManager->allocObject<QmlTypeWrapper>());
    w->d()->mode = mode; w->d()->object = o; w->d()->typeNamespace = t; w->d()->importNamespace = importNamespace;
    t->addref();
    return w.asReturnedValue();
}

static int enumForSingleton(String *name, QObject *qobjectSingleton)
{
    // ### Optimize
    QByteArray enumName = name->toQString().toUtf8();
    const QMetaObject *metaObject = qobjectSingleton->metaObject();
    for (int ii = metaObject->enumeratorCount() - 1; ii >= 0; --ii) {
        QMetaEnum e = metaObject->enumerator(ii);
        bool ok;
        int value = e.keyToValue(enumName.constData(), &ok);
        if (ok)
            return value;
    }
    return -1;
}

static ReturnedValue throwLowercaseEnumError(QV4::ExecutionEngine *v4, String *name, QQmlType *type)
{
    const QString message =
            QStringLiteral("Cannot access enum value '%1' of '%2', enum values need to start with an uppercase letter.")
                .arg(name->toQString()).arg(QLatin1String(type->typeName()));
    return v4->throwTypeError(message);
}

ReturnedValue QmlTypeWrapper::get(const Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<QmlTypeWrapper>());

    QV4::ExecutionEngine *v4 = static_cast<const QmlTypeWrapper *>(m)->engine();
    QV4::Scope scope(v4);

    Scoped<QmlTypeWrapper> w(scope, static_cast<const QmlTypeWrapper *>(m));

    if (hasProperty)
        *hasProperty = true;

    QQmlContextData *context = v4->callingQmlContext();

    QObject *object = w->d()->object;
    QQmlType *type = w->d()->type;

    if (type) {

        // singleton types are handled differently to other types.
        if (type->isSingleton()) {
            QQmlEngine *e = v4->qmlEngine();
            QQmlType::SingletonInstanceInfo *siinfo = type->singletonInstanceInfo();
            siinfo->init(e);

            QObject *qobjectSingleton = siinfo->qobjectApi(e);
            if (qobjectSingleton) {

                // check for enum value
                const bool includeEnums = w->d()->mode == Heap::QmlTypeWrapper::IncludeEnums;
                if (includeEnums && name->startsWithUpper()) {
                    const int value = enumForSingleton(name, qobjectSingleton);
                    if (value != -1)
                        return QV4::Primitive::fromInt32(value).asReturnedValue();
                }

                // check for property.
                bool ok;
                const ReturnedValue result = QV4::QObjectWrapper::getQmlProperty(v4, context, qobjectSingleton, name, QV4::QObjectWrapper::IgnoreRevision, &ok);
                if (hasProperty)
                    *hasProperty = ok;

                // Warn when attempting to access a lowercased enum value, singleton case
                if (!ok && includeEnums && !name->startsWithUpper()) {
                    const int value = enumForSingleton(name, qobjectSingleton);
                    if (value != -1)
                        return throwLowercaseEnumError(v4, name, type);
                }

                return result;
            } else if (!siinfo->scriptApi(e).isUndefined()) {
                // NOTE: if used in a binding, changes will not trigger re-evaluation since non-NOTIFYable.
                QV4::ScopedObject o(scope, QJSValuePrivate::convertedToValue(v4, siinfo->scriptApi(e)));
                if (!!o)
                    return o->get(name);
            }

            // Fall through to base implementation

        } else {

            if (name->startsWithUpper()) {
                bool ok = false;
                int value = type->enumValue(QQmlEnginePrivate::get(v4->qmlEngine()), name, &ok);
                if (ok)
                    return QV4::Primitive::fromInt32(value).asReturnedValue();

                // Fall through to base implementation

            } else if (w->d()->object) {
                QObject *ao = qmlAttachedPropertiesObjectById(type->attachedPropertiesId(QQmlEnginePrivate::get(v4->qmlEngine())), object);
                if (ao)
                    return QV4::QObjectWrapper::getQmlProperty(v4, context, ao, name, QV4::QObjectWrapper::IgnoreRevision, hasProperty);

                // Fall through to base implementation
            }

            // Fall through to base implementation
        }

        // Fall through to base implementation

    } else if (w->d()->typeNamespace) {
        Q_ASSERT(w->d()->importNamespace);
        QQmlTypeNameCache::Result r = w->d()->typeNamespace->query(name, w->d()->importNamespace);

        if (r.isValid()) {
            if (r.type) {
                return create(scope.engine, object, r.type, w->d()->mode);
            } else if (r.scriptIndex != -1) {
                QV4::ScopedObject scripts(scope, context->importedScripts.valueRef());
                return scripts->getIndexed(r.scriptIndex);
            } else if (r.importNamespace) {
                return create(scope.engine, object, context->imports, r.importNamespace);
            }

            return QV4::Encode::undefined();

        }

        // Fall through to base implementation

    } else {
        Q_ASSERT(!"Unreachable");
    }

    bool ok = false;
    const ReturnedValue result = Object::get(m, name, &ok);
    if (hasProperty)
        *hasProperty = ok;

    // Warn when attempting to access a lowercased enum value, non-singleton case
    if (!ok && type && !type->isSingleton() && !name->startsWithUpper()) {
        bool enumOk = false;
        type->enumValue(QQmlEnginePrivate::get(v4->qmlEngine()), name, &enumOk);
        if (enumOk)
            return throwLowercaseEnumError(v4, name, type);
    }

    return result;
}


void QmlTypeWrapper::put(Managed *m, String *name, const Value &value)
{
    Q_ASSERT(m->as<QmlTypeWrapper>());
    QmlTypeWrapper *w = static_cast<QmlTypeWrapper *>(m);
    QV4::ExecutionEngine *v4 = w->engine();
    if (v4->hasException)
        return;

    QV4::Scope scope(v4);
    QQmlContextData *context = v4->callingQmlContext();

    QQmlType *type = w->d()->type;
    if (type && !type->isSingleton() && w->d()->object) {
        QObject *object = w->d()->object;
        QQmlEngine *e = scope.engine->qmlEngine();
        QObject *ao = qmlAttachedPropertiesObjectById(type->attachedPropertiesId(QQmlEnginePrivate::get(e)), object);
        if (ao)
            QV4::QObjectWrapper::setQmlProperty(v4, context, ao, name, QV4::QObjectWrapper::IgnoreRevision, value);
    } else if (type && type->isSingleton()) {
        QQmlEngine *e = scope.engine->qmlEngine();
        QQmlType::SingletonInstanceInfo *siinfo = type->singletonInstanceInfo();
        siinfo->init(e);

        QObject *qobjectSingleton = siinfo->qobjectApi(e);
        if (qobjectSingleton) {
            QV4::QObjectWrapper::setQmlProperty(v4, context, qobjectSingleton, name, QV4::QObjectWrapper::IgnoreRevision, value);
        } else if (!siinfo->scriptApi(e).isUndefined()) {
            QV4::ScopedObject apiprivate(scope, QJSValuePrivate::convertedToValue(v4, siinfo->scriptApi(e)));
            if (!apiprivate) {
                QString error = QLatin1String("Cannot assign to read-only property \"") + name->toQString() + QLatin1Char('\"');
                v4->throwError(error);
                return;
            } else {
                apiprivate->put(name, value);
            }
        }
    }
}

PropertyAttributes QmlTypeWrapper::query(const Managed *m, String *name)
{
    // ### Implement more efficiently.
    bool hasProperty = false;
    static_cast<Object *>(const_cast<Managed*>(m))->get(name, &hasProperty);
    return hasProperty ? Attr_Data : Attr_Invalid;
}

bool QmlTypeWrapper::isEqualTo(Managed *a, Managed *b)
{
    Q_ASSERT(a->as<QV4::QmlTypeWrapper>());
    QV4::QmlTypeWrapper *qmlTypeWrapperA = static_cast<QV4::QmlTypeWrapper *>(a);
    if (QV4::QmlTypeWrapper *qmlTypeWrapperB = b->as<QV4::QmlTypeWrapper>())
        return qmlTypeWrapperA->toVariant() == qmlTypeWrapperB->toVariant();
    else if (QV4::QObjectWrapper *qobjectWrapper = b->as<QV4::QObjectWrapper>())
        return qmlTypeWrapperA->toVariant().value<QObject*>() == qobjectWrapper->object();

    return false;
}

QT_END_NAMESPACE
