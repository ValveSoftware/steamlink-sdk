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

#include "qv4qobjectwrapper_p.h"

#include <private/qqmlpropertycache_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qjsvalue_p.h>
#include <private/qqmlaccessors_p.h>
#include <private/qqmlexpression_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmltypewrapper_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmllistwrapper_p.h>
#include <private/qqmlbuiltinfunctions_p.h>
#include <private/qv8engine_p.h>

#include <private/qv4functionobject_p.h>
#include <private/qv4runtime_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4sequenceobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4jsonobject_p.h>
#include <private/qv4regexpobject_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4mm_p.h>
#include <private/qqmlscriptstring_p.h>
#include <private/qv4compileddata_p.h>

#include <QtQml/qjsvalue.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qtimer.h>
#include <QtCore/qatomic.h>

QT_BEGIN_NAMESPACE

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
# if (__GNUC__ * 100 + __GNUC_MINOR__) >= 405
// The code in this file does not violate strict aliasing, but GCC thinks it does
// so turn off the warnings for us to have a clean build
#  pragma GCC diagnostic ignored "-Wstrict-aliasing"
# endif
#endif


using namespace QV4;

static QPair<QObject *, int> extractQtMethod(QV4::FunctionObject *function)
{
    QV4::ExecutionEngine *v4 = function->engine();
    if (v4) {
        QV4::Scope scope(v4);
        QV4::Scoped<QObjectMethod> method(scope, function->as<QObjectMethod>());
        if (method)
            return qMakePair(method->object(), method->methodIndex());
    }

    return qMakePair((QObject *)0, -1);
}

static QPair<QObject *, int> extractQtSignal(const ValueRef value)
{
    QV4::ExecutionEngine *v4 = value->engine();
    if (v4) {
        QV4::Scope scope(v4);
        QV4::ScopedFunctionObject function(scope, value);
        if (function)
            return extractQtMethod(function);

        QV4::Scoped<QV4::QmlSignalHandler> handler(scope, value);
        if (handler)
            return qMakePair(handler->object(), handler->signalIndex());
    }

    return qMakePair((QObject *)0, -1);
}


struct ReadAccessor {
    static inline void Indirect(QObject *object, const QQmlPropertyData &property,
                                void *output, QQmlNotifier **n)
    {
        Q_ASSERT(n == 0);
        Q_UNUSED(n);

        void *args[] = { output, 0 };
        QMetaObject::metacall(object, QMetaObject::ReadProperty, property.coreIndex, args);
    }

    static inline void Direct(QObject *object, const QQmlPropertyData &property,
                              void *output, QQmlNotifier **n)
    {
        Q_ASSERT(n == 0);
        Q_UNUSED(n);

        void *args[] = { output, 0 };
        object->qt_metacall(QMetaObject::ReadProperty, property.coreIndex, args);
    }

    static inline void Accessor(QObject *object, const QQmlPropertyData &property,
                                void *output, QQmlNotifier **n)
    {
        Q_ASSERT(property.accessors);

        property.accessors->read(object, property.accessorData, output);
        if (n) property.accessors->notifier(object, property.accessorData, n);
    }
};

// Load value properties
template<void (*ReadFunction)(QObject *, const QQmlPropertyData &,
                              void *, QQmlNotifier **)>
static QV4::ReturnedValue LoadProperty(QV8Engine *engine, QObject *object,
                                          const QQmlPropertyData &property,
                                          QQmlNotifier **notifier)
{
    Q_ASSERT(!property.isFunction());
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    QV4::Scope scope(v4);

    if (property.isQObject()) {
        QObject *rv = 0;
        ReadFunction(object, property, &rv, notifier);
        return QV4::QObjectWrapper::wrap(v4, rv);
    } else if (property.isQList()) {
        return QmlListWrapper::create(engine, object, property.coreIndex, property.propType);
    } else if (property.propType == QMetaType::QReal) {
        qreal v = 0;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.propType == QMetaType::Int || property.isEnum()) {
        int v = 0;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.propType == QMetaType::Bool) {
        bool v = false;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.propType == QMetaType::QString) {
        QString v;
        ReadFunction(object, property, &v, notifier);
        return v4->newString(v)->asReturnedValue();
    } else if (property.propType == QMetaType::UInt) {
        uint v = 0;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.propType == QMetaType::Float) {
        float v = 0;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.propType == QMetaType::Double) {
        double v = 0;
        ReadFunction(object, property, &v, notifier);
        return QV4::Encode(v);
    } else if (property.isV4Handle()) {
        QQmlV4Handle handle;
        ReadFunction(object, property, &handle, notifier);
        return handle;
    } else if (property.propType == qMetaTypeId<QJSValue>()) {
        QJSValue v;
        ReadFunction(object, property, &v, notifier);
        return QJSValuePrivate::get(v)->getValue(v4);
    } else if (property.isQVariant()) {
        QVariant v;
        ReadFunction(object, property, &v, notifier);

        if (QQmlValueTypeFactory::isValueType(v.userType())) {
            if (QQmlValueType *valueType = QQmlValueTypeFactory::valueType(v.userType()))
                return QV4::QmlValueTypeWrapper::create(engine, object, property.coreIndex, valueType); // VariantReference value-type.
        }

        return engine->fromVariant(v);
    } else if (QQmlValueTypeFactory::isValueType(property.propType)) {
        Q_ASSERT(notifier == 0);

        if (QQmlValueType *valueType = QQmlValueTypeFactory::valueType(property.propType))
            return QV4::QmlValueTypeWrapper::create(engine, object, property.coreIndex, valueType);
    } else {
        Q_ASSERT(notifier == 0);

        // see if it's a sequence type
        bool succeeded = false;
        QV4::ScopedValue retn(scope, QV4::SequencePrototype::newSequence(v4, property.propType, object, property.coreIndex, &succeeded));
        if (succeeded)
            return retn.asReturnedValue();
    }

    if (property.propType == QMetaType::UnknownType) {
        QMetaProperty p = object->metaObject()->property(property.coreIndex);
        qWarning("QMetaProperty::read: Unable to handle unregistered datatype '%s' for property "
                 "'%s::%s'", p.typeName(), object->metaObject()->className(), p.name());
        return QV4::Encode::undefined();
    } else {
        QVariant v(property.propType, (void *)0);
        ReadFunction(object, property, v.data(), notifier);
        return engine->fromVariant(v);
    }
}

QObjectWrapper::Data::Data(ExecutionEngine *engine, QObject *object)
    : Object::Data(engine)
    , object(object)
{
    setVTable(staticVTable());
}

void QObjectWrapper::initializeBindings(ExecutionEngine *engine)
{
    engine->functionClass->prototype->defineDefaultProperty(QStringLiteral("connect"), method_connect);
    engine->functionClass->prototype->defineDefaultProperty(QStringLiteral("disconnect"), method_disconnect);
}

QQmlPropertyData *QObjectWrapper::findProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local) const
{
    Q_UNUSED(revisionMode);

    QQmlData *ddata = QQmlData::get(d()->object, false);
    if (!ddata)
        return 0;
    QQmlPropertyData *result = 0;
    if (ddata && ddata->propertyCache)
        result = ddata->propertyCache->property(name, d()->object, qmlContext);
    else
        result = QQmlPropertyCache::property(engine->v8Engine->engine(), d()->object, name, qmlContext, *local);
    return result;
}

ReturnedValue QObjectWrapper::getQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, String *n, QObjectWrapper::RevisionMode revisionMode,
                                             bool *hasProperty, bool includeImports)
{
    if (QQmlData::wasDeleted(d()->object)) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::undefined();
    }

    QV4::Scope scope(ctx);
    QV4::ScopedString name(scope, n);

    if (name->equals(scope.engine->id_destroy) || name->equals(scope.engine->id_toString)) {
        int index = name->equals(scope.engine->id_destroy) ? QV4::QObjectMethod::DestroyMethod : QV4::QObjectMethod::ToStringMethod;
        QV4::ScopedValue method(scope, QV4::QObjectMethod::create(ctx->d()->engine->rootContext, d()->object, index));
        if (hasProperty)
            *hasProperty = true;
        return method.asReturnedValue();
    }

    QQmlPropertyData local;
    QQmlPropertyData *result = findProperty(ctx->d()->engine, qmlContext, name.getPointer(), revisionMode, &local);

    if (!result) {
        if (includeImports && name->startsWithUpper()) {
            // Check for attached properties
            if (qmlContext && qmlContext->imports) {
                QQmlTypeNameCache::Result r = qmlContext->imports->query(name.getPointer());

                if (hasProperty)
                    *hasProperty = true;

                if (r.isValid()) {
                    if (r.scriptIndex != -1) {
                        return QV4::Encode::undefined();
                    } else if (r.type) {
                        return QmlTypeWrapper::create(ctx->d()->engine->v8Engine, d()->object, r.type, QmlTypeWrapper::ExcludeEnums);
                    } else if (r.importNamespace) {
                        return QmlTypeWrapper::create(ctx->d()->engine->v8Engine, d()->object, qmlContext->imports, r.importNamespace, QmlTypeWrapper::ExcludeEnums);
                    }
                    Q_ASSERT(!"Unreachable");
                }
            }
        }
        return QV4::Object::get(this, name.getPointer(), hasProperty);
    }

    QQmlData *ddata = QQmlData::get(d()->object, false);

    if (revisionMode == QV4::QObjectWrapper::CheckRevision && result->hasRevision()) {
        if (ddata && ddata->propertyCache && !ddata->propertyCache->isAllowedInRevision(result)) {
            if (hasProperty)
                *hasProperty = false;
            return QV4::Encode::undefined();
        }
    }

    if (hasProperty)
        *hasProperty = true;

    return getProperty(d()->object, ctx, result);
}

ReturnedValue QObjectWrapper::getProperty(QObject *object, ExecutionContext *ctx, QQmlPropertyData *property, bool captureRequired)
{
    QV4::Scope scope(ctx);

    QQmlData::flushPendingBinding(object, property->coreIndex);

    if (property->isFunction() && !property->isVarProperty()) {
        if (property->isVMEFunction()) {
            QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
            Q_ASSERT(vmemo);
            return vmemo->vmeMethod(property->coreIndex);
        } else if (property->isV4Function()) {
            QV4::Scoped<QV4::Object> qmlcontextobject(scope, ctx->d()->engine->qmlContextObject());
            return QV4::QObjectMethod::create(ctx->d()->engine->rootContext, object, property->coreIndex, qmlcontextobject);
        } else if (property->isSignalHandler()) {
            QV4::Scoped<QV4::QmlSignalHandler> handler(scope, scope.engine->memoryManager->alloc<QV4::QmlSignalHandler>(ctx->d()->engine, object, property->coreIndex));

            QV4::ScopedString connect(scope, ctx->d()->engine->newIdentifier(QStringLiteral("connect")));
            QV4::ScopedString disconnect(scope, ctx->d()->engine->newIdentifier(QStringLiteral("disconnect")));
            handler->put(connect.getPointer(), QV4::ScopedValue(scope, ctx->d()->engine->functionClass->prototype->get(connect.getPointer())));
            handler->put(disconnect.getPointer(), QV4::ScopedValue(scope, ctx->d()->engine->functionClass->prototype->get(disconnect.getPointer())));

            return handler.asReturnedValue();
        } else {
            return QV4::QObjectMethod::create(ctx->d()->engine->rootContext, object, property->coreIndex);
        }
    }

    QQmlEnginePrivate *ep = ctx->d()->engine->v8Engine->engine() ? QQmlEnginePrivate::get(ctx->d()->engine->v8Engine->engine()) : 0;

    if (property->hasAccessors()) {
        QQmlNotifier *n = 0;
        QQmlNotifier **nptr = 0;

        if (ep && ep->propertyCapture && property->accessors->notifier)
            nptr = &n;

        QV4::ScopedValue rv(scope, LoadProperty<ReadAccessor::Accessor>(ctx->d()->engine->v8Engine, object, *property, nptr));

        if (captureRequired) {
            if (property->accessors->notifier) {
                if (n)
                    ep->captureProperty(n);
            } else {
                ep->captureProperty(object, property->coreIndex, property->notifyIndex);
            }
        }

        return rv.asReturnedValue();
    }

    if (captureRequired && ep && !property->isConstant())
        ep->captureProperty(object, property->coreIndex, property->notifyIndex);

    if (property->isVarProperty()) {
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        return vmemo->vmeProperty(property->coreIndex);
    } else if (property->isDirect())  {
        return LoadProperty<ReadAccessor::Direct>(ctx->d()->engine->v8Engine, object, *property, 0);
    } else {
        return LoadProperty<ReadAccessor::Indirect>(ctx->d()->engine->v8Engine, object, *property, 0);
    }
}

ReturnedValue QObjectWrapper::getQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, QObject *object, String *name, QObjectWrapper::RevisionMode revisionMode, bool *hasProperty)
{
    QV4::Scope scope(ctx);
    if (QQmlData::wasDeleted(object)) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::null();
    }

    if (!QQmlData::get(object, true)) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::null();
    }

    QV4::Scoped<QObjectWrapper> wrapper(scope, wrap(ctx->d()->engine, object));
    if (!wrapper) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::null();
    }
    return wrapper->getQmlProperty(ctx, qmlContext, name, revisionMode, hasProperty);
}

bool QObjectWrapper::setQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, QObject *object, String *name,
                                    QObjectWrapper::RevisionMode revisionMode, const ValueRef value)
{
    if (QQmlData::wasDeleted(object))
        return false;

    QQmlPropertyData local;
    QQmlPropertyData *result = 0;
    {
        result = QQmlPropertyCache::property(ctx->d()->engine->v8Engine->engine(), object, name, qmlContext, local);
    }

    if (!result)
        return false;

    if (revisionMode == QV4::QObjectWrapper::CheckRevision && result->hasRevision()) {
        QQmlData *ddata = QQmlData::get(object);
        if (ddata && ddata->propertyCache && !ddata->propertyCache->isAllowedInRevision(result))
            return false;
    }

    setProperty(object, ctx, result, value);
    return true;
}

void QObjectWrapper::setProperty(QObject *object, ExecutionContext *ctx, QQmlPropertyData *property, const ValueRef value)
{
    if (!property->isWritable() && !property->isQList()) {
        QString error = QLatin1String("Cannot assign to read-only property \"") +
                        property->name(object) + QLatin1Char('\"');
        ctx->throwTypeError(error);
        return;
    }

    QQmlBinding *newBinding = 0;
    QV4::Scope scope(ctx);
    QV4::ScopedFunctionObject f(scope, value);
    if (f) {
        if (!f->bindingKeyFlag()) {
            if (!property->isVarProperty() && property->propType != qMetaTypeId<QJSValue>()) {
                // assigning a JS function to a non var or QJSValue property or is not allowed.
                QString error = QLatin1String("Cannot assign JavaScript function to ");
                if (!QMetaType::typeName(property->propType))
                    error += QLatin1String("[unknown property type]");
                else
                    error += QLatin1String(QMetaType::typeName(property->propType));
                ctx->throwError(error);
                return;
            }
        } else {
            // binding assignment.
            QQmlContextData *callingQmlContext = QV4::QmlContextWrapper::callingContext(ctx->d()->engine);

            QV4::Scoped<QQmlBindingFunction> bindingFunction(scope, f);
            bindingFunction->initBindingLocation();

            newBinding = new QQmlBinding(value, object, callingQmlContext);
            newBinding->setTarget(object, *property, callingQmlContext);
        }
    }

    QQmlAbstractBinding *oldBinding =
        QQmlPropertyPrivate::setBinding(object, property->coreIndex, -1, newBinding);
    if (oldBinding)
        oldBinding->destroy();

    if (!newBinding && property->isVarProperty()) {
        // allow assignment of "special" values (null, undefined, function) to var properties
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        vmemo->setVMEProperty(property->coreIndex, value);
        return;
    }

#define PROPERTY_STORE(cpptype, value) \
    cpptype o = value; \
    int status = -1; \
    int flags = 0; \
    void *argv[] = { &o, 0, &status, &flags }; \
    QMetaObject::metacall(object, QMetaObject::WriteProperty, property->coreIndex, argv);

    if (value->isNull() && property->isQObject()) {
        PROPERTY_STORE(QObject*, 0);
    } else if (value->isUndefined() && property->isResettable()) {
        void *a[] = { 0 };
        QMetaObject::metacall(object, QMetaObject::ResetProperty, property->coreIndex, a);
    } else if (value->isUndefined() && property->propType == qMetaTypeId<QVariant>()) {
        PROPERTY_STORE(QVariant, QVariant());
    } else if (value->isUndefined() && property->propType == QMetaType::QJsonValue) {
        PROPERTY_STORE(QJsonValue, QJsonValue(QJsonValue::Undefined));
    } else if (!newBinding && property->propType == qMetaTypeId<QJSValue>()) {
        PROPERTY_STORE(QJSValue, new QJSValuePrivate(ctx->d()->engine, value));
    } else if (value->isUndefined() && property->propType != qMetaTypeId<QQmlScriptString>()) {
        QString error = QLatin1String("Cannot assign [undefined] to ");
        if (!QMetaType::typeName(property->propType))
            error += QLatin1String("[unknown property type]");
        else
            error += QLatin1String(QMetaType::typeName(property->propType));
        ctx->throwError(error);
        return;
    } else if (value->asFunctionObject()) {
        // this is handled by the binding creation above
    } else if (property->propType == QMetaType::Int && value->isNumber()) {
        PROPERTY_STORE(int, value->asDouble());
    } else if (property->propType == QMetaType::QReal && value->isNumber()) {
        PROPERTY_STORE(qreal, qreal(value->asDouble()));
    } else if (property->propType == QMetaType::Float && value->isNumber()) {
        PROPERTY_STORE(float, float(value->asDouble()));
    } else if (property->propType == QMetaType::Double && value->isNumber()) {
        PROPERTY_STORE(double, double(value->asDouble()));
    } else if (property->propType == QMetaType::QString && value->isString()) {
        PROPERTY_STORE(QString, value->toQStringNoThrow());
    } else if (property->isVarProperty()) {
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        vmemo->setVMEProperty(property->coreIndex, value);
    } else if (property->propType == qMetaTypeId<QQmlScriptString>() && (value->isUndefined() || value->isPrimitive())) {
        QQmlScriptString ss(value->toQStringNoThrow(), 0 /* context */, object);
        if (value->isNumber()) {
            ss.d->numberValue = value->toNumber();
            ss.d->isNumberLiteral = true;
        } else if (value->isString()) {
            ss.d->script = QV4::CompiledData::Binding::escapedString(ss.d->script);
            ss.d->isStringLiteral = true;
        }
        PROPERTY_STORE(QQmlScriptString, ss);
    } else {
        QVariant v;
        if (property->isQList())
            v = ctx->d()->engine->v8Engine->toVariant(value, qMetaTypeId<QList<QObject *> >());
        else
            v = ctx->d()->engine->v8Engine->toVariant(value, property->propType);

        QQmlContextData *callingQmlContext = QV4::QmlContextWrapper::callingContext(ctx->d()->engine);
        if (!QQmlPropertyPrivate::write(object, *property, v, callingQmlContext)) {
            const char *valueType = 0;
            if (v.userType() == QVariant::Invalid) valueType = "null";
            else valueType = QMetaType::typeName(v.userType());

            const char *targetTypeName = QMetaType::typeName(property->propType);
            if (!targetTypeName)
                targetTypeName = "an unregistered type";

            QString error = QLatin1String("Cannot assign ") +
                            QLatin1String(valueType) +
                            QLatin1String(" to ") +
                            QLatin1String(targetTypeName);
            ctx->throwError(error);
            return;
        }
    }
}

ReturnedValue QObjectWrapper::wrap(ExecutionEngine *engine, QObject *object)
{
    if (QQmlData::wasDeleted(object))
        return QV4::Encode::null();

    QQmlData *ddata = QQmlData::get(object, true);
    if (!ddata)
        return QV4::Encode::undefined();

    Scope scope(engine);

    if (ddata->jsEngineId == engine->m_engineId && !ddata->jsWrapper.isUndefined()) {
        // We own the JS object
        return ddata->jsWrapper.value();
    } else if (ddata->jsWrapper.isUndefined() &&
               (ddata->jsEngineId == engine->m_engineId || // We own the QObject
                ddata->jsEngineId == 0 ||    // No one owns the QObject
                !ddata->hasTaintedV8Object)) { // Someone else has used the QObject, but it isn't tainted

        QV4::ScopedValue rv(scope, create(engine, object));
        ddata->jsWrapper = rv;
        ddata->jsEngineId = engine->m_engineId;
        return rv.asReturnedValue();

    } else {
        // If this object is tainted, we have to check to see if it is in our
        // tainted object list
        Scoped<Object> alternateWrapper(scope, (Object *)0);
        if (engine->m_multiplyWrappedQObjects && ddata->hasTaintedV8Object)
            alternateWrapper = engine->m_multiplyWrappedQObjects->value(object);

        // If our tainted handle doesn't exist or has been collected, and there isn't
        // a handle in the ddata, we can assume ownership of the ddata->v8object
        if (ddata->jsWrapper.isUndefined() && !alternateWrapper) {
            QV4::ScopedValue result(scope, create(engine, object));
            ddata->jsWrapper = result;
            ddata->jsEngineId = engine->m_engineId;
            return result.asReturnedValue();
        }

        if (!alternateWrapper) {
            alternateWrapper = create(engine, object);
            if (!engine->m_multiplyWrappedQObjects)
                engine->m_multiplyWrappedQObjects = new MultiplyWrappedQObjectMap;
            engine->m_multiplyWrappedQObjects->insert(object, alternateWrapper.getPointer());
            ddata->hasTaintedV8Object = true;
        }

        return alternateWrapper.asReturnedValue();
    }
}

ReturnedValue QObjectWrapper::getProperty(QObject *object, ExecutionContext *ctx, int propertyIndex, bool captureRequired)
{
    if (QQmlData::wasDeleted(object))
        return QV4::Encode::null();
    QQmlData *ddata = QQmlData::get(object, /*create*/false);
    if (!ddata)
        return QV4::Encode::undefined();

    QQmlPropertyCache *cache = ddata->propertyCache;
    Q_ASSERT(cache);
    QQmlPropertyData *property = cache->property(propertyIndex);
    Q_ASSERT(property); // We resolved this property earlier, so it better exist!
    return getProperty(object, ctx, property, captureRequired);
}

void QObjectWrapper::setProperty(ExecutionContext *ctx, int propertyIndex, const ValueRef value)
{
    if (QQmlData::wasDeleted(d()->object))
        return;
    QQmlData *ddata = QQmlData::get(d()->object, /*create*/false);
    if (!ddata)
        return;

    QQmlPropertyCache *cache = ddata->propertyCache;
    Q_ASSERT(cache);
    QQmlPropertyData *property = cache->property(propertyIndex);
    Q_ASSERT(property); // We resolved this property earlier, so it better exist!
    return setProperty(d()->object, ctx, property, value);
}

bool QObjectWrapper::isEqualTo(Managed *a, Managed *b)
{
    Q_ASSERT(a->as<QV4::QObjectWrapper>());
    QV4::QObjectWrapper *qobjectWrapper = static_cast<QV4::QObjectWrapper *>(a);
    QV4::Object *o = b->asObject();
    if (o) {
        if (QV4::QmlTypeWrapper *qmlTypeWrapper = o->as<QV4::QmlTypeWrapper>())
            return qmlTypeWrapper->toVariant().value<QObject*>() == qobjectWrapper->object();
    }

    return false;
}

ReturnedValue QObjectWrapper::create(ExecutionEngine *engine, QObject *object)
{
    QQmlEngine *qmlEngine = engine->v8Engine->engine();
    if (qmlEngine)
        QQmlData::ensurePropertyCache(qmlEngine, object);
    return (engine->memoryManager->alloc<QV4::QObjectWrapper>(engine, object))->asReturnedValue();
}

QV4::ReturnedValue QObjectWrapper::get(Managed *m, String *name, bool *hasProperty)
{
    QObjectWrapper *that = static_cast<QObjectWrapper*>(m);
    ExecutionEngine *v4 = m->engine();
    QQmlContextData *qmlContext = QV4::QmlContextWrapper::callingContext(v4);
    return that->getQmlProperty(v4->currentContext(), qmlContext, name, IgnoreRevision, hasProperty, /*includeImports*/ true);
}

void QObjectWrapper::put(Managed *m, String *name, const ValueRef value)
{
    QObjectWrapper *that = static_cast<QObjectWrapper*>(m);
    ExecutionEngine *v4 = m->engine();

    if (v4->hasException || QQmlData::wasDeleted(that->d()->object))
        return;

    QQmlContextData *qmlContext = QV4::QmlContextWrapper::callingContext(v4);
    if (!setQmlProperty(v4->currentContext(), qmlContext, that->d()->object, name, QV4::QObjectWrapper::IgnoreRevision, value)) {
        QQmlData *ddata = QQmlData::get(that->d()->object);
        // Types created by QML are not extensible at run-time, but for other QObjects we can store them
        // as regular JavaScript properties, like on JavaScript objects.
        if (ddata && ddata->context) {
            QString error = QLatin1String("Cannot assign to non-existent property \"") +
                            name->toQString() + QLatin1Char('\"');
            v4->currentContext()->throwError(error);
        } else {
            QV4::Object::put(m, name, value);
        }
    }
}

PropertyAttributes QObjectWrapper::query(const Managed *m, String *name)
{
    const QObjectWrapper *that = static_cast<const QObjectWrapper*>(m);
    ExecutionEngine *engine = that->engine();
    QQmlContextData *qmlContext = QV4::QmlContextWrapper::callingContext(engine);
    QQmlPropertyData local;
    if (that->findProperty(engine, qmlContext, name, IgnoreRevision, &local)
        || name->equals(engine->id_destroy) || name->equals(engine->id_toString))
        return QV4::Attr_Data;
    else
        return QV4::Object::query(m, name);
}

void QObjectWrapper::advanceIterator(Managed *m, ObjectIterator *it, String *&name, uint *index, Property *p, PropertyAttributes *attributes)
{
    // Used to block access to QObject::destroyed() and QObject::deleteLater() from QML
    static const int destroyedIdx1 = QObject::staticMetaObject.indexOfSignal("destroyed(QObject*)");
    static const int destroyedIdx2 = QObject::staticMetaObject.indexOfSignal("destroyed()");
    static const int deleteLaterIdx = QObject::staticMetaObject.indexOfSlot("deleteLater()");

    name = (String *)0;
    *index = UINT_MAX;

    QObjectWrapper *that = static_cast<QObjectWrapper*>(m);

    if (that->d()->object) {
        const QMetaObject *mo = that->d()->object->metaObject();
        const int propertyCount = mo->propertyCount();
        if (it->arrayIndex < static_cast<uint>(propertyCount)) {
            name = that->engine()->newString(QString::fromUtf8(mo->property(it->arrayIndex).name()))->getPointer();
            ++it->arrayIndex;
            *attributes = QV4::Attr_Data;
            p->value = that->get(name);
            return;
        }
        const int methodCount = mo->methodCount();
        while (it->arrayIndex < static_cast<uint>(propertyCount + methodCount)) {
            const int index = it->arrayIndex - propertyCount;
            const QMetaMethod method = mo->method(index);
            ++it->arrayIndex;
            if (method.access() == QMetaMethod::Private || index == deleteLaterIdx || index == destroyedIdx1 || index == destroyedIdx2)
                continue;
            name = that->engine()->newString(QString::fromUtf8(method.name()))->getPointer();
            *attributes = QV4::Attr_Data;
            p->value = that->get(name);
            return;
        }
    }
    QV4::Object::advanceIterator(m, it, name, index, p, attributes);
}

namespace QV4 {

struct QObjectSlotDispatcher : public QtPrivate::QSlotObjectBase
{
    QV4::PersistentValue function;
    QV4::PersistentValue thisObject;
    int signalIndex;

    QObjectSlotDispatcher()
        : QtPrivate::QSlotObjectBase(&impl)
        , signalIndex(-1)
    {}

    static void impl(int which, QSlotObjectBase *this_, QObject *r, void **metaArgs, bool *ret)
    {
        switch (which) {
        case Destroy: {
            delete static_cast<QObjectSlotDispatcher*>(this_);
        }
        break;
        case Call: {
            QObjectSlotDispatcher *This = static_cast<QObjectSlotDispatcher*>(this_);
            QV4::ExecutionEngine *v4 = This->function.engine();
            // Might be that we're still connected to a signal that's emitted long
            // after the engine died. We don't track connections in a global list, so
            // we need this safeguard.
            if (!v4)
                break;

            QVarLengthArray<int, 9> dummy;
            int *argsTypes = QQmlPropertyCache::methodParameterTypes(r, This->signalIndex, dummy, 0);

            int argCount = argsTypes ? argsTypes[0]:0;

            QV4::Scope scope(v4);
            QV4::ScopedFunctionObject f(scope, This->function.value());
            QV4::ExecutionContext *ctx = v4->currentContext();

            QV4::ScopedCallData callData(scope, argCount);
            callData->thisObject = This->thisObject.isUndefined() ? v4->globalObject->asReturnedValue() : This->thisObject.value();
            for (int ii = 0; ii < argCount; ++ii) {
                int type = argsTypes[ii + 1];
                if (type == qMetaTypeId<QVariant>()) {
                    callData->args[ii] = v4->v8Engine->fromVariant(*((QVariant *)metaArgs[ii + 1]));
                } else {
                    callData->args[ii] = v4->v8Engine->fromVariant(QVariant(type, metaArgs[ii + 1]));
                }
            }

            f->call(callData);
            if (scope.hasException() && v4->v8Engine) {
                QQmlError error = QV4::ExecutionEngine::catchExceptionAsQmlError(ctx);
                if (error.description().isEmpty()) {
                    QV4::ScopedString name(scope, f->name());
                    error.setDescription(QString::fromLatin1("Unknown exception occurred during evaluation of connected function: %1").arg(name->toQString()));
                }
                if (QQmlEngine *qmlEngine = v4->v8Engine->engine()) {
                    QQmlEnginePrivate::get(qmlEngine)->warning(error);
                } else {
                    QMessageLogger(error.url().toString().toLatin1().constData(),
                                   error.line(), 0).warning().noquote()
                            << error.toString();
                }
            }
        }
        break;
        case Compare: {
            QObjectSlotDispatcher *connection = static_cast<QObjectSlotDispatcher*>(this_);
            if (connection->function.isUndefined()) {
                *ret = false;
                return;
            }

            // This is tricky. Normally the metaArgs[0] pointer is a pointer to the _function_
            // for the new-style QObject::connect. Here we use the engine pointer as sentinel
            // to distinguish those type of QSlotObjectBase connections from our QML connections.
            QV4::ExecutionEngine *v4 = reinterpret_cast<QV4::ExecutionEngine*>(metaArgs[0]);
            if (v4 != connection->function.engine()) {
                *ret = false;
                return;
            }

            QV4::Scope scope(v4);
            QV4::ScopedValue function(scope, *reinterpret_cast<QV4::Value*>(metaArgs[1]));
            QV4::ScopedValue thisObject(scope, *reinterpret_cast<QV4::Value*>(metaArgs[2]));
            QObject *receiverToDisconnect = reinterpret_cast<QObject*>(metaArgs[3]);
            int slotIndexToDisconnect = *reinterpret_cast<int*>(metaArgs[4]);

            if (slotIndexToDisconnect != -1) {
                // This is a QObject function wrapper
                if (connection->thisObject.isUndefined() == thisObject->isUndefined() &&
                        (connection->thisObject.isUndefined() || RuntimeHelpers::strictEqual(connection->thisObject, thisObject))) {

                    QV4::ScopedFunctionObject f(scope, connection->function.value());
                    QPair<QObject *, int> connectedFunctionData = extractQtMethod(f);
                    if (connectedFunctionData.first == receiverToDisconnect &&
                        connectedFunctionData.second == slotIndexToDisconnect) {
                        *ret = true;
                        return;
                    }
                }
            } else {
                // This is a normal JS function
                if (RuntimeHelpers::strictEqual(connection->function, function) &&
                        connection->thisObject.isUndefined() == thisObject->isUndefined() &&
                        (connection->thisObject.isUndefined() || RuntimeHelpers::strictEqual(connection->thisObject, thisObject))) {
                    *ret = true;
                    return;
                }
            }

            *ret = false;
        }
        break;
        case NumOperations:
        break;
        }
    };
};

} // namespace QV4

ReturnedValue QObjectWrapper::method_connect(CallContext *ctx)
{
    if (ctx->d()->callData->argc == 0)
        V4THROW_ERROR("Function.prototype.connect: no arguments given");

    QPair<QObject *, int> signalInfo = extractQtSignal(ctx->d()->callData->thisObject);
    QObject *signalObject = signalInfo.first;
    int signalIndex = signalInfo.second; // in method range, not signal range!

    if (signalIndex < 0)
        V4THROW_ERROR("Function.prototype.connect: this object is not a signal");

    if (!signalObject)
        V4THROW_ERROR("Function.prototype.connect: cannot connect to deleted QObject");

    if (signalObject->metaObject()->method(signalIndex).methodType() != QMetaMethod::Signal)
        V4THROW_ERROR("Function.prototype.connect: this object is not a signal");

    QV4::Scope scope(ctx);
    QV4::ScopedFunctionObject f(scope);
    QV4::ScopedValue thisObject (scope, QV4::Encode::undefined());

    if (ctx->d()->callData->argc == 1) {
        f = ctx->d()->callData->args[0];
    } else if (ctx->d()->callData->argc >= 2) {
        thisObject = ctx->d()->callData->args[0];
        f = ctx->d()->callData->args[1];
    }

    if (!f)
        V4THROW_ERROR("Function.prototype.connect: target is not a function");

    if (!thisObject->isUndefined() && !thisObject->isObject())
        V4THROW_ERROR("Function.prototype.connect: target this is not an object");

    QV4::QObjectSlotDispatcher *slot = new QV4::QObjectSlotDispatcher;
    slot->signalIndex = signalIndex;

    slot->thisObject = thisObject;
    slot->function = f;

    if (QQmlData *ddata = QQmlData::get(signalObject)) {
        if (QQmlPropertyCache *propertyCache = ddata->propertyCache) {
            QQmlPropertyPrivate::flushSignal(signalObject, propertyCache->methodIndexToSignalIndex(signalIndex));
        }
    }
    QObjectPrivate::connect(signalObject, signalIndex, slot, Qt::AutoConnection);

    return Encode::undefined();
}

ReturnedValue QObjectWrapper::method_disconnect(CallContext *ctx)
{
    if (ctx->d()->callData->argc == 0)
        V4THROW_ERROR("Function.prototype.disconnect: no arguments given");

    QV4::Scope scope(ctx);

    QPair<QObject *, int> signalInfo = extractQtSignal(ctx->d()->callData->thisObject);
    QObject *signalObject = signalInfo.first;
    int signalIndex = signalInfo.second;

    if (signalIndex == -1)
        V4THROW_ERROR("Function.prototype.disconnect: this object is not a signal");

    if (!signalObject)
        V4THROW_ERROR("Function.prototype.disconnect: cannot disconnect from deleted QObject");

    if (signalIndex < 0 || signalObject->metaObject()->method(signalIndex).methodType() != QMetaMethod::Signal)
        V4THROW_ERROR("Function.prototype.disconnect: this object is not a signal");

    QV4::ScopedFunctionObject functionValue(scope);
    QV4::ScopedValue functionThisValue(scope, QV4::Encode::undefined());

    if (ctx->d()->callData->argc == 1) {
        functionValue = ctx->d()->callData->args[0];
    } else if (ctx->d()->callData->argc >= 2) {
        functionThisValue = ctx->d()->callData->args[0];
        functionValue = ctx->d()->callData->args[1];
    }

    if (!functionValue)
        V4THROW_ERROR("Function.prototype.disconnect: target is not a function");

    if (!functionThisValue->isUndefined() && !functionThisValue->isObject())
        V4THROW_ERROR("Function.prototype.disconnect: target this is not an object");

    QPair<QObject *, int> functionData = extractQtMethod(functionValue);

    void *a[] = {
        ctx->d()->engine,
        functionValue.ptr,
        functionThisValue.ptr,
        functionData.first,
        &functionData.second
    };

    QObjectPrivate::disconnect(signalObject, signalIndex, reinterpret_cast<void**>(&a));

    return Encode::undefined();
}

static void markChildQObjectsRecursively(QObject *parent, QV4::ExecutionEngine *e)
{
    const QObjectList &children = parent->children();
    for (int i = 0; i < children.count(); ++i) {
        QObject *child = children.at(i);
        if (!child)
            continue;
        QQmlData *ddata = QQmlData::get(child, /*create*/false);
        if (ddata)
            ddata->jsWrapper.markOnce(e);
        markChildQObjectsRecursively(child, e);
    }
}

void QObjectWrapper::markObjects(Managed *that, QV4::ExecutionEngine *e)
{
    QObjectWrapper *This = static_cast<QObjectWrapper*>(that);

    if (QObject *o = This->d()->object.data()) {
        QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
        if (vme)
            vme->mark(e);

        // Children usually don't need to be marked, the gc keeps them alive.
        // But in the rare case of a "floating" QObject without a parent that
        // _gets_ marked (we've been called here!) then we also need to
        // propagate the marking down to the children recursively.
        if (!o->parent())
            markChildQObjectsRecursively(o, e);
    }

    QV4::Object::markObjects(that, e);
}

namespace {
    struct QObjectDeleter : public QV4::GCDeletable
    {
        QObjectDeleter(QObject *o)
            : m_objectToDelete(o)
        {}
        ~QObjectDeleter()
        {
            QQmlData *ddata = QQmlData::get(m_objectToDelete, false);
            if (ddata && ddata->ownContext && ddata->context)
                ddata->context->emitDestruction();
            // This object is notionally destroyed now
            ddata->isQueuedForDeletion = true;
            if (lastCall)
                delete m_objectToDelete;
            else
                m_objectToDelete->deleteLater();
        }

        QObject *m_objectToDelete;
    };
}

void QObjectWrapper::destroy(Managed *that)
{
    QObjectWrapper *This = static_cast<QObjectWrapper*>(that);
    QPointer<QObject> object = This->d()->object;
    ExecutionEngine *engine = This->engine();
    This->d()->~Data();
    This = 0;
    if (!object)
        return;

    QQmlData *ddata = QQmlData::get(object, false);
    if (!ddata)
        return;

    if (object->parent() || ddata->indestructible)
        return;

    QObjectDeleter *deleter = new QObjectDeleter(object);
    engine->memoryManager->registerDeletable(deleter);
}


DEFINE_OBJECT_VTABLE(QObjectWrapper);

// XXX TODO: Need to review all calls to QQmlEngine *engine() to confirm QObjects work
// correctly in a worker thread

namespace {

template<typename A, typename B, typename C, typename D, typename E,
         typename F, typename G, typename H>
class MaxSizeOf8 {
    template<typename Z, typename X>
    struct SMax {
        char dummy[sizeof(Z) > sizeof(X) ? sizeof(Z) : sizeof(X)];
    };
public:
    static const size_t Size = sizeof(SMax<A, SMax<B, SMax<C, SMax<D, SMax<E, SMax<F, SMax<G, H> > > > > > >);
};

struct CallArgument {
    inline CallArgument();
    inline ~CallArgument();
    inline void *dataPtr();

    inline void initAsType(int type);
    inline void fromValue(int type, QV8Engine *, const ValueRef);
    inline ReturnedValue toValue(QV8Engine *);

private:
    CallArgument(const CallArgument &);

    inline void cleanup();

    union {
        float floatValue;
        double doubleValue;
        quint32 intValue;
        bool boolValue;
        QObject *qobjectPtr;

        char allocData[MaxSizeOf8<QVariant,
                                QString,
                                QList<QObject *>,
                                QJSValue,
                                QQmlV4Handle,
                                QJsonArray,
                                QJsonObject,
                                QJsonValue>::Size];
        qint64 q_for_alignment;
    };

    // Pointers to allocData
    union {
        QString *qstringPtr;
        QVariant *qvariantPtr;
        QList<QObject *> *qlistPtr;
        QJSValue *qjsValuePtr;
        QQmlV4Handle *handlePtr;
        QJsonArray *jsonArrayPtr;
        QJsonObject *jsonObjectPtr;
        QJsonValue *jsonValuePtr;
    };

    int type;
};
}

static QV4::ReturnedValue CallMethod(QObject *object, int index, int returnType, int argCount,
                                        int *argTypes, QV8Engine *engine, QV4::CallData *callArgs)
{
    if (argCount > 0) {

        // Special handling is required for value types.
        // We need to save the current value in a temporary,
        // and reapply it after converting all arguments.
        // This avoids the "overwriting copy-value-type-value"
        // problem during Q_INVOKABLE function invocation.
        QQmlValueType *valueTypeObject = qobject_cast<QQmlValueType*>(object);
        QVariant valueTypeValue;
        if (valueTypeObject)
            valueTypeValue = valueTypeObject->value();

        // Convert all arguments.
        QVarLengthArray<CallArgument, 9> args(argCount + 1);
        args[0].initAsType(returnType);
        for (int ii = 0; ii < argCount; ++ii)
            args[ii + 1].fromValue(argTypes[ii], engine, callArgs->args[ii]);
        QVarLengthArray<void *, 9> argData(args.count());
        for (int ii = 0; ii < args.count(); ++ii)
            argData[ii] = args[ii].dataPtr();

        // Reinstate saved value type object value if required.
        if (valueTypeObject)
            valueTypeObject->setValue(valueTypeValue);

        QMetaObject::metacall(object, QMetaObject::InvokeMetaMethod, index, argData.data());

        return args[0].toValue(engine);

    } else if (returnType != QMetaType::Void) {

        CallArgument arg;
        arg.initAsType(returnType);

        void *args[] = { arg.dataPtr() };

        QMetaObject::metacall(object, QMetaObject::InvokeMetaMethod, index, args);

        return arg.toValue(engine);

    } else {

        void *args[] = { 0 };
        QMetaObject::metacall(object, QMetaObject::InvokeMetaMethod, index, args);
        return Encode::undefined();

    }
}

/*!
    Returns the match score for converting \a actual to be of type \a conversionType.  A
    zero score means "perfect match" whereas a higher score is worse.

    The conversion table is copied out of the \l QScript::callQtMethod() function.
*/
static int MatchScore(const QV4::ValueRef actual, int conversionType)
{
    if (actual->isNumber()) {
        switch (conversionType) {
        case QMetaType::Double:
            return 0;
        case QMetaType::Float:
            return 1;
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return 2;
        case QMetaType::Long:
        case QMetaType::ULong:
            return 3;
        case QMetaType::Int:
        case QMetaType::UInt:
            return 4;
        case QMetaType::Short:
        case QMetaType::UShort:
            return 5;
            break;
        case QMetaType::Char:
        case QMetaType::UChar:
            return 6;
        case QMetaType::QJsonValue:
            return 5;
        default:
            return 10;
        }
    } else if (actual->isString()) {
        switch (conversionType) {
        case QMetaType::QString:
            return 0;
        case QMetaType::QJsonValue:
            return 5;
        default:
            return 10;
        }
    } else if (actual->isBoolean()) {
        switch (conversionType) {
        case QMetaType::Bool:
            return 0;
        case QMetaType::QJsonValue:
            return 5;
        default:
            return 10;
        }
    } else if (actual->asDateObject()) {
        switch (conversionType) {
        case QMetaType::QDateTime:
            return 0;
        case QMetaType::QDate:
            return 1;
        case QMetaType::QTime:
            return 2;
        default:
            return 10;
        }
    } else if (actual->as<QV4::RegExpObject>()) {
        switch (conversionType) {
        case QMetaType::QRegExp:
            return 0;
        default:
            return 10;
        }
    } else if (actual->asArrayObject()) {
        switch (conversionType) {
        case QMetaType::QJsonArray:
            return 3;
        case QMetaType::QStringList:
        case QMetaType::QVariantList:
            return 5;
        case QMetaType::QVector4D:
        case QMetaType::QMatrix4x4:
            return 6;
        case QMetaType::QVector3D:
            return 7;
        default:
            return 10;
        }
    } else if (actual->isNull()) {
        switch (conversionType) {
        case QMetaType::VoidStar:
        case QMetaType::QObjectStar:
        case QMetaType::QJsonValue:
            return 0;
        default: {
            const char *typeName = QMetaType::typeName(conversionType);
            if (typeName && typeName[strlen(typeName) - 1] == '*')
                return 0;
            else
                return 10;
        }
        }
    } else if (QV4::Object *obj = actual->asObject()) {
        QV8Engine *engine = obj->engine()->v8Engine;

        if (obj->as<QV4::VariantObject>()) {
            if (conversionType == qMetaTypeId<QVariant>())
                return 0;
            if (engine->toVariant(actual, -1).userType() == conversionType)
                return 0;
            else
                return 10;
        }

        if (obj->as<QObjectWrapper>()) {
            switch (conversionType) {
            case QMetaType::QObjectStar:
                return 0;
            default:
                return 10;
            }
        }

        if (obj->as<QV4::QmlValueTypeWrapper>()) {
            if (engine->toVariant(actual, -1).userType() == conversionType)
                return 0;
            return 10;
        } else if (conversionType == QMetaType::QJsonObject) {
            return 5;
        } else {
            return 10;
        }

    } else {
        return 10;
    }
}

static inline int QMetaObject_methods(const QMetaObject *metaObject)
{
    struct Private
    {
        int revision;
        int className;
        int classInfoCount, classInfoData;
        int methodCount, methodData;
    };

    return reinterpret_cast<const Private *>(metaObject->d.data)->methodCount;
}

/*!
Returns the next related method, if one, or 0.
*/
static const QQmlPropertyData * RelatedMethod(QObject *object,
                                                      const QQmlPropertyData *current,
                                                      QQmlPropertyData &dummy)
{
    QQmlPropertyCache *cache = QQmlData::get(object)->propertyCache;
    if (!current->isOverload())
        return 0;

    Q_ASSERT(!current->overrideIndexIsProperty);

    if (cache) {
        return cache->method(current->overrideIndex);
    } else {
        const QMetaObject *mo = object->metaObject();
        int methodOffset = mo->methodCount() - QMetaObject_methods(mo);

        while (methodOffset > current->overrideIndex) {
            mo = mo->superClass();
            methodOffset -= QMetaObject_methods(mo);
        }

        // If we've been called before with the same override index, then
        // we can't go any further...
        if (&dummy == current && dummy.coreIndex == current->overrideIndex)
            return 0;

        QMetaMethod method = mo->method(current->overrideIndex);
        dummy.load(method);

        // Look for overloaded methods
        QByteArray methodName = method.name();
        for (int ii = current->overrideIndex - 1; ii >= methodOffset; --ii) {
            if (methodName == mo->method(ii).name()) {
                dummy.setFlags(dummy.getFlags() | QQmlPropertyData::IsOverload);
                dummy.overrideIndexIsProperty = 0;
                dummy.overrideIndex = ii;
                return &dummy;
            }
        }

        return &dummy;
    }
}

static QV4::ReturnedValue CallPrecise(QObject *object, const QQmlPropertyData &data,
                                         QV8Engine *engine, QV4::CallData *callArgs)
{
    QByteArray unknownTypeError;

    int returnType = QQmlPropertyCache::methodReturnType(object, data, &unknownTypeError);

    if (returnType == QMetaType::UnknownType) {
        QString typeName = QString::fromLatin1(unknownTypeError);
        QString error = QString::fromLatin1("Unknown method return type: %1").arg(typeName);
        return QV8Engine::getV4(engine)->currentContext()->throwError(error);
    }

    if (data.hasArguments()) {

        int *args = 0;
        QVarLengthArray<int, 9> dummy;

        args = QQmlPropertyCache::methodParameterTypes(object, data.coreIndex, dummy,
                                                               &unknownTypeError);

        if (!args) {
            QString typeName = QString::fromLatin1(unknownTypeError);
            QString error = QString::fromLatin1("Unknown method parameter type: %1").arg(typeName);
            return QV8Engine::getV4(engine)->currentContext()->throwError(error);
        }

        if (args[0] > callArgs->argc) {
            QString error = QLatin1String("Insufficient arguments");
            return QV8Engine::getV4(engine)->currentContext()->throwError(error);
        }

        return CallMethod(object, data.coreIndex, returnType, args[0], args + 1, engine, callArgs);

    } else {

        return CallMethod(object, data.coreIndex, returnType, 0, 0, engine, callArgs);

    }
}

/*!
Resolve the overloaded method to call.  The algorithm works conceptually like this:
    1.  Resolve the set of overloads it is *possible* to call.
        Impossible overloads include those that have too many parameters or have parameters
        of unknown type.
    2.  Filter the set of overloads to only contain those with the closest number of
        parameters.
        For example, if we are called with 3 parameters and there are 2 overloads that
        take 2 parameters and one that takes 3, eliminate the 2 parameter overloads.
    3.  Find the best remaining overload based on its match score.
        If two or more overloads have the same match score, call the last one.  The match
        score is constructed by adding the matchScore() result for each of the parameters.
*/
static QV4::ReturnedValue CallOverloaded(QObject *object, const QQmlPropertyData &data,
                                            QV8Engine *engine, QV4::CallData *callArgs)
{
    int argumentCount = callArgs->argc;

    QQmlPropertyData best;
    int bestParameterScore = INT_MAX;
    int bestMatchScore = INT_MAX;

    // Special handling is required for value types.
    // We need to save the current value in a temporary,
    // and reapply it after converting all arguments.
    // This avoids the "overwriting copy-value-type-value"
    // problem during Q_INVOKABLE function invocation.
    QQmlValueType *valueTypeObject = qobject_cast<QQmlValueType*>(object);
    QVariant valueTypeValue;
    if (valueTypeObject)
        valueTypeValue = valueTypeObject->value();

    QQmlPropertyData dummy;
    const QQmlPropertyData *attempt = &data;

    QV4::Scope scope(QV8Engine::getV4(engine));
    QV4::ScopedValue v(scope);

    do {
        QVarLengthArray<int, 9> dummy;
        int methodArgumentCount = 0;
        int *methodArgTypes = 0;
        if (attempt->hasArguments()) {
            typedef QQmlPropertyCache PC;
            int *args = PC::methodParameterTypes(object, attempt->coreIndex, dummy, 0);
            if (!args) // Must be an unknown argument
                continue;

            methodArgumentCount = args[0];
            methodArgTypes = args + 1;
        }

        if (methodArgumentCount > argumentCount)
            continue; // We don't have sufficient arguments to call this method

        int methodParameterScore = argumentCount - methodArgumentCount;
        if (methodParameterScore > bestParameterScore)
            continue; // We already have a better option

        int methodMatchScore = 0;
        for (int ii = 0; ii < methodArgumentCount; ++ii)
            methodMatchScore += MatchScore((v = callArgs->args[ii]), methodArgTypes[ii]);

        if (bestParameterScore > methodParameterScore || bestMatchScore > methodMatchScore) {
            best = *attempt;
            bestParameterScore = methodParameterScore;
            bestMatchScore = methodMatchScore;
        }

        if (bestParameterScore == 0 && bestMatchScore == 0)
            break; // We can't get better than that

    } while((attempt = RelatedMethod(object, attempt, dummy)) != 0);

    if (best.isValid()) {
        if (valueTypeObject)
            valueTypeObject->setValue(valueTypeValue);
        return CallPrecise(object, best, engine, callArgs);
    } else {
        QString error = QLatin1String("Unable to determine callable overload.  Candidates are:");
        const QQmlPropertyData *candidate = &data;
        while (candidate) {
            error += QLatin1String("\n    ") +
                     QString::fromUtf8(object->metaObject()->method(candidate->coreIndex).methodSignature().constData());
            candidate = RelatedMethod(object, candidate, dummy);
        }

        return QV8Engine::getV4(engine)->currentContext()->throwError(error);
    }
}

CallArgument::CallArgument()
: type(QVariant::Invalid)
{
}

CallArgument::~CallArgument()
{
    cleanup();
}

void CallArgument::cleanup()
{
    if (type == QMetaType::QString) {
        qstringPtr->~QString();
    } else if (type == -1 || type == QMetaType::QVariant) {
        qvariantPtr->~QVariant();
    } else if (type == qMetaTypeId<QJSValue>()) {
        qjsValuePtr->~QJSValue();
    } else if (type == qMetaTypeId<QList<QObject *> >()) {
        qlistPtr->~QList<QObject *>();
    }  else if (type == QMetaType::QJsonArray) {
        jsonArrayPtr->~QJsonArray();
    }  else if (type == QMetaType::QJsonObject) {
        jsonObjectPtr->~QJsonObject();
    }  else if (type == QMetaType::QJsonValue) {
        jsonValuePtr->~QJsonValue();
    }
}

void *CallArgument::dataPtr()
{
    if (type == -1)
        return qvariantPtr->data();
    else if (type != 0)
        return (void *)&allocData;
    return 0;
}

void CallArgument::initAsType(int callType)
{
    if (type != 0) { cleanup(); type = 0; }
    if (callType == QMetaType::UnknownType || callType == QMetaType::Void) return;

    if (callType == qMetaTypeId<QJSValue>()) {
        qjsValuePtr = new (&allocData) QJSValue();
        type = callType;
    } else if (callType == QMetaType::Int ||
               callType == QMetaType::UInt ||
               callType == QMetaType::Bool ||
               callType == QMetaType::Double ||
               callType == QMetaType::Float) {
        type = callType;
    } else if (callType == QMetaType::QObjectStar) {
        qobjectPtr = 0;
        type = callType;
    } else if (callType == QMetaType::QString) {
        qstringPtr = new (&allocData) QString();
        type = callType;
    } else if (callType == QMetaType::QVariant) {
        type = callType;
        qvariantPtr = new (&allocData) QVariant();
    } else if (callType == qMetaTypeId<QList<QObject *> >()) {
        type = callType;
        qlistPtr = new (&allocData) QList<QObject *>();
    } else if (callType == qMetaTypeId<QQmlV4Handle>()) {
        type = callType;
        handlePtr = new (&allocData) QQmlV4Handle;
    } else if (callType == QMetaType::QJsonArray) {
        type = callType;
        jsonArrayPtr = new (&allocData) QJsonArray();
    } else if (callType == QMetaType::QJsonObject) {
        type = callType;
        jsonObjectPtr = new (&allocData) QJsonObject();
    } else if (callType == QMetaType::QJsonValue) {
        type = callType;
        jsonValuePtr = new (&allocData) QJsonValue();
    } else {
        type = -1;
        qvariantPtr = new (&allocData) QVariant(callType, (void *)0);
    }
}

void CallArgument::fromValue(int callType, QV8Engine *engine, const QV4::ValueRef value)
{
    if (type != 0) {
        cleanup();
        type = 0;
    }

    QV4::Scope scope(QV8Engine::getV4(engine));

    bool queryEngine = false;
    if (callType == qMetaTypeId<QJSValue>()) {
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
        qjsValuePtr = new (&allocData) QJSValue(new QJSValuePrivate(v4, value));
        type = qMetaTypeId<QJSValue>();
    } else if (callType == QMetaType::Int) {
        intValue = quint32(value->toInt32());
        type = callType;
    } else if (callType == QMetaType::UInt) {
        intValue = quint32(value->toUInt32());
        type = callType;
    } else if (callType == QMetaType::Bool) {
        boolValue = value->toBoolean();
        type = callType;
    } else if (callType == QMetaType::Double) {
        doubleValue = double(value->toNumber());
        type = callType;
    } else if (callType == QMetaType::Float) {
        floatValue = float(value->toNumber());
        type = callType;
    } else if (callType == QMetaType::QString) {
        if (value->isNull() || value->isUndefined())
            qstringPtr = new (&allocData) QString();
        else
            qstringPtr = new (&allocData) QString(value->toQStringNoThrow());
        type = callType;
    } else if (callType == QMetaType::QObjectStar) {
        qobjectPtr = 0;
        if (QV4::QObjectWrapper *qobjectWrapper = value->as<QV4::QObjectWrapper>())
            qobjectPtr = qobjectWrapper->object();
        else if (QV4::QmlTypeWrapper *qmlTypeWrapper = value->as<QV4::QmlTypeWrapper>())
            queryEngine = qmlTypeWrapper->isSingleton();
        type = callType;
    } else if (callType == qMetaTypeId<QVariant>()) {
        qvariantPtr = new (&allocData) QVariant(engine->toVariant(value, -1));
        type = callType;
    } else if (callType == qMetaTypeId<QList<QObject*> >()) {
        qlistPtr = new (&allocData) QList<QObject *>();
        QV4::ScopedArrayObject array(scope, value);
        if (array) {
            Scoped<QV4::QObjectWrapper> qobjectWrapper(scope);

            uint32_t length = array->getLength();
            for (uint32_t ii = 0; ii < length; ++ii)  {
                QObject *o = 0;
                qobjectWrapper = array->getIndexed(ii);
                if (!!qobjectWrapper)
                    o = qobjectWrapper->object();
                qlistPtr->append(o);
            }
        } else {
            QObject *o = 0;
            if (QV4::QObjectWrapper *qobjectWrapper = value->as<QV4::QObjectWrapper>())
                o = qobjectWrapper->object();
            qlistPtr->append(o);
        }
        type = callType;
    } else if (callType == qMetaTypeId<QQmlV4Handle>()) {
        handlePtr = new (&allocData) QQmlV4Handle(value->asReturnedValue());
        type = callType;
    } else if (callType == QMetaType::QJsonArray) {
        QV4::ScopedArrayObject a(scope, value);
        jsonArrayPtr = new (&allocData) QJsonArray(QV4::JsonObject::toJsonArray(a));
        type = callType;
    } else if (callType == QMetaType::QJsonObject) {
        QV4::ScopedObject o(scope, value);
        jsonObjectPtr = new (&allocData) QJsonObject(QV4::JsonObject::toJsonObject(o));
        type = callType;
    } else if (callType == QMetaType::QJsonValue) {
        jsonValuePtr = new (&allocData) QJsonValue(QV4::JsonObject::toJsonValue(value));
        type = callType;
    } else if (callType == QMetaType::Void) {
        *qvariantPtr = QVariant();
    } else {
        queryEngine = true;
    }

    if (queryEngine) {
        qvariantPtr = new (&allocData) QVariant();
        type = -1;

        QQmlEnginePrivate *ep = engine->engine() ? QQmlEnginePrivate::get(engine->engine()) : 0;
        QVariant v = engine->toVariant(value, callType);

        if (v.userType() == callType) {
            *qvariantPtr = v;
        } else if (v.canConvert(callType)) {
            *qvariantPtr = v;
            qvariantPtr->convert(callType);
        } else {
            QQmlMetaObject mo = ep ? ep->rawMetaObjectForType(callType) : QQmlMetaObject();
            if (!mo.isNull()) {
                QObject *obj = ep->toQObject(v);

                if (obj != 0 && !QQmlMetaObject::canConvert(obj, mo))
                    obj = 0;

                *qvariantPtr = QVariant(callType, &obj);
            } else {
                *qvariantPtr = QVariant(callType, (void *)0);
            }
        }
    }
}

QV4::ReturnedValue CallArgument::toValue(QV8Engine *engine)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    QV4::Scope scope(v4);

    if (type == qMetaTypeId<QJSValue>()) {
        return QJSValuePrivate::get(*qjsValuePtr)->getValue(v4);
    } else if (type == QMetaType::Int) {
        return QV4::Encode(int(intValue));
    } else if (type == QMetaType::UInt) {
        return QV4::Encode((uint)intValue);
    } else if (type == QMetaType::Bool) {
        return QV4::Encode(boolValue);
    } else if (type == QMetaType::Double) {
        return QV4::Encode(doubleValue);
    } else if (type == QMetaType::Float) {
        return QV4::Encode(floatValue);
    } else if (type == QMetaType::QString) {
        return engine->toString(*qstringPtr);
    } else if (type == QMetaType::QObjectStar) {
        QObject *object = qobjectPtr;
        if (object)
            QQmlData::get(object, true)->setImplicitDestructible();
        return QV4::QObjectWrapper::wrap(v4, object);
    } else if (type == qMetaTypeId<QList<QObject *> >()) {
        // XXX Can this be made more by using Array as a prototype and implementing
        // directly against QList<QObject*>?
        QList<QObject *> &list = *qlistPtr;
        QV4::Scoped<ArrayObject> array(scope, v4->newArrayObject());
        array->arrayReserve(list.count());
        QV4::ScopedValue v(scope);
        for (int ii = 0; ii < list.count(); ++ii)
            array->arrayPut(ii, (v = QV4::QObjectWrapper::wrap(v4, list.at(ii))));
        array->setArrayLengthUnchecked(list.count());
        return array.asReturnedValue();
    } else if (type == qMetaTypeId<QQmlV4Handle>()) {
        return *handlePtr;
    } else if (type == QMetaType::QJsonArray) {
        return QV4::JsonObject::fromJsonArray(v4, *jsonArrayPtr);
    } else if (type == QMetaType::QJsonObject) {
        return QV4::JsonObject::fromJsonObject(v4, *jsonObjectPtr);
    } else if (type == QMetaType::QJsonValue) {
        return QV4::JsonObject::fromJsonValue(v4, *jsonValuePtr);
    } else if (type == -1 || type == qMetaTypeId<QVariant>()) {
        QVariant value = *qvariantPtr;
        QV4::ScopedValue rv(scope, engine->fromVariant(value));
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, rv);
        if (!!qobjectWrapper) {
            if (QObject *object = qobjectWrapper->object())
                QQmlData::get(object, true)->setImplicitDestructible();
        }
        return rv.asReturnedValue();
    } else {
        return QV4::Encode::undefined();
    }
}

ReturnedValue QObjectMethod::create(ExecutionContext *scope, QObject *object, int index, const ValueRef qmlGlobal)
{
    return (scope->d()->engine->memoryManager->alloc<QObjectMethod>(scope, object, index, qmlGlobal))->asReturnedValue();
}

QObjectMethod::Data::Data(ExecutionContext *scope, QObject *object, int index, const ValueRef qmlGlobal)
    : FunctionObject::Data(scope)
    , object(object)
    , index(index)
    , qmlGlobal(qmlGlobal)
{
    setVTable(staticVTable());
    subtype = WrappedQtMethod;
}

QV4::ReturnedValue QObjectMethod::method_toString(QV4::ExecutionContext *ctx)
{
    QString result;
    if (d()->object) {
        QString objectName = d()->object->objectName();

        result += QString::fromUtf8(d()->object->metaObject()->className());
        result += QLatin1String("(0x");
        result += QString::number((quintptr)d()->object.data(),16);

        if (!objectName.isEmpty()) {
            result += QLatin1String(", \"");
            result += objectName;
            result += QLatin1Char('\"');
        }

        result += QLatin1Char(')');
    } else {
        result = QLatin1String("null");
    }

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

QV4::ReturnedValue QObjectMethod::method_destroy(QV4::ExecutionContext *ctx, const ValueRef args, int argc)
{
    if (!d()->object)
        return Encode::undefined();
    if (QQmlData::keepAliveDuringGarbageCollection(d()->object))
        return ctx->throwError(QStringLiteral("Invalid attempt to destroy() an indestructible object"));

    int delay = 0;
    if (argc > 0)
        delay = args[0].toUInt32();

    if (delay > 0)
        QTimer::singleShot(delay, d()->object, SLOT(deleteLater()));
    else
        d()->object->deleteLater();

    return Encode::undefined();
}

ReturnedValue QObjectMethod::call(Managed *m, CallData *callData)
{
    QObjectMethod *This = static_cast<QObjectMethod*>(m);
    return This->callInternal(callData);
}

ReturnedValue QObjectMethod::callInternal(CallData *callData)
{
    ExecutionContext *context = engine()->currentContext();
    if (d()->index == DestroyMethod)
        return method_destroy(context, callData->args, callData->argc);
    else if (d()->index == ToStringMethod)
        return method_toString(context);

    QObject *object = d()->object.data();
    if (!object)
        return Encode::undefined();

    QQmlData *ddata = QQmlData::get(object);
    if (!ddata)
        return Encode::undefined();

    QV8Engine *v8Engine = context->d()->engine->v8Engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8Engine);
    QV4::Scope scope(v4);

    QQmlPropertyData method;

    if (QQmlData *ddata = static_cast<QQmlData *>(QObjectPrivate::get(object)->declarativeData)) {
        if (ddata->propertyCache) {
            QQmlPropertyData *data = ddata->propertyCache->method(d()->index);
            if (!data)
                return QV4::Encode::undefined();
            method = *data;
        }
    }

    if (method.coreIndex == -1) {
        const QMetaObject *mo = object->metaObject();
        const QMetaMethod moMethod = mo->method(d()->index);
        method.load(moMethod);

        if (method.coreIndex == -1)
            return QV4::Encode::undefined();

        // Look for overloaded methods
        QByteArray methodName = moMethod.name();
        const int methodOffset = mo->methodOffset();
        for (int ii = d()->index - 1; ii >= methodOffset; --ii) {
            if (methodName == mo->method(ii).name()) {
                method.setFlags(method.getFlags() | QQmlPropertyData::IsOverload);
                method.overrideIndexIsProperty = 0;
                method.overrideIndex = ii;
                break;
            }
        }
    }

    if (method.isV4Function()) {
        QV4::ScopedValue rv(scope, QV4::Primitive::undefinedValue());

        QV4::ScopedValue qmlGlobal(scope, d()->qmlGlobal.value());
        QQmlV4Function func(callData, rv, qmlGlobal,
                            QmlContextWrapper::getContext(qmlGlobal),
                            v8Engine);
        QQmlV4Function *funcptr = &func;

        void *args[] = { 0, &funcptr };
        QMetaObject::metacall(object, QMetaObject::InvokeMetaMethod, method.coreIndex, args);

        return rv.asReturnedValue();
    }

    if (!method.isOverload()) {
        return CallPrecise(object, method, v8Engine, callData);
    } else {
        return CallOverloaded(object, method, v8Engine, callData);
    }
}

DEFINE_OBJECT_VTABLE(QObjectMethod);

QmlSignalHandler::Data::Data(ExecutionEngine *engine, QObject *object, int signalIndex)
    : Object::Data(engine)
    , object(object)
    , signalIndex(signalIndex)
{
    setVTable(staticVTable());
}

DEFINE_OBJECT_VTABLE(QmlSignalHandler);

void MultiplyWrappedQObjectMap::insert(QObject *key, Object *value)
{
    QHash<QObject*, Object*>::insert(key, value);
    connect(key, SIGNAL(destroyed(QObject*)), this, SLOT(removeDestroyedObject(QObject*)));
}

MultiplyWrappedQObjectMap::Iterator MultiplyWrappedQObjectMap::erase(MultiplyWrappedQObjectMap::Iterator it)
{
    disconnect(it.key(), SIGNAL(destroyed(QObject*)), this, SLOT(removeDestroyedObject(QObject*)));
    return QHash<QObject*, Object*>::erase(it);
}

void MultiplyWrappedQObjectMap::remove(QObject *key)
{
    Iterator it = find(key);
    if (it == end())
        return;
    erase(it);
}

void MultiplyWrappedQObjectMap::removeDestroyedObject(QObject *object)
{
    QHash<QObject*, Object*>::remove(object);
}

QT_END_NAMESPACE

