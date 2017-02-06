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

#include <QtQml/qqml.h>

#include "qv4sequenceobject_p.h"

#include <private/qv4functionobject_p.h>
#include <private/qv4arrayobject_p.h>
#include <private/qqmlengine_p.h>
#include <private/qv4scopedvalue_p.h>
#include "qv4runtime_p.h"
#include "qv4objectiterator_p.h"
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qqmlmodelindexvaluetype_p.h>
#include <QtCore/qabstractitemmodel.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

using namespace QV4;

// helper function to generate valid warnings if errors occur during sequence operations.
static void generateWarning(QV4::ExecutionEngine *v4, const QString& description)
{
    QQmlEngine *engine = v4->qmlEngine();
    if (!engine)
        return;
    QQmlError retn;
    retn.setDescription(description);

    QV4::StackFrame frame = v4->currentStackFrame();

    retn.setLine(frame.line);
    retn.setUrl(QUrl(frame.source));
    QQmlEnginePrivate::warning(engine, retn);
}

//  F(elementType, elementTypeName, sequenceType, defaultValue)
#define FOREACH_QML_SEQUENCE_TYPE(F) \
    F(int, IntVector, QVector<int>, 0) \
    F(qreal, RealVector, QVector<qreal>, 0.0) \
    F(bool, BoolVector, QVector<bool>, false) \
    F(int, Int, QList<int>, 0) \
    F(qreal, Real, QList<qreal>, 0.0) \
    F(bool, Bool, QList<bool>, false) \
    F(QString, String, QList<QString>, QString()) \
    F(QString, QString, QStringList, QString()) \
    F(QUrl, Url, QList<QUrl>, QUrl()) \
    F(QModelIndex, QModelIndex, QModelIndexList, QModelIndex()) \
    F(QItemSelectionRange, QItemSelectionRange, QItemSelection, QItemSelectionRange())

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *engine, const QString &element)
{
    return engine->newString(element)->asReturnedValue();
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *, int element)
{
    return QV4::Encode(element);
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *engine, const QUrl &element)
{
    return engine->newString(element.toString())->asReturnedValue();
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *engine, const QModelIndex &element)
{
    const QMetaObject *vtmo = QQmlValueTypeFactory::metaObjectForMetaType(QMetaType::QModelIndex);
    return QV4::QQmlValueTypeWrapper::create(engine, QVariant(element), vtmo, QMetaType::QModelIndex);
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *engine, const QItemSelectionRange &element)
{
    int metaTypeId = qMetaTypeId<QItemSelectionRange>();
    const QMetaObject *vtmo = QQmlValueTypeFactory::metaObjectForMetaType(metaTypeId);
    return QV4::QQmlValueTypeWrapper::create(engine, QVariant::fromValue(element), vtmo, metaTypeId);
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *, qreal element)
{
    return QV4::Encode(element);
}

static QV4::ReturnedValue convertElementToValue(QV4::ExecutionEngine *, bool element)
{
    return QV4::Encode(element);
}

static QString convertElementToString(const QString &element)
{
    return element;
}

static QString convertElementToString(int element)
{
    return QString::number(element);
}

static QString convertElementToString(const QUrl &element)
{
    return element.toString();
}

static QString convertElementToString(const QModelIndex &element)
{
    return reinterpret_cast<const QQmlModelIndexValueType *>(&element)->toString();
}

static QString convertElementToString(const QItemSelectionRange &element)
{
    return reinterpret_cast<const QQmlItemSelectionRangeValueType *>(&element)->toString();
}

static QString convertElementToString(qreal element)
{
    QString qstr;
    RuntimeHelpers::numberToString(&qstr, element, 10);
    return qstr;
}

static QString convertElementToString(bool element)
{
    if (element)
        return QStringLiteral("true");
    else
        return QStringLiteral("false");
}

template <typename ElementType> ElementType convertValueToElement(const Value &value);

template <> QString convertValueToElement(const Value &value)
{
    return value.toQString();
}

template <> int convertValueToElement(const Value &value)
{
    return value.toInt32();
}

template <> QUrl convertValueToElement(const Value &value)
{
    return QUrl(value.toQString());
}

template <> QModelIndex convertValueToElement(const Value &value)
{
    const QQmlValueTypeWrapper *v = value.as<QQmlValueTypeWrapper>();
    if (v)
        return v->toVariant().toModelIndex();
    return QModelIndex();
}

template <> QItemSelectionRange convertValueToElement(const Value &value)
{
    const QQmlValueTypeWrapper *v = value.as<QQmlValueTypeWrapper>();
    if (v)
        return v->toVariant().value<QItemSelectionRange>();
    return QItemSelectionRange();
}

template <> qreal convertValueToElement(const Value &value)
{
    return value.toNumber();
}

template <> bool convertValueToElement(const Value &value)
{
    return value.toBoolean();
}

namespace QV4 {

template <typename Container> struct QQmlSequence;

namespace Heap {

template <typename Container>
struct QQmlSequence : Object {
    void init(const Container &container);
    void init(QObject *object, int propertyIndex);
    void destroy() {
        delete container;
        object.destroy();
        Object::destroy();
    }

    mutable Container *container;
    QQmlQPointer<QObject> object;
    int propertyIndex;
    bool isReference;
};

}

template <typename Container>
struct QQmlSequence : public QV4::Object
{
    V4_OBJECT2(QQmlSequence<Container>, QV4::Object)
    Q_MANAGED_TYPE(QmlSequence)
    V4_PROTOTYPE(sequencePrototype)
    V4_NEEDS_DESTROY
public:

    void init()
    {
        defineAccessorProperty(QStringLiteral("length"), method_get_length, method_set_length);
    }

    QV4::ReturnedValue containerGetIndexed(uint index, bool *hasProperty) const
    {
        /* Qt containers have int (rather than uint) allowable indexes. */
        if (index > INT_MAX) {
            generateWarning(engine(), QLatin1String("Index out of range during indexed get"));
            if (hasProperty)
                *hasProperty = false;
            return Encode::undefined();
        }
        if (d()->isReference) {
            if (!d()->object) {
                if (hasProperty)
                    *hasProperty = false;
                return Encode::undefined();
            }
            loadReference();
        }
        qint32 signedIdx = static_cast<qint32>(index);
        if (signedIdx < d()->container->count()) {
            if (hasProperty)
                *hasProperty = true;
            return convertElementToValue(engine(), d()->container->at(signedIdx));
        }
        if (hasProperty)
            *hasProperty = false;
        return Encode::undefined();
    }

    void containerPutIndexed(uint index, const QV4::Value &value)
    {
        if (internalClass()->engine->hasException)
            return;

        /* Qt containers have int (rather than uint) allowable indexes. */
        if (index > INT_MAX) {
            generateWarning(engine(), QLatin1String("Index out of range during indexed set"));
            return;
        }

        if (d()->isReference) {
            if (!d()->object)
                return;
            loadReference();
        }

        qint32 signedIdx = static_cast<qint32>(index);

        int count = d()->container->count();

        typename Container::value_type element = convertValueToElement<typename Container::value_type>(value);

        if (signedIdx == count) {
            d()->container->append(element);
        } else if (signedIdx < count) {
            (*d()->container)[signedIdx] = element;
        } else {
            /* according to ECMA262r3 we need to insert */
            /* the value at the given index, increasing length to index+1. */
            d()->container->reserve(signedIdx + 1);
            while (signedIdx > count++) {
                d()->container->append(typename Container::value_type());
            }
            d()->container->append(element);
        }

        if (d()->isReference)
            storeReference();
    }

    QV4::PropertyAttributes containerQueryIndexed(uint index) const
    {
        /* Qt containers have int (rather than uint) allowable indexes. */
        if (index > INT_MAX) {
            generateWarning(engine(), QLatin1String("Index out of range during indexed query"));
            return QV4::Attr_Invalid;
        }
        if (d()->isReference) {
            if (!d()->object)
                return QV4::Attr_Invalid;
            loadReference();
        }
        qint32 signedIdx = static_cast<qint32>(index);
        return (signedIdx < d()->container->count()) ? QV4::Attr_Data : QV4::Attr_Invalid;
    }

    void containerAdvanceIterator(ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attrs)
    {
        name->setM(0);
        *index = UINT_MAX;

        if (d()->isReference) {
            if (!d()->object) {
                QV4::Object::advanceIterator(this, it, name, index, p, attrs);
                return;
            }
            loadReference();
        }

        if (it->arrayIndex < static_cast<uint>(d()->container->count())) {
            *index = it->arrayIndex;
            ++it->arrayIndex;
            *attrs = QV4::Attr_Data;
            p->value = convertElementToValue(engine(), d()->container->at(*index));
            return;
        }
        QV4::Object::advanceIterator(this, it, name, index, p, attrs);
    }

    bool containerDeleteIndexedProperty(uint index)
    {
        /* Qt containers have int (rather than uint) allowable indexes. */
        if (index > INT_MAX)
            return false;
        if (d()->isReference) {
            if (!d()->object)
                return false;
            loadReference();
        }
        qint32 signedIdx = static_cast<qint32>(index);

        if (signedIdx >= d()->container->count())
            return false;

        /* according to ECMA262r3 it should be Undefined, */
        /* but we cannot, so we insert a default-value instead. */
        d()->container->replace(signedIdx, typename Container::value_type());

        if (d()->isReference)
            storeReference();

        return true;
    }

    bool containerIsEqualTo(Managed *other)
    {
        if (!other)
            return false;
        QQmlSequence<Container> *otherSequence = other->as<QQmlSequence<Container> >();
        if (!otherSequence)
            return false;
        if (d()->isReference && otherSequence->d()->isReference) {
            return d()->object == otherSequence->d()->object && d()->propertyIndex == otherSequence->d()->propertyIndex;
        } else if (!d()->isReference && !otherSequence->d()->isReference) {
            return this == otherSequence;
        }
        return false;
    }

    struct DefaultCompareFunctor
    {
        bool operator()(typename Container::value_type lhs, typename Container::value_type rhs)
        {
            return convertElementToString(lhs) < convertElementToString(rhs);
        }
    };

    struct CompareFunctor
    {
        CompareFunctor(QV4::ExecutionContext *ctx, const QV4::Value &compareFn)
            : m_ctx(ctx), m_compareFn(&compareFn)
        {}

        bool operator()(typename Container::value_type lhs, typename Container::value_type rhs)
        {
            QV4::Scope scope(m_ctx);
            ScopedObject compare(scope, m_compareFn);
            ScopedCallData callData(scope, 2);
            callData->args[0] = convertElementToValue(this->m_ctx->d()->engine, lhs);
            callData->args[1] = convertElementToValue(this->m_ctx->d()->engine, rhs);
            callData->thisObject = this->m_ctx->d()->engine->globalObject;
            compare->call(scope, callData);
            return scope.result.toNumber() < 0;
        }

    private:
        QV4::ExecutionContext *m_ctx;
        const QV4::Value *m_compareFn;
    };

    void sort(QV4::CallContext *ctx)
    {
        if (d()->isReference) {
            if (!d()->object)
                return;
            loadReference();
        }

        QV4::Scope scope(ctx);
        if (ctx->argc() == 1 && ctx->args()[0].as<FunctionObject>()) {
            CompareFunctor cf(ctx, ctx->args()[0]);
            std::sort(d()->container->begin(), d()->container->end(), cf);
        } else {
            DefaultCompareFunctor cf;
            std::sort(d()->container->begin(), d()->container->end(), cf);
        }

        if (d()->isReference)
            storeReference();
    }

    static QV4::ReturnedValue method_get_length(QV4::CallContext *ctx)
    {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlSequence<Container> > This(scope, ctx->thisObject().as<QQmlSequence<Container> >());
        if (!This)
            return ctx->engine()->throwTypeError();

        if (This->d()->isReference) {
            if (!This->d()->object)
                return QV4::Encode(0);
            This->loadReference();
        }
        return QV4::Encode(This->d()->container->count());
    }

    static QV4::ReturnedValue method_set_length(QV4::CallContext* ctx)
    {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlSequence<Container> > This(scope, ctx->thisObject().as<QQmlSequence<Container> >());
        if (!This)
            return ctx->engine()->throwTypeError();

        quint32 newLength = ctx->args()[0].toUInt32();
        /* Qt containers have int (rather than uint) allowable indexes. */
        if (newLength > INT_MAX) {
            generateWarning(scope.engine, QLatin1String("Index out of range during length set"));
            return QV4::Encode::undefined();
        }
        /* Read the sequence from the QObject property if we're a reference */
        if (This->d()->isReference) {
            if (!This->d()->object)
                return QV4::Encode::undefined();
            This->loadReference();
        }
        /* Determine whether we need to modify the sequence */
        qint32 newCount = static_cast<qint32>(newLength);
        qint32 count = This->d()->container->count();
        if (newCount == count) {
            return QV4::Encode::undefined();
        } else if (newCount > count) {
            /* according to ECMA262r3 we need to insert */
            /* undefined values increasing length to newLength. */
            /* We cannot, so we insert default-values instead. */
            This->d()->container->reserve(newCount);
            while (newCount > count++) {
                This->d()->container->append(typename Container::value_type());
            }
        } else {
            /* according to ECMA262r3 we need to remove */
            /* elements until the sequence is the required length. */
            while (newCount < count) {
                count--;
                This->d()->container->removeAt(count);
            }
        }
        /* write back if required. */
        if (This->d()->isReference) {
            /* write back.  already checked that object is non-null, so skip that check here. */
            This->storeReference();
        }
        return QV4::Encode::undefined();
    }

    QVariant toVariant() const
    { return QVariant::fromValue<Container>(*d()->container); }

    static QVariant toVariant(QV4::ArrayObject *array)
    {
        QV4::Scope scope(array->engine());
        Container result;
        quint32 length = array->getLength();
        QV4::ScopedValue v(scope);
        for (quint32 i = 0; i < length; ++i)
            result << convertValueToElement<typename Container::value_type>((v = array->getIndexed(i)));
        return QVariant::fromValue(result);
    }

    void loadReference() const
    {
        Q_ASSERT(d()->object);
        Q_ASSERT(d()->isReference);
        void *a[] = { d()->container, 0 };
        QMetaObject::metacall(d()->object, QMetaObject::ReadProperty, d()->propertyIndex, a);
    }

    void storeReference()
    {
        Q_ASSERT(d()->object);
        Q_ASSERT(d()->isReference);
        int status = -1;
        QQmlPropertyData::WriteFlags flags = QQmlPropertyData::DontRemoveBinding;
        void *a[] = { d()->container, 0, &status, &flags };
        QMetaObject::metacall(d()->object, QMetaObject::WriteProperty, d()->propertyIndex, a);
    }

    static QV4::ReturnedValue getIndexed(const QV4::Managed *that, uint index, bool *hasProperty)
    { return static_cast<const QQmlSequence<Container> *>(that)->containerGetIndexed(index, hasProperty); }
    static void putIndexed(Managed *that, uint index, const QV4::Value &value)
    { static_cast<QQmlSequence<Container> *>(that)->containerPutIndexed(index, value); }
    static QV4::PropertyAttributes queryIndexed(const QV4::Managed *that, uint index)
    { return static_cast<const QQmlSequence<Container> *>(that)->containerQueryIndexed(index); }
    static bool deleteIndexedProperty(QV4::Managed *that, uint index)
    { return static_cast<QQmlSequence<Container> *>(that)->containerDeleteIndexedProperty(index); }
    static bool isEqualTo(Managed *that, Managed *other)
    { return static_cast<QQmlSequence<Container> *>(that)->containerIsEqualTo(other); }
    static void advanceIterator(Managed *that, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attrs)
    { return static_cast<QQmlSequence<Container> *>(that)->containerAdvanceIterator(it, name, index, p, attrs); }

};


template <typename Container>
void Heap::QQmlSequence<Container>::init(const Container &container)
{
    Object::init();
    this->container = new Container(container);
    propertyIndex = -1;
    isReference = false;
    object.init();

    QV4::Scope scope(internalClass->engine);
    QV4::Scoped<QV4::QQmlSequence<Container> > o(scope, this);
    o->setArrayType(Heap::ArrayData::Custom);
    o->init();
}

template <typename Container>
void Heap::QQmlSequence<Container>::init(QObject *object, int propertyIndex)
{
    Object::init();
    this->container = new Container;
    this->propertyIndex = propertyIndex;
    isReference = true;
    this->object.init(object);
    QV4::Scope scope(internalClass->engine);
    QV4::Scoped<QV4::QQmlSequence<Container> > o(scope, this);
    o->setArrayType(Heap::ArrayData::Custom);
    o->loadReference();
    o->init();
}

}

namespace QV4 {

typedef QQmlSequence<QVector<int> > QQmlIntVectorList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlIntVectorList);
typedef QQmlSequence<QVector<qreal> > QQmlRealVectorList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlRealVectorList);
typedef QQmlSequence<QVector<bool> > QQmlBoolVectorList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlBoolVectorList);
typedef QQmlSequence<QStringList> QQmlQStringList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlQStringList);
typedef QQmlSequence<QList<QString> > QQmlStringList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlStringList);
typedef QQmlSequence<QList<int> > QQmlIntList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlIntList);
typedef QQmlSequence<QList<QUrl> > QQmlUrlList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlUrlList);
typedef QQmlSequence<QModelIndexList> QQmlQModelIndexList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlQModelIndexList);
typedef QQmlSequence<QItemSelection> QQmlQItemSelectionRangeList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlQItemSelectionRangeList);
typedef QQmlSequence<QList<bool> > QQmlBoolList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlBoolList);
typedef QQmlSequence<QList<qreal> > QQmlRealList;
DEFINE_OBJECT_TEMPLATE_VTABLE(QQmlRealList);

}

#define REGISTER_QML_SEQUENCE_METATYPE(unused, unused2, SequenceType, unused3) qRegisterMetaType<SequenceType>(#SequenceType);
void SequencePrototype::init()
{
    FOREACH_QML_SEQUENCE_TYPE(REGISTER_QML_SEQUENCE_METATYPE)
    defineDefaultProperty(QStringLiteral("sort"), method_sort, 1);
    defineDefaultProperty(engine()->id_valueOf(), method_valueOf, 0);
}
#undef REGISTER_QML_SEQUENCE_METATYPE

QV4::ReturnedValue SequencePrototype::method_sort(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::ScopedObject o(scope, ctx->thisObject());
    if (!o || !o->isListType())
        return ctx->engine()->throwTypeError();

    if (ctx->argc() >= 2)
        return o.asReturnedValue();

#define CALL_SORT(SequenceElementType, SequenceElementTypeName, SequenceType, DefaultValue) \
        if (QQml##SequenceElementTypeName##List *s = o->as<QQml##SequenceElementTypeName##List>()) { \
            s->sort(ctx); \
        } else

        FOREACH_QML_SEQUENCE_TYPE(CALL_SORT)

#undef CALL_SORT
        {}
    return o.asReturnedValue();
}

#define IS_SEQUENCE(unused1, unused2, SequenceType, unused3) \
    if (sequenceTypeId == qMetaTypeId<SequenceType>()) { \
        return true; \
    } else

bool SequencePrototype::isSequenceType(int sequenceTypeId)
{
    FOREACH_QML_SEQUENCE_TYPE(IS_SEQUENCE) { /* else */ return false; }
}
#undef IS_SEQUENCE

#define NEW_REFERENCE_SEQUENCE(ElementType, ElementTypeName, SequenceType, unused) \
    if (sequenceType == qMetaTypeId<SequenceType>()) { \
        QV4::ScopedObject obj(scope, engine->memoryManager->allocObject<QQml##ElementTypeName##List>(object, propertyIndex)); \
        return obj.asReturnedValue(); \
    } else

ReturnedValue SequencePrototype::newSequence(QV4::ExecutionEngine *engine, int sequenceType, QObject *object, int propertyIndex, bool *succeeded)
{
    QV4::Scope scope(engine);
    // This function is called when the property is a QObject Q_PROPERTY of
    // the given sequence type.  Internally we store a typed-sequence
    // (as well as object ptr + property index for updated-read and write-back)
    // and so access/mutate avoids variant conversion.
    *succeeded = true;
    FOREACH_QML_SEQUENCE_TYPE(NEW_REFERENCE_SEQUENCE) { /* else */ *succeeded = false; return QV4::Encode::undefined(); }
}
#undef NEW_REFERENCE_SEQUENCE

#define NEW_COPY_SEQUENCE(ElementType, ElementTypeName, SequenceType, unused) \
    if (sequenceType == qMetaTypeId<SequenceType>()) { \
        QV4::ScopedObject obj(scope, engine->memoryManager->allocObject<QQml##ElementTypeName##List>(v.value<SequenceType >())); \
        return obj.asReturnedValue(); \
    } else

ReturnedValue SequencePrototype::fromVariant(QV4::ExecutionEngine *engine, const QVariant& v, bool *succeeded)
{
    QV4::Scope scope(engine);
    // This function is called when assigning a sequence value to a normal JS var
    // in a JS block.  Internally, we store a sequence of the specified type.
    // Access and mutation is extremely fast since it will not need to modify any
    // QObject property.
    int sequenceType = v.userType();
    *succeeded = true;
    FOREACH_QML_SEQUENCE_TYPE(NEW_COPY_SEQUENCE) { /* else */ *succeeded = false; return QV4::Encode::undefined(); }
}
#undef NEW_COPY_SEQUENCE

#define SEQUENCE_TO_VARIANT(ElementType, ElementTypeName, SequenceType, unused) \
    if (QQml##ElementTypeName##List *list = object->as<QQml##ElementTypeName##List>()) \
        return list->toVariant(); \
    else

QVariant SequencePrototype::toVariant(Object *object)
{
    Q_ASSERT(object->isListType());
    FOREACH_QML_SEQUENCE_TYPE(SEQUENCE_TO_VARIANT) { /* else */ return QVariant(); }
}

#undef SEQUENCE_TO_VARIANT
#define SEQUENCE_TO_VARIANT(ElementType, ElementTypeName, SequenceType, unused) \
    if (typeHint == qMetaTypeId<SequenceType>()) { \
        return QQml##ElementTypeName##List::toVariant(a); \
    } else

QVariant SequencePrototype::toVariant(const QV4::Value &array, int typeHint, bool *succeeded)
{
    *succeeded = true;

    if (!array.as<ArrayObject>()) {
        *succeeded = false;
        return QVariant();
    }
    QV4::Scope scope(array.as<Object>()->engine());
    QV4::ScopedArrayObject a(scope, array);

    FOREACH_QML_SEQUENCE_TYPE(SEQUENCE_TO_VARIANT) { /* else */ *succeeded = false; return QVariant(); }
}

#undef SEQUENCE_TO_VARIANT

#define MAP_META_TYPE(ElementType, ElementTypeName, SequenceType, unused) \
    if (object->as<QQml##ElementTypeName##List>()) { \
        return qMetaTypeId<SequenceType>(); \
    } else

int SequencePrototype::metaTypeForSequence(const QV4::Object *object)
{
    FOREACH_QML_SEQUENCE_TYPE(MAP_META_TYPE)
    /*else*/ {
        return -1;
    }
}

#undef MAP_META_TYPE

QT_END_NAMESPACE
