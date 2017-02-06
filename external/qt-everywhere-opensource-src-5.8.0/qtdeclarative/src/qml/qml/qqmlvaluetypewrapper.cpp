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

#include "qqmlvaluetypewrapper_p.h"
#include <private/qv8engine_p.h>

#include <private/qqmlvaluetype_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlbuiltinfunctions_p.h>

#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4alloca_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4qobjectwrapper_p.h>

QT_BEGIN_NAMESPACE


DEFINE_OBJECT_VTABLE(QV4::QQmlValueTypeWrapper);

namespace QV4 {
namespace Heap {

struct QQmlValueTypeReference : QQmlValueTypeWrapper
{
    void init() {
        QQmlValueTypeWrapper::init();
        object.init();
    }
    void destroy() {
        object.destroy();
        QQmlValueTypeWrapper::destroy();
    }
    QQmlQPointer<QObject> object;
    int property;
};

}

struct QQmlValueTypeReference : public QQmlValueTypeWrapper
{
    V4_OBJECT2(QQmlValueTypeReference, QQmlValueTypeWrapper)
    V4_NEEDS_DESTROY

    bool readReferenceValue() const;
};

}

DEFINE_OBJECT_VTABLE(QV4::QQmlValueTypeReference);

using namespace QV4;

void Heap::QQmlValueTypeWrapper::destroy()
{
    if (gadgetPtr) {
        valueType->metaType.destruct(gadgetPtr);
        ::operator delete(gadgetPtr);
    }
    Object::destroy();
}

void Heap::QQmlValueTypeWrapper::setValue(const QVariant &value) const
{
    Q_ASSERT(valueType->typeId == value.userType());
    if (gadgetPtr)
        valueType->metaType.destruct(gadgetPtr);
    if (!gadgetPtr)
        gadgetPtr = ::operator new(valueType->metaType.sizeOf());
    valueType->metaType.construct(gadgetPtr, value.constData());
}

QVariant Heap::QQmlValueTypeWrapper::toVariant() const
{
    Q_ASSERT(gadgetPtr);
    return QVariant(valueType->typeId, gadgetPtr);
}


bool QQmlValueTypeReference::readReferenceValue() const
{
    if (!d()->object)
        return false;
    // A reference resource may be either a "true" reference (eg, to a QVector3D property)
    // or a "variant" reference (eg, to a QVariant property which happens to contain a value-type).
    QMetaProperty writebackProperty = d()->object->metaObject()->property(d()->property);
    if (writebackProperty.userType() == QMetaType::QVariant) {
        // variant-containing-value-type reference
        QVariant variantReferenceValue;

        void *a[] = { &variantReferenceValue, 0 };
        QMetaObject::metacall(d()->object, QMetaObject::ReadProperty, d()->property, a);

        int variantReferenceType = variantReferenceValue.userType();
        if (variantReferenceType != typeId()) {
            // This is a stale VariantReference.  That is, the variant has been
            // overwritten with a different type in the meantime.
            // We need to modify this reference to the updated value type, if
            // possible, or return false if it is not a value type.
            if (QQmlValueTypeFactory::isValueType(variantReferenceType)) {
                QQmlPropertyCache *cache = 0;
                if (const QMetaObject *mo = QQmlValueTypeFactory::metaObjectForMetaType(variantReferenceType))
                    cache = QJSEnginePrivate::get(engine())->cache(mo);
                if (d()->gadgetPtr) {
                    d()->valueType->metaType.destruct(d()->gadgetPtr);
                    ::operator delete(d()->gadgetPtr);
                }
                d()->gadgetPtr =0;
                d()->setPropertyCache(cache);
                d()->valueType = QQmlValueTypeFactory::valueType(variantReferenceType);
                if (!cache)
                    return false;
            } else {
                return false;
            }
        }
        d()->setValue(variantReferenceValue);
    } else {
        if (!d()->gadgetPtr) {
            d()->gadgetPtr = ::operator new(d()->valueType->metaType.sizeOf());
            d()->valueType->metaType.construct(d()->gadgetPtr, 0);
        }
        // value-type reference
        void *args[] = { d()->gadgetPtr, 0 };
        QMetaObject::metacall(d()->object, QMetaObject::ReadProperty, d()->property, args);
    }
    return true;
}

void QQmlValueTypeWrapper::initProto(ExecutionEngine *v4)
{
    if (v4->valueTypeWrapperPrototype()->d_unchecked())
        return;

    Scope scope(v4);
    ScopedObject o(scope, v4->newObject());
    o->defineDefaultProperty(v4->id_toString(), method_toString, 1);
    v4->jsObjects[QV4::ExecutionEngine::ValueTypeProto] = o->d();
}

ReturnedValue QQmlValueTypeWrapper::create(ExecutionEngine *engine, QObject *object, int property, const QMetaObject *metaObject, int typeId)
{
    Scope scope(engine);
    initProto(engine);

    Scoped<QQmlValueTypeReference> r(scope, engine->memoryManager->allocObject<QQmlValueTypeReference>());
    r->d()->object = object;
    r->d()->property = property;
    r->d()->setPropertyCache(QJSEnginePrivate::get(engine)->cache(metaObject));
    r->d()->valueType = QQmlValueTypeFactory::valueType(typeId);
    r->d()->gadgetPtr = 0;
    return r->asReturnedValue();
}

ReturnedValue QQmlValueTypeWrapper::create(ExecutionEngine *engine, const QVariant &value,  const QMetaObject *metaObject, int typeId)
{
    Scope scope(engine);
    initProto(engine);

    Scoped<QQmlValueTypeWrapper> r(scope, engine->memoryManager->allocObject<QQmlValueTypeWrapper>());
    r->d()->setPropertyCache(QJSEnginePrivate::get(engine)->cache(metaObject));
    r->d()->valueType = QQmlValueTypeFactory::valueType(typeId);
    r->d()->gadgetPtr = 0;
    r->d()->setValue(value);
    return r->asReturnedValue();
}

QVariant QQmlValueTypeWrapper::toVariant() const
{
    if (const QQmlValueTypeReference *ref = as<const QQmlValueTypeReference>())
        if (!ref->readReferenceValue())
            return QVariant();
    return d()->toVariant();
}

bool QQmlValueTypeWrapper::toGadget(void *data) const
{
    if (const QQmlValueTypeReference *ref = as<const QQmlValueTypeReference>())
        if (!ref->readReferenceValue())
            return false;
    const int typeId = d()->valueType->typeId;
    QMetaType::destruct(typeId, data);
    QMetaType::construct(typeId, data, d()->gadgetPtr);
    return true;
}

bool QQmlValueTypeWrapper::isEqualTo(Managed *m, Managed *other)
{
    Q_ASSERT(m && m->as<QQmlValueTypeWrapper>() && other);
    QV4::QQmlValueTypeWrapper *lv = static_cast<QQmlValueTypeWrapper *>(m);

    if (QV4::VariantObject *rv = other->as<VariantObject>())
        return lv->isEqual(rv->d()->data());

    if (QV4::QQmlValueTypeWrapper *v = other->as<QQmlValueTypeWrapper>())
        return lv->isEqual(v->toVariant());

    return false;
}

PropertyAttributes QQmlValueTypeWrapper::query(const Managed *m, String *name)
{
    Q_ASSERT(m->as<const QQmlValueTypeWrapper>());
    const QQmlValueTypeWrapper *r = static_cast<const QQmlValueTypeWrapper *>(m);

    QQmlPropertyData *result = r->d()->propertyCache()->property(name, 0, 0);
    return result ? Attr_Data : Attr_Invalid;
}

void QQmlValueTypeWrapper::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes)
{
    name->setM(0);
    *index = UINT_MAX;

    QQmlValueTypeWrapper *that = static_cast<QQmlValueTypeWrapper*>(m);

    if (QQmlValueTypeReference *ref = that->as<QQmlValueTypeReference>()) {
        if (!ref->readReferenceValue())
            return;
    }

    if (that->d()->propertyCache()) {
        const QMetaObject *mo = that->d()->propertyCache()->createMetaObject();
        const int propertyCount = mo->propertyCount();
        if (it->arrayIndex < static_cast<uint>(propertyCount)) {
            Scope scope(that->engine());
            ScopedString propName(scope, that->engine()->newString(QString::fromUtf8(mo->property(it->arrayIndex).name())));
            name->setM(propName->d());
            ++it->arrayIndex;
            *attributes = QV4::Attr_Data;
            p->value = that->QV4::Object::get(propName);
            return;
        }
    }
    QV4::Object::advanceIterator(m, it, name, index, p, attributes);
}

bool QQmlValueTypeWrapper::isEqual(const QVariant& value)
{
    if (QQmlValueTypeReference *ref = as<QQmlValueTypeReference>())
        if (!ref->readReferenceValue())
            return false;
    return (value == d()->toVariant());
}

int QQmlValueTypeWrapper::typeId() const
{
    return d()->valueType->typeId;
}

bool QQmlValueTypeWrapper::write(QObject *target, int propertyIndex) const
{
    bool destructGadgetOnExit = false;
    if (const QQmlValueTypeReference *ref = as<const QQmlValueTypeReference>()) {
        if (!d()->gadgetPtr) {
            d()->gadgetPtr = alloca(d()->valueType->metaType.sizeOf());
            d()->valueType->metaType.construct(d()->gadgetPtr, 0);
            destructGadgetOnExit = true;
        }
        if (!ref->readReferenceValue())
            return false;
    }

    int flags = 0;
    int status = -1;
    void *a[] = { d()->gadgetPtr, 0, &status, &flags };
    QMetaObject::metacall(target, QMetaObject::WriteProperty, propertyIndex, a);

    if (destructGadgetOnExit) {
        d()->valueType->metaType.destruct(d()->gadgetPtr);
        d()->gadgetPtr = 0;
    }
    return true;
}

ReturnedValue QQmlValueTypeWrapper::method_toString(CallContext *ctx)
{
    Object *o = ctx->thisObject().as<Object>();
    if (!o)
        return ctx->engine()->throwTypeError();
    QQmlValueTypeWrapper *w = o->as<QQmlValueTypeWrapper>();
    if (!w)
        return ctx->engine()->throwTypeError();

    if (QQmlValueTypeReference *ref = w->as<QQmlValueTypeReference>())
        if (!ref->readReferenceValue())
            return Encode::undefined();

    QString result;
    // Prepare a buffer to pass to QMetaType::convert()
    QString convertResult;
    convertResult.~QString();
    if (QMetaType::convert(w->d()->gadgetPtr, w->d()->valueType->typeId, &convertResult, QMetaType::QString)) {
        result = convertResult;
    } else {
        result += QString::fromUtf8(QMetaType::typeName(w->d()->valueType->typeId))
                + QLatin1Char('(');
        const QMetaObject *mo = w->d()->propertyCache()->metaObject();
        const int propCount = mo->propertyCount();
        for (int i = 0; i < propCount; ++i) {
            if (mo->property(i).isDesignable()) {
                QVariant value = mo->property(i).readOnGadget(w->d()->gadgetPtr);
                if (i > 0)
                    result += QLatin1String(", ");
                result += value.toString();
            }
        }
        result += QLatin1Char(')');
    }
    return Encode(ctx->engine()->newString(result));
}

ReturnedValue QQmlValueTypeWrapper::get(const Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<QQmlValueTypeWrapper>());
    const QQmlValueTypeWrapper *r = static_cast<const QQmlValueTypeWrapper *>(m);
    QV4::ExecutionEngine *v4 = r->engine();

    // Note: readReferenceValue() can change the reference->type.
    if (const QQmlValueTypeReference *reference = r->as<QQmlValueTypeReference>()) {
        if (!reference->readReferenceValue())
            return Primitive::undefinedValue().asReturnedValue();
    }

    QQmlPropertyData *result = r->d()->propertyCache()->property(name, 0, 0);
    if (!result)
        return Object::get(m, name, hasProperty);

    if (hasProperty)
        *hasProperty = true;

    if (result->isFunction())
        // calling a Q_INVOKABLE function of a value type
        return QV4::QObjectMethod::create(v4->rootContext(), r, result->coreIndex());

#define VALUE_TYPE_LOAD(metatype, cpptype, constructor) \
    if (result->propType() == metatype) { \
        cpptype v; \
        void *args[] = { &v, 0 }; \
        metaObject->d.static_metacall(reinterpret_cast<QObject*>(gadget), QMetaObject::ReadProperty, index, args); \
        return QV4::Encode(constructor(v)); \
    }

    const QMetaObject *metaObject = r->d()->propertyCache()->metaObject();

    int index = result->coreIndex();
    QQmlMetaObject::resolveGadgetMethodOrPropertyIndex(QMetaObject::ReadProperty, &metaObject, &index);

    void *gadget = r->d()->gadgetPtr;

    // These four types are the most common used by the value type wrappers
    VALUE_TYPE_LOAD(QMetaType::QReal, qreal, qreal);
    VALUE_TYPE_LOAD(QMetaType::Int || result->isEnum(), int, int);
    VALUE_TYPE_LOAD(QMetaType::Int, int, int);
    VALUE_TYPE_LOAD(QMetaType::QString, QString, v4->newString);
    VALUE_TYPE_LOAD(QMetaType::Bool, bool, bool);

    QVariant v;
    void *args[] = { Q_NULLPTR, Q_NULLPTR };
    if (result->propType() == QMetaType::QVariant) {
        args[0] = &v;
    } else {
        v = QVariant(result->propType(), static_cast<void *>(Q_NULLPTR));
        args[0] = v.data();
    }
    metaObject->d.static_metacall(reinterpret_cast<QObject*>(gadget), QMetaObject::ReadProperty, index, args);
    return v4->fromVariant(v);
#undef VALUE_TYPE_ACCESSOR
}

void QQmlValueTypeWrapper::put(Managed *m, String *name, const Value &value)
{
    Q_ASSERT(m->as<QQmlValueTypeWrapper>());
    ExecutionEngine *v4 = static_cast<QQmlValueTypeWrapper *>(m)->engine();
    Scope scope(v4);
    if (scope.hasException())
        return;

    Scoped<QQmlValueTypeWrapper> r(scope, static_cast<QQmlValueTypeWrapper *>(m));
    Scoped<QQmlValueTypeReference> reference(scope, m->d());

    int writeBackPropertyType = -1;

    if (reference) {
        QMetaProperty writebackProperty = reference->d()->object->metaObject()->property(reference->d()->property);

        if (!writebackProperty.isWritable() || !reference->readReferenceValue())
            return;

        writeBackPropertyType = writebackProperty.userType();
    }

    const QMetaObject *metaObject = r->d()->propertyCache()->metaObject();
    const QQmlPropertyData *pd = r->d()->propertyCache()->property(name, 0, 0);
    if (!pd)
        return;

    if (reference) {
        QV4::ScopedFunctionObject f(scope, value);
        if (f) {
            if (!f->isBinding()) {
                // assigning a JS function to a non-var-property is not allowed.
                QString error = QStringLiteral("Cannot assign JavaScript function to value-type property");
                ScopedString e(scope, v4->newString(error));
                v4->throwError(e);
                return;
            }

            QQmlContextData *context = v4->callingQmlContext();

            QQmlPropertyData cacheData;
            cacheData.setWritable(true);
            cacheData.setPropType(writeBackPropertyType);
            cacheData.setCoreIndex(reference->d()->property);

            QV4::Scoped<QQmlBindingFunction> bindingFunction(scope, (const Value &)f);
            bindingFunction->initBindingLocation();

            QQmlBinding *newBinding = QQmlBinding::create(&cacheData, value, reference->d()->object, context);
            newBinding->setTarget(reference->d()->object, cacheData, pd);
            QQmlPropertyPrivate::setBinding(newBinding);
            return;
        } else {
            QQmlPropertyPrivate::removeBinding(reference->d()->object, QQmlPropertyIndex(reference->d()->property, pd->coreIndex()));
        }
    }

    QMetaProperty property = metaObject->property(pd->coreIndex());
    Q_ASSERT(property.isValid());

    QVariant v = v4->toVariant(value, property.userType());

    if (property.isEnumType() && (QMetaType::Type)v.type() == QMetaType::Double)
        v = v.toInt();

    void *gadget = r->d()->gadgetPtr;
    property.writeOnGadget(gadget, v);


    if (reference) {
        if (writeBackPropertyType == QMetaType::QVariant) {
            QVariant variantReferenceValue = r->d()->toVariant();

            int flags = 0;
            int status = -1;
            void *a[] = { &variantReferenceValue, 0, &status, &flags };
            QMetaObject::metacall(reference->d()->object, QMetaObject::WriteProperty, reference->d()->property, a);

        } else {
            int flags = 0;
            int status = -1;
            void *a[] = { r->d()->gadgetPtr, 0, &status, &flags };
            QMetaObject::metacall(reference->d()->object, QMetaObject::WriteProperty, reference->d()->property, a);
        }
    }
}

QT_END_NAMESPACE
