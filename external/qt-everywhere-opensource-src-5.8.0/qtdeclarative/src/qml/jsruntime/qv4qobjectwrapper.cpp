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

#include "qv4qobjectwrapper_p.h"

#include <private/qqmlpropertycache_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qjsvalue_p.h>
#include <private/qqmlexpression_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmltypewrapper_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmllistwrapper_p.h>
#include <private/qqmlbuiltinfunctions_p.h>
#include <private/qv8engine_p.h>

#include <private/qv4arraybuffer_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4runtime_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4sequenceobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4jsonobject_p.h>
#include <private/qv4regexpobject_p.h>
#include <private/qv4dateobject_p.h>
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
#include <QtCore/qmetaobject.h>

QT_BEGIN_NAMESPACE

// The code in this file does not violate strict aliasing, but GCC thinks it does
// so turn off the warnings for us to have a clean build
QT_WARNING_DISABLE_GCC("-Wstrict-aliasing")

using namespace QV4;

QPair<QObject *, int> QObjectMethod::extractQtMethod(const QV4::FunctionObject *function)
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

static QPair<QObject *, int> extractQtSignal(const Value &value)
{
    if (value.isObject()) {
        QV4::ExecutionEngine *v4 = value.as<Object>()->engine();
        QV4::Scope scope(v4);
        QV4::ScopedFunctionObject function(scope, value);
        if (function)
            return QObjectMethod::extractQtMethod(function);

        QV4::Scoped<QV4::QmlSignalHandler> handler(scope, value);
        if (handler)
            return qMakePair(handler->object(), handler->signalIndex());
    }

    return qMakePair((QObject *)0, -1);
}

static QV4::ReturnedValue loadProperty(QV4::ExecutionEngine *v4, QObject *object,
                                       const QQmlPropertyData &property)
{
    Q_ASSERT(!property.isFunction());
    QV4::Scope scope(v4);

    if (property.isQObject()) {
        QObject *rv = 0;
        property.readProperty(object, &rv);
        return QV4::QObjectWrapper::wrap(v4, rv);
    } else if (property.isQList()) {
        return QmlListWrapper::create(v4, object, property.coreIndex(), property.propType());
    } else if (property.propType() == QMetaType::QReal) {
        qreal v = 0;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.propType() == QMetaType::Int || property.isEnum()) {
        int v = 0;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.propType() == QMetaType::Bool) {
        bool v = false;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.propType() == QMetaType::QString) {
        QString v;
        property.readProperty(object, &v);
        return v4->newString(v)->asReturnedValue();
    } else if (property.propType() == QMetaType::UInt) {
        uint v = 0;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.propType() == QMetaType::Float) {
        float v = 0;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.propType() == QMetaType::Double) {
        double v = 0;
        property.readProperty(object, &v);
        return QV4::Encode(v);
    } else if (property.isV4Handle()) {
        QQmlV4Handle handle;
        property.readProperty(object, &handle);
        return handle;
    } else if (property.propType() == qMetaTypeId<QJSValue>()) {
        QJSValue v;
        property.readProperty(object, &v);
        return QJSValuePrivate::convertedToValue(v4, v);
    } else if (property.isQVariant()) {
        QVariant v;
        property.readProperty(object, &v);

        if (QQmlValueTypeFactory::isValueType(v.userType())) {
            if (const QMetaObject *valueTypeMetaObject = QQmlValueTypeFactory::metaObjectForMetaType(v.userType()))
                return QV4::QQmlValueTypeWrapper::create(v4, object, property.coreIndex(), valueTypeMetaObject, v.userType()); // VariantReference value-type.
        }

        return scope.engine->fromVariant(v);
    } else if (QQmlValueTypeFactory::isValueType(property.propType())) {
        if (const QMetaObject *valueTypeMetaObject = QQmlValueTypeFactory::metaObjectForMetaType(property.propType()))
            return QV4::QQmlValueTypeWrapper::create(v4, object, property.coreIndex(), valueTypeMetaObject, property.propType());
    } else {
        // see if it's a sequence type
        bool succeeded = false;
        QV4::ScopedValue retn(scope, QV4::SequencePrototype::newSequence(v4, property.propType(), object, property.coreIndex(), &succeeded));
        if (succeeded)
            return retn->asReturnedValue();
    }

    if (property.propType() == QMetaType::UnknownType) {
        QMetaProperty p = object->metaObject()->property(property.coreIndex());
        qWarning("QMetaProperty::read: Unable to handle unregistered datatype '%s' for property "
                 "'%s::%s'", p.typeName(), object->metaObject()->className(), p.name());
        return QV4::Encode::undefined();
    } else {
        QVariant v(property.propType(), (void *)0);
        property.readProperty(object, v.data());
        return scope.engine->fromVariant(v);
    }
}

void QObjectWrapper::initializeBindings(ExecutionEngine *engine)
{
    engine->functionPrototype()->defineDefaultProperty(QStringLiteral("connect"), method_connect);
    engine->functionPrototype()->defineDefaultProperty(QStringLiteral("disconnect"), method_disconnect);
}

QQmlPropertyData *QObjectWrapper::findProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local) const
{
    Q_UNUSED(revisionMode);

    QQmlData *ddata = QQmlData::get(d()->object(), false);
    if (!ddata)
        return 0;
    QQmlPropertyData *result = 0;
    if (ddata && ddata->propertyCache)
        result = ddata->propertyCache->property(name, d()->object(), qmlContext);
    else
        result = QQmlPropertyCache::property(engine->jsEngine(), d()->object(), name, qmlContext, *local);
    return result;
}

ReturnedValue QObjectWrapper::getQmlProperty(QQmlContextData *qmlContext, String *name, QObjectWrapper::RevisionMode revisionMode,
                                             bool *hasProperty, bool includeImports) const
{
    if (QQmlData::wasDeleted(d()->object())) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::undefined();
    }

    ExecutionEngine *v4 = engine();

    if (name->equals(v4->id_destroy()) || name->equals(v4->id_toString())) {
        int index = name->equals(v4->id_destroy()) ? QV4::QObjectMethod::DestroyMethod : QV4::QObjectMethod::ToStringMethod;
        if (hasProperty)
            *hasProperty = true;
        ExecutionContext *global = v4->rootContext();
        return QV4::QObjectMethod::create(global, d()->object(), index);
    }

    QQmlPropertyData local;
    QQmlPropertyData *result = findProperty(v4, qmlContext, name, revisionMode, &local);

    if (!result) {
        if (includeImports && name->startsWithUpper()) {
            // Check for attached properties
            if (qmlContext && qmlContext->imports) {
                QQmlTypeNameCache::Result r = qmlContext->imports->query(name);

                if (hasProperty)
                    *hasProperty = true;

                if (r.isValid()) {
                    if (r.scriptIndex != -1) {
                        return QV4::Encode::undefined();
                    } else if (r.type) {
                        return QmlTypeWrapper::create(v4, d()->object(),
                                                      r.type, Heap::QmlTypeWrapper::ExcludeEnums);
                    } else if (r.importNamespace) {
                        return QmlTypeWrapper::create(v4, d()->object(),
                                                      qmlContext->imports, r.importNamespace, Heap::QmlTypeWrapper::ExcludeEnums);
                    }
                    Q_ASSERT(!"Unreachable");
                }
            }
        }
        return QV4::Object::get(this, name, hasProperty);
    }

    QQmlData *ddata = QQmlData::get(d()->object(), false);

    if (revisionMode == QV4::QObjectWrapper::CheckRevision && result->hasRevision()) {
        if (ddata && ddata->propertyCache && !ddata->propertyCache->isAllowedInRevision(result)) {
            if (hasProperty)
                *hasProperty = false;
            return QV4::Encode::undefined();
        }
    }

    if (hasProperty)
        *hasProperty = true;

    return getProperty(v4, d()->object(), result);
}

ReturnedValue QObjectWrapper::getProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property, bool captureRequired)
{
    QQmlData::flushPendingBinding(object, QQmlPropertyIndex(property->coreIndex()));

    if (property->isFunction() && !property->isVarProperty()) {
        if (property->isVMEFunction()) {
            QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
            Q_ASSERT(vmemo);
            return vmemo->vmeMethod(property->coreIndex());
        } else if (property->isV4Function()) {
            Scope scope(engine);
            ScopedContext global(scope, engine->qmlContext());
            if (!global)
                global = engine->rootContext();
            return QV4::QObjectMethod::create(global, object, property->coreIndex());
        } else if (property->isSignalHandler()) {
            QmlSignalHandler::initProto(engine);
            return engine->memoryManager->allocObject<QV4::QmlSignalHandler>(object, property->coreIndex())->asReturnedValue();
        } else {
            ExecutionContext *global = engine->rootContext();
            return QV4::QObjectMethod::create(global, object, property->coreIndex());
        }
    }

    QQmlEnginePrivate *ep = engine->qmlEngine() ? QQmlEnginePrivate::get(engine->qmlEngine()) : 0;

    if (captureRequired && ep && ep->propertyCapture && !property->isConstant())
        ep->propertyCapture->captureProperty(object, property->coreIndex(), property->notifyIndex());

    if (property->isVarProperty()) {
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        return vmemo->vmeProperty(property->coreIndex());
    } else {
        return loadProperty(engine, object, *property);
    }
}

ReturnedValue QObjectWrapper::getQmlProperty(QV4::ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name, QObjectWrapper::RevisionMode revisionMode, bool *hasProperty)
{
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

    QV4::Scope scope(engine);
    QV4::Scoped<QObjectWrapper> wrapper(scope, wrap(engine, object));
    if (!wrapper) {
        if (hasProperty)
            *hasProperty = false;
        return QV4::Encode::null();
    }
    return wrapper->getQmlProperty(qmlContext, name, revisionMode, hasProperty);
}

bool QObjectWrapper::setQmlProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name,
                                    QObjectWrapper::RevisionMode revisionMode, const Value &value)
{
    if (QQmlData::wasDeleted(object))
        return false;

    QQmlPropertyData local;
    QQmlPropertyData *result = QQmlPropertyCache::property(engine->jsEngine(), object, name, qmlContext, local);
    if (!result)
        return false;

    if (revisionMode == QV4::QObjectWrapper::CheckRevision && result->hasRevision()) {
        QQmlData *ddata = QQmlData::get(object);
        if (ddata && ddata->propertyCache && !ddata->propertyCache->isAllowedInRevision(result))
            return false;
    }

    setProperty(engine, object, result, value);
    return true;
}

void QObjectWrapper::setProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property, const Value &value)
{
    if (!property->isWritable() && !property->isQList()) {
        QString error = QLatin1String("Cannot assign to read-only property \"") +
                        property->name(object) + QLatin1Char('\"');
        engine->throwTypeError(error);
        return;
    }

    QQmlBinding *newBinding = 0;
    QV4::Scope scope(engine);
    QV4::ScopedFunctionObject f(scope, value);
    if (f) {
        if (!f->isBinding()) {
            if (!property->isVarProperty() && property->propType() != qMetaTypeId<QJSValue>()) {
                // assigning a JS function to a non var or QJSValue property or is not allowed.
                QString error = QLatin1String("Cannot assign JavaScript function to ");
                if (!QMetaType::typeName(property->propType()))
                    error += QLatin1String("[unknown property type]");
                else
                    error += QLatin1String(QMetaType::typeName(property->propType()));
                scope.engine->throwError(error);
                return;
            }
        } else {
            // binding assignment.
            QQmlContextData *callingQmlContext = scope.engine->callingQmlContext();

            QV4::Scoped<QQmlBindingFunction> bindingFunction(scope, (const Value &)f);
            bindingFunction->initBindingLocation();

            newBinding = QQmlBinding::create(property, value, object, callingQmlContext);
            newBinding->setTarget(object, *property, nullptr);
        }
    }

    if (newBinding)
        QQmlPropertyPrivate::setBinding(newBinding);
    else
        QQmlPropertyPrivate::removeBinding(object, QQmlPropertyIndex(property->coreIndex()));

    if (!newBinding && property->isVarProperty()) {
        // allow assignment of "special" values (null, undefined, function) to var properties
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        vmemo->setVMEProperty(property->coreIndex(), value);
        return;
    }

#define PROPERTY_STORE(cpptype, value) \
    cpptype o = value; \
    int status = -1; \
    int flags = 0; \
    void *argv[] = { &o, 0, &status, &flags }; \
    QMetaObject::metacall(object, QMetaObject::WriteProperty, property->coreIndex(), argv);

    if (value.isNull() && property->isQObject()) {
        PROPERTY_STORE(QObject*, 0);
    } else if (value.isUndefined() && property->isResettable()) {
        void *a[] = { 0 };
        QMetaObject::metacall(object, QMetaObject::ResetProperty, property->coreIndex(), a);
    } else if (value.isUndefined() && property->propType() == qMetaTypeId<QVariant>()) {
        PROPERTY_STORE(QVariant, QVariant());
    } else if (value.isUndefined() && property->propType() == QMetaType::QJsonValue) {
        PROPERTY_STORE(QJsonValue, QJsonValue(QJsonValue::Undefined));
    } else if (!newBinding && property->propType() == qMetaTypeId<QJSValue>()) {
        PROPERTY_STORE(QJSValue, QJSValue(scope.engine, value.asReturnedValue()));
    } else if (value.isUndefined() && property->propType() != qMetaTypeId<QQmlScriptString>()) {
        QString error = QLatin1String("Cannot assign [undefined] to ");
        if (!QMetaType::typeName(property->propType()))
            error += QLatin1String("[unknown property type]");
        else
            error += QLatin1String(QMetaType::typeName(property->propType()));
        scope.engine->throwError(error);
        return;
    } else if (value.as<FunctionObject>()) {
        // this is handled by the binding creation above
    } else if (property->propType() == QMetaType::Int && value.isNumber()) {
        PROPERTY_STORE(int, value.asDouble());
    } else if (property->propType() == QMetaType::QReal && value.isNumber()) {
        PROPERTY_STORE(qreal, qreal(value.asDouble()));
    } else if (property->propType() == QMetaType::Float && value.isNumber()) {
        PROPERTY_STORE(float, float(value.asDouble()));
    } else if (property->propType() == QMetaType::Double && value.isNumber()) {
        PROPERTY_STORE(double, double(value.asDouble()));
    } else if (property->propType() == QMetaType::QString && value.isString()) {
        PROPERTY_STORE(QString, value.toQStringNoThrow());
    } else if (property->isVarProperty()) {
        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(object);
        Q_ASSERT(vmemo);
        vmemo->setVMEProperty(property->coreIndex(), value);
    } else if (property->propType() == qMetaTypeId<QQmlScriptString>() && (value.isUndefined() || value.isPrimitive())) {
        QQmlScriptString ss(value.toQStringNoThrow(), 0 /* context */, object);
        if (value.isNumber()) {
            ss.d->numberValue = value.toNumber();
            ss.d->isNumberLiteral = true;
        } else if (value.isString()) {
            ss.d->script = QV4::CompiledData::Binding::escapedString(ss.d->script);
            ss.d->isStringLiteral = true;
        }
        PROPERTY_STORE(QQmlScriptString, ss);
    } else {
        QVariant v;
        if (property->isQList())
            v = scope.engine->toVariant(value, qMetaTypeId<QList<QObject *> >());
        else
            v = scope.engine->toVariant(value, property->propType());

        QQmlContextData *callingQmlContext = scope.engine->callingQmlContext();
        if (!QQmlPropertyPrivate::write(object, *property, v, callingQmlContext)) {
            const char *valueType = 0;
            if (v.userType() == QVariant::Invalid) valueType = "null";
            else valueType = QMetaType::typeName(v.userType());

            const char *targetTypeName = QMetaType::typeName(property->propType());
            if (!targetTypeName)
                targetTypeName = "an unregistered type";

            QString error = QLatin1String("Cannot assign ") +
                            QLatin1String(valueType) +
                            QLatin1String(" to ") +
                            QLatin1String(targetTypeName);
            scope.engine->throwError(error);
            return;
        }
    }
}

ReturnedValue QObjectWrapper::wrap_slowPath(ExecutionEngine *engine, QObject *object)
{
    if (QQmlData::wasDeleted(object))
        return QV4::Encode::null();

    QQmlData *ddata = QQmlData::get(object, true);
    if (!ddata)
        return QV4::Encode::undefined();

    Scope scope(engine);

    if (ddata->jsWrapper.isUndefined() &&
               (ddata->jsEngineId == engine->m_engineId || // We own the QObject
                ddata->jsEngineId == 0 ||    // No one owns the QObject
                !ddata->hasTaintedV4Object)) { // Someone else has used the QObject, but it isn't tainted

        QV4::ScopedValue rv(scope, create(engine, object));
        ddata->jsWrapper.set(scope.engine, rv);
        ddata->jsEngineId = engine->m_engineId;
        return rv->asReturnedValue();

    } else {
        // If this object is tainted, we have to check to see if it is in our
        // tainted object list
        ScopedObject alternateWrapper(scope, (Object *)0);
        if (engine->m_multiplyWrappedQObjects && ddata->hasTaintedV4Object)
            alternateWrapper = engine->m_multiplyWrappedQObjects->value(object);

        // If our tainted handle doesn't exist or has been collected, and there isn't
        // a handle in the ddata, we can assume ownership of the ddata->jsWrapper
        if (ddata->jsWrapper.isUndefined() && !alternateWrapper) {
            QV4::ScopedValue result(scope, create(engine, object));
            ddata->jsWrapper.set(scope.engine, result);
            ddata->jsEngineId = engine->m_engineId;
            return result->asReturnedValue();
        }

        if (!alternateWrapper) {
            alternateWrapper = create(engine, object);
            if (!engine->m_multiplyWrappedQObjects)
                engine->m_multiplyWrappedQObjects = new MultiplyWrappedQObjectMap;
            engine->m_multiplyWrappedQObjects->insert(object, alternateWrapper->d());
            ddata->hasTaintedV4Object = true;
        }

        return alternateWrapper.asReturnedValue();
    }
}

void QObjectWrapper::markWrapper(QObject *object, ExecutionEngine *engine)
{
    if (QQmlData::wasDeleted(object))
        return;

    QQmlData *ddata = QQmlData::get(object);
    if (!ddata)
        return;

    if (ddata->jsEngineId == engine->m_engineId)
        ddata->jsWrapper.markOnce(engine);
    else  if (engine->m_multiplyWrappedQObjects && ddata->hasTaintedV4Object)
        engine->m_multiplyWrappedQObjects->mark(object, engine);
}

ReturnedValue QObjectWrapper::getProperty(ExecutionEngine *engine, QObject *object, int propertyIndex, bool captureRequired)
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
    return getProperty(engine, object, property, captureRequired);
}

void QObjectWrapper::setProperty(ExecutionEngine *engine, int propertyIndex, const Value &value)
{
    setProperty(engine, d()->object(), propertyIndex, value);
}

void QObjectWrapper::setProperty(ExecutionEngine *engine, QObject *object, int propertyIndex, const Value &value)
{
    Q_ASSERT(propertyIndex < 0xffff);
    Q_ASSERT(propertyIndex >= 0);

    if (QQmlData::wasDeleted(object))
        return;
    QQmlData *ddata = QQmlData::get(object, /*create*/false);
    if (!ddata)
        return;

    QQmlPropertyCache *cache = ddata->propertyCache;
    Q_ASSERT(cache);
    QQmlPropertyData *property = cache->property(propertyIndex);
    Q_ASSERT(property); // We resolved this property earlier, so it better exist!
    return setProperty(engine, object, property, value);
}

bool QObjectWrapper::isEqualTo(Managed *a, Managed *b)
{
    Q_ASSERT(a->as<QV4::QObjectWrapper>());
    QV4::QObjectWrapper *qobjectWrapper = static_cast<QV4::QObjectWrapper *>(a);
    QV4::Object *o = b->as<Object>();
    if (o) {
        if (QV4::QmlTypeWrapper *qmlTypeWrapper = o->as<QV4::QmlTypeWrapper>())
            return qmlTypeWrapper->toVariant().value<QObject*>() == qobjectWrapper->object();
    }

    return false;
}

ReturnedValue QObjectWrapper::create(ExecutionEngine *engine, QObject *object)
{
    if (QJSEngine *jsEngine = engine->jsEngine()) {
        if (QQmlPropertyCache *cache = QQmlData::ensurePropertyCache(jsEngine, object)) {
            ReturnedValue result = QV4::Encode::null();
            void *args[] = { &result, &engine };
            if (cache->callJSFactoryMethod(object, args))
                return result;
        }
    }
    return (engine->memoryManager->allocObject<QV4::QObjectWrapper>(object))->asReturnedValue();
}

QV4::ReturnedValue QObjectWrapper::get(const Managed *m, String *name, bool *hasProperty)
{
    const QObjectWrapper *that = static_cast<const QObjectWrapper*>(m);
    QQmlContextData *qmlContext = that->engine()->callingQmlContext();
    return that->getQmlProperty(qmlContext, name, IgnoreRevision, hasProperty, /*includeImports*/ true);
}

void QObjectWrapper::put(Managed *m, String *name, const Value &value)
{
    QObjectWrapper *that = static_cast<QObjectWrapper*>(m);
    ExecutionEngine *v4 = that->engine();

    if (v4->hasException || QQmlData::wasDeleted(that->d()->object()))
        return;

    QQmlContextData *qmlContext = v4->callingQmlContext();
    if (!setQmlProperty(v4, qmlContext, that->d()->object(), name, QV4::QObjectWrapper::IgnoreRevision, value)) {
        QQmlData *ddata = QQmlData::get(that->d()->object());
        // Types created by QML are not extensible at run-time, but for other QObjects we can store them
        // as regular JavaScript properties, like on JavaScript objects.
        if (ddata && ddata->context) {
            QString error = QLatin1String("Cannot assign to non-existent property \"") +
                            name->toQString() + QLatin1Char('\"');
            v4->throwError(error);
        } else {
            QV4::Object::put(m, name, value);
        }
    }
}

PropertyAttributes QObjectWrapper::query(const Managed *m, String *name)
{
    const QObjectWrapper *that = static_cast<const QObjectWrapper*>(m);
    ExecutionEngine *engine = that->engine();
    QQmlContextData *qmlContext = engine->callingQmlContext();
    QQmlPropertyData local;
    if (that->findProperty(engine, qmlContext, name, IgnoreRevision, &local)
        || name->equals(engine->id_destroy()) || name->equals(engine->id_toString()))
        return QV4::Attr_Data;
    else
        return QV4::Object::query(m, name);
}

void QObjectWrapper::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes)
{
    // Used to block access to QObject::destroyed() and QObject::deleteLater() from QML
    static const int destroyedIdx1 = QObject::staticMetaObject.indexOfSignal("destroyed(QObject*)");
    static const int destroyedIdx2 = QObject::staticMetaObject.indexOfSignal("destroyed()");
    static const int deleteLaterIdx = QObject::staticMetaObject.indexOfSlot("deleteLater()");

    name->setM(0);
    *index = UINT_MAX;

    QObjectWrapper *that = static_cast<QObjectWrapper*>(m);

    if (that->d()->object()) {
        const QMetaObject *mo = that->d()->object()->metaObject();
        // These indices don't apply to gadgets, so don't block them.
        const bool preventDestruction = mo->superClass() || mo == &QObject::staticMetaObject;
        const int propertyCount = mo->propertyCount();
        if (it->arrayIndex < static_cast<uint>(propertyCount)) {
            Scope scope(that->engine());
            ScopedString propName(scope, that->engine()->newString(QString::fromUtf8(mo->property(it->arrayIndex).name())));
            name->setM(propName->d());
            ++it->arrayIndex;
            *attributes = QV4::Attr_Data;
            p->value = that->get(propName);
            return;
        }
        const int methodCount = mo->methodCount();
        while (it->arrayIndex < static_cast<uint>(propertyCount + methodCount)) {
            const int index = it->arrayIndex - propertyCount;
            const QMetaMethod method = mo->method(index);
            ++it->arrayIndex;
            if (method.access() == QMetaMethod::Private || (preventDestruction && (index == deleteLaterIdx || index == destroyedIdx1 || index == destroyedIdx2)))
                continue;
            Scope scope(that->engine());
            ScopedString methodName(scope, that->engine()->newString(QString::fromUtf8(method.name())));
            name->setM(methodName->d());
            *attributes = QV4::Attr_Data;
            p->value = that->get(methodName);
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

            QQmlMetaObject::ArgTypeStorage storage;
            int *argsTypes = QQmlMetaObject(r).methodParameterTypes(This->signalIndex, &storage, 0);

            int argCount = argsTypes ? argsTypes[0]:0;

            QV4::Scope scope(v4);
            QV4::ScopedFunctionObject f(scope, This->function.value());

            QV4::ScopedCallData callData(scope, argCount);
            callData->thisObject = This->thisObject.isUndefined() ? v4->globalObject->asReturnedValue() : This->thisObject.value();
            for (int ii = 0; ii < argCount; ++ii) {
                int type = argsTypes[ii + 1];
                if (type == qMetaTypeId<QVariant>()) {
                    callData->args[ii] = v4->fromVariant(*((QVariant *)metaArgs[ii + 1]));
                } else {
                    callData->args[ii] = v4->fromVariant(QVariant(type, metaArgs[ii + 1]));
                }
            }

            f->call(scope, callData);
            if (scope.hasException()) {
                QQmlError error = v4->catchExceptionAsQmlError();
                if (error.description().isEmpty()) {
                    QV4::ScopedString name(scope, f->name());
                    error.setDescription(QStringLiteral("Unknown exception occurred during evaluation of connected function: %1").arg(name->toQString()));
                }
                if (QQmlEngine *qmlEngine = v4->qmlEngine()) {
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
                        (connection->thisObject.isUndefined() || RuntimeHelpers::strictEqual(*connection->thisObject.valueRef(), thisObject))) {

                    QV4::ScopedFunctionObject f(scope, connection->function.value());
                    QPair<QObject *, int> connectedFunctionData = QObjectMethod::extractQtMethod(f);
                    if (connectedFunctionData.first == receiverToDisconnect &&
                        connectedFunctionData.second == slotIndexToDisconnect) {
                        *ret = true;
                        return;
                    }
                }
            } else {
                // This is a normal JS function
                if (RuntimeHelpers::strictEqual(*connection->function.valueRef(), function) &&
                        connection->thisObject.isUndefined() == thisObject->isUndefined() &&
                        (connection->thisObject.isUndefined() || RuntimeHelpers::strictEqual(*connection->thisObject.valueRef(), thisObject))) {
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
    if (ctx->argc() == 0)
        V4THROW_ERROR("Function.prototype.connect: no arguments given");

    QPair<QObject *, int> signalInfo = extractQtSignal(ctx->thisObject());
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

    if (ctx->argc() == 1) {
        f = ctx->args()[0];
    } else if (ctx->argc() >= 2) {
        thisObject = ctx->args()[0];
        f = ctx->args()[1];
    }

    if (!f)
        V4THROW_ERROR("Function.prototype.connect: target is not a function");

    if (!thisObject->isUndefined() && !thisObject->isObject())
        V4THROW_ERROR("Function.prototype.connect: target this is not an object");

    QV4::QObjectSlotDispatcher *slot = new QV4::QObjectSlotDispatcher;
    slot->signalIndex = signalIndex;

    slot->thisObject.set(scope.engine, thisObject);
    slot->function.set(scope.engine, f);

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
    if (ctx->argc() == 0)
        V4THROW_ERROR("Function.prototype.disconnect: no arguments given");

    QV4::Scope scope(ctx);

    QPair<QObject *, int> signalInfo = extractQtSignal(ctx->thisObject());
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

    if (ctx->argc() == 1) {
        functionValue = ctx->args()[0];
    } else if (ctx->argc() >= 2) {
        functionThisValue = ctx->args()[0];
        functionValue = ctx->args()[1];
    }

    if (!functionValue)
        V4THROW_ERROR("Function.prototype.disconnect: target is not a function");

    if (!functionThisValue->isUndefined() && !functionThisValue->isObject())
        V4THROW_ERROR("Function.prototype.disconnect: target this is not an object");

    QPair<QObject *, int> functionData = QObjectMethod::extractQtMethod(functionValue);

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
        QObjectWrapper::markWrapper(child, e);
        markChildQObjectsRecursively(child, e);
    }
}

void QObjectWrapper::markObjects(Heap::Base *that, QV4::ExecutionEngine *e)
{
    QObjectWrapper::Data *This = static_cast<QObjectWrapper::Data *>(that);

    if (QObject *o = This->object()) {
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

void QObjectWrapper::destroyObject(bool lastCall)
{
    Heap::QObjectWrapper *h = d();
    if (!h->internalClass)
        return; // destroyObject already got called

    if (h->object()) {
        QQmlData *ddata = QQmlData::get(h->object(), false);
        if (ddata) {
            if (!h->object()->parent() && !ddata->indestructible) {
                if (ddata && ddata->ownContext && ddata->context)
                    ddata->context->emitDestruction();
                // This object is notionally destroyed now
                ddata->isQueuedForDeletion = true;
                if (lastCall)
                    delete h->object();
                else
                    h->object()->deleteLater();
            } else {
                // If the object is C++-owned, we still have to release the weak reference we have
                // to it.
                ddata->jsWrapper.clear();
            }
        }
    }

    h->internalClass = 0;
    h->~Data();
}


DEFINE_OBJECT_VTABLE(QObjectWrapper);

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
    inline void fromValue(int type, ExecutionEngine *, const Value &);
    inline ReturnedValue toValue(ExecutionEngine *);

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
        QByteArray *qbyteArrayPtr;
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

static QV4::ReturnedValue CallMethod(const QQmlObjectOrGadget &object, int index, int returnType, int argCount,
                                        int *argTypes, QV4::ExecutionEngine *engine, QV4::CallData *callArgs,
                                         QMetaObject::Call callType = QMetaObject::InvokeMetaMethod)
{
    if (argCount > 0) {
        // Convert all arguments.
        QVarLengthArray<CallArgument, 9> args(argCount + 1);
        args[0].initAsType(returnType);
        for (int ii = 0; ii < argCount; ++ii)
            args[ii + 1].fromValue(argTypes[ii], engine, callArgs->args[ii]);
        QVarLengthArray<void *, 9> argData(args.count());
        for (int ii = 0; ii < args.count(); ++ii)
            argData[ii] = args[ii].dataPtr();

        object.metacall(callType, index, argData.data());

        return args[0].toValue(engine);

    } else if (returnType != QMetaType::Void) {

        CallArgument arg;
        arg.initAsType(returnType);

        void *args[] = { arg.dataPtr() };

        object.metacall(callType, index, args);

        return arg.toValue(engine);

    } else {

        void *args[] = { 0 };
        object.metacall(callType, index, args);
        return Encode::undefined();

    }
}

/*!
    Returns the match score for converting \a actual to be of type \a conversionType.  A
    zero score means "perfect match" whereas a higher score is worse.

    The conversion table is copied out of the \l QScript::callQtMethod() function.
*/
static int MatchScore(const QV4::Value &actual, int conversionType)
{
    if (actual.isNumber()) {
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
    } else if (actual.isString()) {
        switch (conversionType) {
        case QMetaType::QString:
            return 0;
        case QMetaType::QJsonValue:
            return 5;
        default:
            return 10;
        }
    } else if (actual.isBoolean()) {
        switch (conversionType) {
        case QMetaType::Bool:
            return 0;
        case QMetaType::QJsonValue:
            return 5;
        default:
            return 10;
        }
    } else if (actual.as<DateObject>()) {
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
    } else if (actual.as<QV4::RegExpObject>()) {
        switch (conversionType) {
        case QMetaType::QRegExp:
            return 0;
        default:
            return 10;
        }
    } else if (actual.as<ArrayBuffer>()) {
        switch (conversionType) {
        case QMetaType::QByteArray:
            return 0;
        default:
            return 10;
        }
    } else if (actual.as<ArrayObject>()) {
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
    } else if (actual.isNull()) {
        switch (conversionType) {
        case QMetaType::Nullptr:
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
    } else if (const Object *obj = actual.as<Object>()) {
        if (obj->as<QV4::VariantObject>()) {
            if (conversionType == qMetaTypeId<QVariant>())
                return 0;
            if (obj->engine()->toVariant(actual, -1).userType() == conversionType)
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

        if (obj->as<QV4::QQmlValueTypeWrapper>()) {
            if (obj->engine()->toVariant(actual, -1).userType() == conversionType)
                return 0;
            return 10;
        } else if (conversionType == QMetaType::QJsonObject) {
            return 5;
        } else if (conversionType == qMetaTypeId<QJSValue>()) {
            return 0;
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
static const QQmlPropertyData * RelatedMethod(const QQmlObjectOrGadget &object,
                                              const QQmlPropertyData *current,
                                              QQmlPropertyData &dummy,
                                              const QQmlPropertyCache *propertyCache)
{
    if (!current->isOverload())
        return 0;

    Q_ASSERT(!current->overrideIndexIsProperty());

    if (propertyCache) {
        return propertyCache->method(current->overrideIndex());
    } else {
        const QMetaObject *mo = object.metaObject();
        int methodOffset = mo->methodCount() - QMetaObject_methods(mo);

        while (methodOffset > current->overrideIndex()) {
            mo = mo->superClass();
            methodOffset -= QMetaObject_methods(mo);
        }

        // If we've been called before with the same override index, then
        // we can't go any further...
        if (&dummy == current && dummy.coreIndex() == current->overrideIndex())
            return 0;

        QMetaMethod method = mo->method(current->overrideIndex());
        dummy.load(method);

        // Look for overloaded methods
        QByteArray methodName = method.name();
        for (int ii = current->overrideIndex() - 1; ii >= methodOffset; --ii) {
            if (methodName == mo->method(ii).name()) {
                dummy.setOverload(true);
                dummy.setOverrideIndexIsProperty(0);
                dummy.setOverrideIndex(ii);
                return &dummy;
            }
        }

        return &dummy;
    }
}

static QV4::ReturnedValue CallPrecise(const QQmlObjectOrGadget &object, const QQmlPropertyData &data,
                                      QV4::ExecutionEngine *engine, QV4::CallData *callArgs,
                                      QMetaObject::Call callType = QMetaObject::InvokeMetaMethod)
{
    QByteArray unknownTypeError;

    int returnType = object.methodReturnType(data, &unknownTypeError);

    if (returnType == QMetaType::UnknownType) {
        QString typeName = QString::fromLatin1(unknownTypeError);
        QString error = QStringLiteral("Unknown method return type: %1").arg(typeName);
        return engine->throwError(error);
    }

    if (data.hasArguments()) {

        int *args = 0;
        QQmlMetaObject::ArgTypeStorage storage;

        if (data.isConstructor())
            args = static_cast<const QQmlStaticMetaObject&>(object).constructorParameterTypes(
                        data.coreIndex(), &storage, &unknownTypeError);
        else
            args = object.methodParameterTypes(data.coreIndex(), &storage, &unknownTypeError);

        if (!args) {
            QString typeName = QString::fromLatin1(unknownTypeError);
            QString error = QStringLiteral("Unknown method parameter type: %1").arg(typeName);
            return engine->throwError(error);
        }

        if (args[0] > callArgs->argc) {
            QString error = QLatin1String("Insufficient arguments");
            return engine->throwError(error);
        }

        return CallMethod(object, data.coreIndex(), returnType, args[0], args + 1, engine, callArgs, callType);

    } else {

        return CallMethod(object, data.coreIndex(), returnType, 0, 0, engine, callArgs, callType);

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
static QV4::ReturnedValue CallOverloaded(const QQmlObjectOrGadget &object, const QQmlPropertyData &data,
                                         QV4::ExecutionEngine *engine, QV4::CallData *callArgs, const QQmlPropertyCache *propertyCache,
                                         QMetaObject::Call callType = QMetaObject::InvokeMetaMethod)
{
    int argumentCount = callArgs->argc;

    QQmlPropertyData best;
    int bestParameterScore = INT_MAX;
    int bestMatchScore = INT_MAX;

    QQmlPropertyData dummy;
    const QQmlPropertyData *attempt = &data;

    QV4::Scope scope(engine);
    QV4::ScopedValue v(scope);

    do {
        QQmlMetaObject::ArgTypeStorage storage;
        int methodArgumentCount = 0;
        int *methodArgTypes = 0;
        if (attempt->hasArguments()) {
            int *args = object.methodParameterTypes(attempt->coreIndex(), &storage, 0);
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

    } while ((attempt = RelatedMethod(object, attempt, dummy, propertyCache)) != 0);

    if (best.isValid()) {
        return CallPrecise(object, best, engine, callArgs, callType);
    } else {
        QString error = QLatin1String("Unable to determine callable overload.  Candidates are:");
        const QQmlPropertyData *candidate = &data;
        while (candidate) {
            error += QLatin1String("\n    ") +
                     QString::fromUtf8(object.metaObject()->method(candidate->coreIndex())
                                       .methodSignature());
            candidate = RelatedMethod(object, candidate, dummy, propertyCache);
        }

        return engine->throwError(error);
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
    } else if (type == QMetaType::QByteArray) {
        qbyteArrayPtr->~QByteArray();
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

void CallArgument::fromValue(int callType, QV4::ExecutionEngine *engine, const QV4::Value &value)
{
    if (type != 0) {
        cleanup();
        type = 0;
    }

    QV4::Scope scope(engine);

    bool queryEngine = false;
    if (callType == qMetaTypeId<QJSValue>()) {
        qjsValuePtr = new (&allocData) QJSValue(scope.engine, value.asReturnedValue());
        type = qMetaTypeId<QJSValue>();
    } else if (callType == QMetaType::Int) {
        intValue = quint32(value.toInt32());
        type = callType;
    } else if (callType == QMetaType::UInt) {
        intValue = quint32(value.toUInt32());
        type = callType;
    } else if (callType == QMetaType::Bool) {
        boolValue = value.toBoolean();
        type = callType;
    } else if (callType == QMetaType::Double) {
        doubleValue = double(value.toNumber());
        type = callType;
    } else if (callType == QMetaType::Float) {
        floatValue = float(value.toNumber());
        type = callType;
    } else if (callType == QMetaType::QString) {
        if (value.isNull() || value.isUndefined())
            qstringPtr = new (&allocData) QString();
        else
            qstringPtr = new (&allocData) QString(value.toQStringNoThrow());
        type = callType;
    } else if (callType == QMetaType::QObjectStar) {
        qobjectPtr = 0;
        if (const QV4::QObjectWrapper *qobjectWrapper = value.as<QV4::QObjectWrapper>())
            qobjectPtr = qobjectWrapper->object();
        else if (const QV4::QmlTypeWrapper *qmlTypeWrapper = value.as<QV4::QmlTypeWrapper>())
            queryEngine = qmlTypeWrapper->isSingleton();
        type = callType;
    } else if (callType == qMetaTypeId<QVariant>()) {
        qvariantPtr = new (&allocData) QVariant(scope.engine->toVariant(value, -1));
        type = callType;
    } else if (callType == qMetaTypeId<QList<QObject*> >()) {
        qlistPtr = new (&allocData) QList<QObject *>();
        QV4::ScopedArrayObject array(scope, value);
        if (array) {
            Scoped<QV4::QObjectWrapper> qobjectWrapper(scope);

            uint length = array->getLength();
            for (uint ii = 0; ii < length; ++ii)  {
                QObject *o = 0;
                qobjectWrapper = array->getIndexed(ii);
                if (!!qobjectWrapper)
                    o = qobjectWrapper->object();
                qlistPtr->append(o);
            }
        } else {
            QObject *o = 0;
            if (const QV4::QObjectWrapper *qobjectWrapper = value.as<QV4::QObjectWrapper>())
                o = qobjectWrapper->object();
            qlistPtr->append(o);
        }
        type = callType;
    } else if (callType == qMetaTypeId<QQmlV4Handle>()) {
        handlePtr = new (&allocData) QQmlV4Handle(value.asReturnedValue());
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

        QQmlEnginePrivate *ep = engine->qmlEngine() ? QQmlEnginePrivate::get(engine->qmlEngine()) : 0;
        QVariant v = scope.engine->toVariant(value, callType);

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

QV4::ReturnedValue CallArgument::toValue(QV4::ExecutionEngine *engine)
{
    QV4::Scope scope(engine);

    if (type == qMetaTypeId<QJSValue>()) {
        return QJSValuePrivate::convertedToValue(scope.engine, *qjsValuePtr);
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
        return QV4::Encode(engine->newString(*qstringPtr));
    } else if (type == QMetaType::QByteArray) {
        return QV4::Encode(engine->newArrayBuffer(*qbyteArrayPtr));
    } else if (type == QMetaType::QObjectStar) {
        QObject *object = qobjectPtr;
        if (object)
            QQmlData::get(object, true)->setImplicitDestructible();
        return QV4::QObjectWrapper::wrap(scope.engine, object);
    } else if (type == qMetaTypeId<QList<QObject *> >()) {
        // XXX Can this be made more by using Array as a prototype and implementing
        // directly against QList<QObject*>?
        QList<QObject *> &list = *qlistPtr;
        QV4::ScopedArrayObject array(scope, scope.engine->newArrayObject());
        array->arrayReserve(list.count());
        QV4::ScopedValue v(scope);
        for (int ii = 0; ii < list.count(); ++ii)
            array->arrayPut(ii, (v = QV4::QObjectWrapper::wrap(scope.engine, list.at(ii))));
        array->setArrayLengthUnchecked(list.count());
        return array.asReturnedValue();
    } else if (type == qMetaTypeId<QQmlV4Handle>()) {
        return *handlePtr;
    } else if (type == QMetaType::QJsonArray) {
        return QV4::JsonObject::fromJsonArray(scope.engine, *jsonArrayPtr);
    } else if (type == QMetaType::QJsonObject) {
        return QV4::JsonObject::fromJsonObject(scope.engine, *jsonObjectPtr);
    } else if (type == QMetaType::QJsonValue) {
        return QV4::JsonObject::fromJsonValue(scope.engine, *jsonValuePtr);
    } else if (type == -1 || type == qMetaTypeId<QVariant>()) {
        QVariant value = *qvariantPtr;
        QV4::ScopedValue rv(scope, scope.engine->fromVariant(value));
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, rv);
        if (!!qobjectWrapper) {
            if (QObject *object = qobjectWrapper->object())
                QQmlData::get(object, true)->setImplicitDestructible();
        }
        return rv->asReturnedValue();
    } else {
        return QV4::Encode::undefined();
    }
}

ReturnedValue QObjectMethod::create(ExecutionContext *scope, QObject *object, int index)
{
    Scope valueScope(scope);
    Scoped<QObjectMethod> method(valueScope, scope->d()->engine->memoryManager->allocObject<QObjectMethod>(scope));
    method->d()->setObject(object);

    if (QQmlData *ddata = QQmlData::get(object))
        method->d()->setPropertyCache(ddata->propertyCache);

    method->d()->index = index;
    return method.asReturnedValue();
}

ReturnedValue QObjectMethod::create(ExecutionContext *scope, const QQmlValueTypeWrapper *valueType, int index)
{
    Scope valueScope(scope);
    Scoped<QObjectMethod> method(valueScope, valueScope.engine->memoryManager->allocObject<QObjectMethod>(scope));
    method->d()->setPropertyCache(valueType->d()->propertyCache());
    method->d()->index = index;
    method->d()->valueTypeWrapper = valueType->d();
    return method.asReturnedValue();
}

void Heap::QObjectMethod::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope);
}

const QMetaObject *Heap::QObjectMethod::metaObject()
{
    if (propertyCache())
        return propertyCache()->createMetaObject();
    return object()->metaObject();
}

QV4::ReturnedValue QObjectMethod::method_toString(QV4::ExecutionContext *ctx) const
{
    QString result;
    if (const QMetaObject *metaObject = d()->metaObject()) {

        result += QString::fromUtf8(metaObject->className()) +
                QLatin1String("(0x") + QString::number((quintptr)d()->object(),16);

        if (d()->object()) {
            QString objectName = d()->object()->objectName();
            if (!objectName.isEmpty())
                result += QLatin1String(", \"") + objectName + QLatin1Char('\"');
        }

        result += QLatin1Char(')');
    } else {
        result = QLatin1String("null");
    }

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

QV4::ReturnedValue QObjectMethod::method_destroy(QV4::ExecutionContext *ctx, const Value *args, int argc) const
{
    if (!d()->object())
        return Encode::undefined();
    if (QQmlData::keepAliveDuringGarbageCollection(d()->object()))
        return ctx->engine()->throwError(QStringLiteral("Invalid attempt to destroy() an indestructible object"));

    int delay = 0;
    if (argc > 0)
        delay = args[0].toUInt32();

    if (delay > 0)
        QTimer::singleShot(delay, d()->object(), SLOT(deleteLater()));
    else
        d()->object()->deleteLater();

    return Encode::undefined();
}

void QObjectMethod::call(const Managed *m, Scope &scope, CallData *callData)
{
    const QObjectMethod *This = static_cast<const QObjectMethod*>(m);
    This->callInternal(callData, scope);
}

void QObjectMethod::callInternal(CallData *callData, Scope &scope) const
{
    ExecutionEngine *v4 = engine();
    ExecutionContext *context = v4->currentContext;
    if (d()->index == DestroyMethod) {
        scope.result = method_destroy(context, callData->args, callData->argc);
        return;
    }

    else if (d()->index == ToStringMethod) {
        scope.result = method_toString(context);
        return;
    }

    QQmlObjectOrGadget object(d()->object());
    if (!d()->object()) {
        if (!d()->valueTypeWrapper) {
            scope.result = Encode::undefined();
            return;
        }

        object = QQmlObjectOrGadget(d()->propertyCache(), d()->valueTypeWrapper->gadgetPtr);
    }

    QQmlPropertyData method;

    if (d()->propertyCache()) {
        QQmlPropertyData *data = d()->propertyCache()->method(d()->index);
        if (!data) {
            scope.result = QV4::Encode::undefined();
            return;
        }
        method = *data;
    } else {
        const QMetaObject *mo = d()->object()->metaObject();
        const QMetaMethod moMethod = mo->method(d()->index);
        method.load(moMethod);

        if (method.coreIndex() == -1) {
            scope.result = QV4::Encode::undefined();
            return;
        }

        // Look for overloaded methods
        QByteArray methodName = moMethod.name();
        const int methodOffset = mo->methodOffset();
        for (int ii = d()->index - 1; ii >= methodOffset; --ii) {
            if (methodName == mo->method(ii).name()) {
                method.setOverload(true);
                method.setOverrideIndexIsProperty(0);
                method.setOverrideIndex(ii);
                break;
            }
        }
    }

    if (method.isV4Function()) {
        scope.result = QV4::Encode::undefined();
        QQmlV4Function func(callData, &scope.result, v4);
        QQmlV4Function *funcptr = &func;

        void *args[] = { 0, &funcptr };
        object.metacall(QMetaObject::InvokeMetaMethod, method.coreIndex(), args);

        return;
    }

    if (!method.isOverload()) {
        scope.result = CallPrecise(object, method, v4, callData);
    } else {
        scope.result = CallOverloaded(object, method, v4, callData, d()->propertyCache());
    }
}

void QObjectMethod::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    QObjectMethod::Data *This = static_cast<QObjectMethod::Data*>(that);
    if (This->valueTypeWrapper)
        This->valueTypeWrapper->mark(e);

    FunctionObject::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(QObjectMethod);


void Heap::QMetaObjectWrapper::init(const QMetaObject *metaObject)
{
    FunctionObject::init();
    this->metaObject = metaObject;
    constructors = nullptr;
    constructorCount = 0;
}

void Heap::QMetaObjectWrapper::destroy()
{
    delete[] constructors;
}

void Heap::QMetaObjectWrapper::ensureConstructorsCache() {

    const int count = metaObject->constructorCount();
    if (constructorCount != count) {
        delete[] constructors;
        constructorCount = count;
        if (count == 0) {
            constructors = nullptr;
            return;
        }
        constructors = new QQmlPropertyData[count];

        for (int i = 0; i < count; ++i) {
            QMetaMethod method = metaObject->constructor(i);
            QQmlPropertyData &d = constructors[i];
            d.load(method);
            d.setCoreIndex(i);
        }
    }
}


ReturnedValue QMetaObjectWrapper::create(ExecutionEngine *engine, const QMetaObject* metaObject) {

     QV4::Scope scope(engine);
     Scoped<QMetaObjectWrapper> mo(scope, engine->memoryManager->allocObject<QV4::QMetaObjectWrapper>(metaObject)->asReturnedValue());
     mo->init(engine);
     return mo->asReturnedValue();
}

void QMetaObjectWrapper::init(ExecutionEngine *) {
    const QMetaObject & mo = *d()->metaObject;

    for (int i = 0; i < mo.enumeratorCount(); i++) {
        QMetaEnum Enum = mo.enumerator(i);
        for (int k = 0; k < Enum.keyCount(); k++) {
            const char* key = Enum.key(k);
            const int value = Enum.value(k);
            defineReadonlyProperty(QLatin1String(key), Primitive::fromInt32(value));
        }
    }
}

void QMetaObjectWrapper::construct(const Managed *m, Scope &scope, CallData *callData)
{
    const QMetaObjectWrapper *This = static_cast<const QMetaObjectWrapper*>(m);
    scope.result = This->constructInternal(callData);
}

ReturnedValue QMetaObjectWrapper::constructInternal(CallData * callData) const {

    d()->ensureConstructorsCache();

    ExecutionEngine *v4 = engine();
    const QMetaObject* mo = d()->metaObject;
    if (d()->constructorCount == 0) {
        return v4->throwTypeError(QStringLiteral("%1 has no invokable constructor")
                                      .arg(QLatin1String(mo->className())));
    }

    Scope scope(v4);
    Scoped<QObjectWrapper> object(scope);

    if (d()->constructorCount == 1) {
        object = callConstructor(d()->constructors[0], v4, callData);
    }
    else {
        object = callOverloadedConstructor(v4, callData);
    }
    Scoped<QMetaObjectWrapper> metaObject(scope, this);
    object->defineDefaultProperty(v4->id_constructor(), metaObject);
    object->setPrototype(const_cast<QMetaObjectWrapper*>(this));
    return object.asReturnedValue();

}

ReturnedValue QMetaObjectWrapper::callConstructor(const QQmlPropertyData &data, QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const {

    const QMetaObject* mo = d()->metaObject;
    const QQmlStaticMetaObject object(mo);
    return CallPrecise(object, data, engine, callArgs, QMetaObject::CreateInstance);
}


ReturnedValue QMetaObjectWrapper::callOverloadedConstructor(QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const {
    const int numberOfConstructors = d()->constructorCount;
    const int argumentCount = callArgs->argc;
    const QQmlStaticMetaObject object(d()->metaObject);

    QQmlPropertyData best;
    int bestParameterScore = INT_MAX;
    int bestMatchScore = INT_MAX;

    QV4::Scope scope(engine);
    QV4::ScopedValue v(scope);

    for (int i = 0; i < numberOfConstructors; i++) {
        const QQmlPropertyData & attempt = d()->constructors[i];
        int methodArgumentCount = 0;
        int *methodArgTypes = 0;
        if (attempt.hasArguments()) {
            QQmlMetaObject::ArgTypeStorage storage;
            int *args = object.constructorParameterTypes(attempt.coreIndex(), &storage, 0);
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
            best = attempt;
            bestParameterScore = methodParameterScore;
            bestMatchScore = methodMatchScore;
        }

        if (bestParameterScore == 0 && bestMatchScore == 0)
            break; // We can't get better than that
    };

    if (best.isValid()) {
        return CallPrecise(object, best, engine, callArgs, QMetaObject::CreateInstance);
    } else {
        QString error = QLatin1String("Unable to determine callable overload.  Candidates are:");
        for (int i = 0; i < numberOfConstructors; i++) {
            const QQmlPropertyData & candidate = d()->constructors[i];
            error += QLatin1String("\n    ") +
                    QString::fromUtf8(d()->metaObject->constructor(candidate.coreIndex())
                                      .methodSignature());
        }

        return engine->throwError(error);
    }
}

bool QMetaObjectWrapper::isEqualTo(Managed *a, Managed *b)
{
    Q_ASSERT(a->as<QMetaObjectWrapper>());
    QMetaObjectWrapper *aMetaObject = a->as<QMetaObjectWrapper>();
    QMetaObjectWrapper *bMetaObject = b->as<QMetaObjectWrapper>();
    if (!bMetaObject)
        return true;
    return aMetaObject->metaObject() == bMetaObject->metaObject();
}

DEFINE_OBJECT_VTABLE(QMetaObjectWrapper);




void Heap::QmlSignalHandler::init(QObject *object, int signalIndex)
{
    Object::init();
    this->signalIndex = signalIndex;
    setObject(object);
}

DEFINE_OBJECT_VTABLE(QmlSignalHandler);

void QmlSignalHandler::initProto(ExecutionEngine *engine)
{
    if (engine->signalHandlerPrototype()->d_unchecked())
        return;

    Scope scope(engine);
    ScopedObject o(scope, engine->newObject());
    QV4::ScopedString connect(scope, engine->newIdentifier(QStringLiteral("connect")));
    QV4::ScopedString disconnect(scope, engine->newIdentifier(QStringLiteral("disconnect")));
    o->put(connect, QV4::ScopedValue(scope, engine->functionPrototype()->get(connect)));
    o->put(disconnect, QV4::ScopedValue(scope, engine->functionPrototype()->get(disconnect)));

    engine->jsObjects[QV4::ExecutionEngine::SignalHandlerProto] = o->d();
}

void MultiplyWrappedQObjectMap::insert(QObject *key, Heap::Object *value)
{
    QV4::WeakValue v;
    v.set(value->internalClass->engine, value);
    QHash<QObject*, QV4::WeakValue>::insert(key, v);
    connect(key, SIGNAL(destroyed(QObject*)), this, SLOT(removeDestroyedObject(QObject*)));
}



MultiplyWrappedQObjectMap::Iterator MultiplyWrappedQObjectMap::erase(MultiplyWrappedQObjectMap::Iterator it)
{
    disconnect(it.key(), SIGNAL(destroyed(QObject*)), this, SLOT(removeDestroyedObject(QObject*)));
    return QHash<QObject*, QV4::WeakValue>::erase(it);
}

void MultiplyWrappedQObjectMap::remove(QObject *key)
{
    Iterator it = find(key);
    if (it == end())
        return;
    erase(it);
}

void MultiplyWrappedQObjectMap::mark(QObject *key, ExecutionEngine *engine)
{
    Iterator it = find(key);
    if (it == end())
        return;
    it->markOnce(engine);
}

void MultiplyWrappedQObjectMap::removeDestroyedObject(QObject *object)
{
    QHash<QObject*, QV4::WeakValue>::remove(object);
}

QT_END_NAMESPACE

