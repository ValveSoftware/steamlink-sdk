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

#include "qqmlvmemetaobject_p.h"


#include "qqml.h"
#include <private/qqmlrefcount_p.h>
#include "qqmlexpression.h"
#include "qqmlexpression_p.h"
#include "qqmlcontext_p.h"
#include "qqmlbinding_p.h"
#include "qqmlpropertyvalueinterceptor_p.h"

#include <private/qqmlglobal_p.h>

#include <private/qv4object_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4scopedvalue_p.h>

QT_BEGIN_NAMESPACE

QQmlVMEVariantQObjectPtr::QQmlVMEVariantQObjectPtr(bool isVar)
    : QQmlGuard<QObject>(0), m_target(0), m_isVar(isVar), m_index(-1)
{
}

QQmlVMEVariantQObjectPtr::~QQmlVMEVariantQObjectPtr()
{
}

void QQmlVMEVariantQObjectPtr::objectDestroyed(QObject *)
{
    if (m_target && m_index >= 0) {
        if (m_isVar && m_target->varPropertiesInitialized && !m_target->varProperties.isUndefined()) {
            // Set the var property to NULL
            QV4::ExecutionEngine *v4 = m_target->varProperties.engine();
            if (v4) {
                QV4::Scope scope(v4);
                QV4::ScopedArrayObject a(scope, m_target->varProperties.value());
                if (a)
                    a->putIndexed(m_index - m_target->firstVarPropertyIndex, QV4::ScopedValue(scope, QV4::Primitive::nullValue()));
            }
        }

        if (!QQmlData::wasDeleted(m_target->object))
            m_target->activate(m_target->object, m_target->methodOffset() + m_index, 0);
    }
}

void QQmlVMEVariantQObjectPtr::setGuardedValue(QObject *obj, QQmlVMEMetaObject *target, int index)
{
    m_target = target;
    m_index = index;
    setObject(obj);
}

class QQmlVMEVariant
{
public:
    inline QQmlVMEVariant();
    inline ~QQmlVMEVariant();

    inline const void *dataPtr() const;
    inline void *dataPtr();
    inline int dataType() const;
    inline size_t dataSize() const;

    inline QObject *asQObject();
    inline const QVariant &asQVariant();
    inline int asInt();
    inline bool asBool();
    inline double asDouble();
    inline const QString &asQString();
    inline const QUrl &asQUrl();
    inline const QTime &asQTime();
    inline const QDate &asQDate();
    inline const QDateTime &asQDateTime();
    inline const QRectF &asQRectF();
    inline const QPointF &asQPointF();
    inline const QSizeF &asQSizeF();
    inline const QJSValue &asQJSValue();

    inline void setValue(QObject *v, QQmlVMEMetaObject *target, int index);
    inline void setValue(const QVariant &);
    inline void setValue(int);
    inline void setValue(bool);
    inline void setValue(double);
    inline void setValue(const QString &);
    inline void setValue(const QUrl &);
    inline void setValue(const QTime &);
    inline void setValue(const QDate &);
    inline void setValue(const QDateTime &);
    inline void setValue(const QRectF &);
    inline void setValue(const QPointF &);
    inline void setValue(const QSizeF &);
    inline void setValue(const QJSValue &);

    inline void setDataType(int t);

    inline void ensureValueType(int);

private:
    int type;
    void *data[8]; // Large enough to hold all types

    inline void cleanup();
};

class QQmlVMEMetaObjectEndpoint : public QQmlNotifierEndpoint
{
public:
    QQmlVMEMetaObjectEndpoint();
    static void vmecallback(QQmlNotifierEndpoint *, void **);
    void tryConnect();

    QFlagPointer<QQmlVMEMetaObject> metaObject;
};


QQmlVMEVariant::QQmlVMEVariant()
: type(QVariant::Invalid)
{
}

QQmlVMEVariant::~QQmlVMEVariant()
{
    cleanup();
}

void QQmlVMEVariant::cleanup()
{
    if (type == QVariant::Invalid) {
    } else if (type == QMetaType::Int ||
               type == QMetaType::Bool ||
               type == QMetaType::Double) {
        type = QVariant::Invalid;
    } else if (type == QMetaType::QObjectStar) {
        ((QQmlVMEVariantQObjectPtr*)dataPtr())->~QQmlVMEVariantQObjectPtr();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QString) {
        ((QString *)dataPtr())->~QString();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QUrl) {
        ((QUrl *)dataPtr())->~QUrl();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QTime) {
        ((QTime *)dataPtr())->~QTime();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QDate) {
        ((QDate *)dataPtr())->~QDate();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QDateTime) {
        ((QDateTime *)dataPtr())->~QDateTime();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QRectF) {
        ((QRectF *)dataPtr())->~QRectF();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QPointF) {
        ((QPointF *)dataPtr())->~QPointF();
        type = QVariant::Invalid;
    } else if (type == QMetaType::QSizeF) {
        ((QSizeF *)dataPtr())->~QSizeF();
        type = QVariant::Invalid;
    } else if (type == qMetaTypeId<QVariant>()) {
        ((QVariant *)dataPtr())->~QVariant();
        type = QVariant::Invalid;
    } else if (type == qMetaTypeId<QJSValue>()) {
        ((QJSValue *)dataPtr())->~QJSValue();
        type = QVariant::Invalid;
    } else {
        if (QQml_valueTypeProvider()->destroyValueType(type, dataPtr(), dataSize())) {
            type = QVariant::Invalid;
        }
    }
}

int QQmlVMEVariant::dataType() const
{
    return type;
}

const void *QQmlVMEVariant::dataPtr() const
{
    return &data;
}

void *QQmlVMEVariant::dataPtr()
{
    return &data;
}

size_t QQmlVMEVariant::dataSize() const
{
    return sizeof(data);
}

QObject *QQmlVMEVariant::asQObject()
{
    if (type != QMetaType::QObjectStar)
        setValue((QObject *)0, 0, -1);

    return *(QQmlGuard<QObject> *)(dataPtr());
}

const QVariant &QQmlVMEVariant::asQVariant()
{
    if (type != QMetaType::QVariant)
        setValue(QVariant());

    return *(QVariant *)(dataPtr());
}

int QQmlVMEVariant::asInt()
{
    if (type != QMetaType::Int)
        setValue(int(0));

    return *(int *)(dataPtr());
}

bool QQmlVMEVariant::asBool()
{
    if (type != QMetaType::Bool)
        setValue(bool(false));

    return *(bool *)(dataPtr());
}

double QQmlVMEVariant::asDouble()
{
    if (type != QMetaType::Double)
        setValue(double(0));

    return *(double *)(dataPtr());
}

const QString &QQmlVMEVariant::asQString()
{
    if (type != QMetaType::QString)
        setValue(QString());

    return *(QString *)(dataPtr());
}

const QUrl &QQmlVMEVariant::asQUrl()
{
    if (type != QMetaType::QUrl)
        setValue(QUrl());

    return *(QUrl *)(dataPtr());
}

const QTime &QQmlVMEVariant::asQTime()
{
    if (type != QMetaType::QTime)
        setValue(QTime());

    return *(QTime *)(dataPtr());
}

const QDate &QQmlVMEVariant::asQDate()
{
    if (type != QMetaType::QDate)
        setValue(QDate());

    return *(QDate *)(dataPtr());
}

const QDateTime &QQmlVMEVariant::asQDateTime()
{
    if (type != QMetaType::QDateTime)
        setValue(QDateTime());

    return *(QDateTime *)(dataPtr());
}

const QRectF &QQmlVMEVariant::asQRectF()
{
    if (type != QMetaType::QRectF)
        setValue(QRectF());

    return *(QRectF *)(dataPtr());
}

const QSizeF &QQmlVMEVariant::asQSizeF()
{
    if (type != QMetaType::QSizeF)
        setValue(QSizeF());

    return *(QSizeF *)(dataPtr());
}

const QPointF &QQmlVMEVariant::asQPointF()
{
    if (type != QMetaType::QPointF)
        setValue(QPointF());

    return *(QPointF *)(dataPtr());
}

const QJSValue &QQmlVMEVariant::asQJSValue()
{
    if (type != qMetaTypeId<QJSValue>())
        setValue(QJSValue());

    return *(QJSValue *)(dataPtr());
}

void QQmlVMEVariant::setValue(QObject *v, QQmlVMEMetaObject *target, int index)
{
    if (type != QMetaType::QObjectStar) {
        cleanup();
        type = QMetaType::QObjectStar;
        new (dataPtr()) QQmlVMEVariantQObjectPtr(false);
    }
    reinterpret_cast<QQmlVMEVariantQObjectPtr*>(dataPtr())->setGuardedValue(v, target, index);
}

void QQmlVMEVariant::setValue(const QVariant &v)
{
    if (type != qMetaTypeId<QVariant>()) {
        cleanup();
        type = qMetaTypeId<QVariant>();
        new (dataPtr()) QVariant(v);
    } else {
        *(QVariant *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(int v)
{
    if (type != QMetaType::Int) {
        cleanup();
        type = QMetaType::Int;
    }
    *(int *)(dataPtr()) = v;
}

void QQmlVMEVariant::setValue(bool v)
{
    if (type != QMetaType::Bool) {
        cleanup();
        type = QMetaType::Bool;
    }
    *(bool *)(dataPtr()) = v;
}

void QQmlVMEVariant::setValue(double v)
{
    if (type != QMetaType::Double) {
        cleanup();
        type = QMetaType::Double;
    }
    *(double *)(dataPtr()) = v;
}

void QQmlVMEVariant::setValue(const QString &v)
{
    if (type != QMetaType::QString) {
        cleanup();
        type = QMetaType::QString;
        new (dataPtr()) QString(v);
    } else {
        *(QString *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QUrl &v)
{
    if (type != QMetaType::QUrl) {
        cleanup();
        type = QMetaType::QUrl;
        new (dataPtr()) QUrl(v);
    } else {
        *(QUrl *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QTime &v)
{
    if (type != QMetaType::QTime) {
        cleanup();
        type = QMetaType::QTime;
        new (dataPtr()) QTime(v);
    } else {
        *(QTime *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QDate &v)
{
    if (type != QMetaType::QDate) {
        cleanup();
        type = QMetaType::QDate;
        new (dataPtr()) QDate(v);
    } else {
        *(QDate *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QDateTime &v)
{
    if (type != QMetaType::QDateTime) {
        cleanup();
        type = QMetaType::QDateTime;
        new (dataPtr()) QDateTime(v);
    } else {
        *(QDateTime *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QRectF &v)
{
    if (type != QMetaType::QRectF) {
        cleanup();
        type = QMetaType::QRectF;
        new (dataPtr()) QRectF(v);
    } else {
        *(QRectF *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QPointF &v)
{
    if (type != QMetaType::QPointF) {
        cleanup();
        type = QMetaType::QPointF;
        new (dataPtr()) QPointF(v);
    } else {
        *(QPointF *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QSizeF &v)
{
    if (type != QMetaType::QSizeF) {
        cleanup();
        type = QMetaType::QSizeF;
        new (dataPtr()) QSizeF(v);
    } else {
        *(QSizeF *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setValue(const QJSValue &v)
{
    if (type != qMetaTypeId<QJSValue>()) {
        cleanup();
        type = qMetaTypeId<QJSValue>();
        new (dataPtr()) QJSValue(v);
    } else {
        *(QJSValue *)(dataPtr()) = v;
    }
}

void QQmlVMEVariant::setDataType(int t)
{
    type = t;
}

void QQmlVMEVariant::ensureValueType(int t)
{
    if (type != t) {
        cleanup();
        type = t;
        QQml_valueTypeProvider()->initValueType(t, dataPtr(), dataSize());
    }
}

QQmlVMEMetaObjectEndpoint::QQmlVMEMetaObjectEndpoint()
{
    setCallback(QQmlNotifierEndpoint::QQmlVMEMetaObjectEndpoint);
}

void QQmlVMEMetaObjectEndpoint_callback(QQmlNotifierEndpoint *e, void **)
{
    QQmlVMEMetaObjectEndpoint *vmee = static_cast<QQmlVMEMetaObjectEndpoint*>(e);
    vmee->tryConnect();
}

void QQmlVMEMetaObjectEndpoint::tryConnect()
{
    int aliasId = this - metaObject->aliasEndpoints;

    if (metaObject.flag()) {
        // This is actually notify
        int sigIdx = metaObject->methodOffset() + aliasId + metaObject->metaData->propertyCount;
        metaObject->activate(metaObject->object, sigIdx, 0);
    } else {
        QQmlVMEMetaData::AliasData *d = metaObject->metaData->aliasData() + aliasId;
        if (!d->isObjectAlias()) {
            QQmlContextData *ctxt = metaObject->ctxt;
            QObject *target = ctxt->idValues[d->contextIdx].data();
            if (!target)
                return;

            if (d->notifySignal != -1)
                connect(target, d->notifySignal, ctxt->engine);
        }

        metaObject.setFlag();
    }
}

QAbstractDynamicMetaObject *QQmlVMEMetaObject::toDynamicMetaObject(QObject *o)
{
    if (!hasAssignedMetaObjectData) {
        *static_cast<QMetaObject *>(this) = *cache->createMetaObject();

        if (parent.isT1())
            this->d.superdata = parent.asT1()->toDynamicMetaObject(o);
        else
            this->d.superdata = parent.asT2();

        hasAssignedMetaObjectData = true;
    }

    return this;
}

QQmlVMEMetaObject::QQmlVMEMetaObject(QObject *obj,
                                     QQmlPropertyCache *cache,
                                     const QQmlVMEMetaData *meta, QV4::ExecutionContext *qmlBindingContext, QQmlCompiledData *compiledData)
: object(obj),
  ctxt(QQmlData::get(obj, true)->outerContext), cache(cache), metaData(meta),
  hasAssignedMetaObjectData(false), data(0), aliasEndpoints(0), firstVarPropertyIndex(-1),
  varPropertiesInitialized(false), interceptors(0), v8methods(0)
{
    QObjectPrivate *op = QObjectPrivate::get(obj);

    if (op->metaObject) {
        parent = op->metaObject;
        // Use the extra flag in QBiPointer to know if we can safely cast parent.asT1() to QQmlVMEMetaObject*
        parent.setFlagValue(QQmlData::get(obj)->hasVMEMetaObject);
    } else
        parent = obj->metaObject();

    op->metaObject = this;
    QQmlData::get(obj)->hasVMEMetaObject = true;

    data = new QQmlVMEVariant[metaData->propertyCount - metaData->varPropertyCount];

    aConnected.resize(metaData->aliasCount);
    int list_type = qMetaTypeId<QQmlListProperty<QObject> >();
    int qobject_type = qMetaTypeId<QObject*>();
    int variant_type = qMetaTypeId<QVariant>();
    // Need JS wrapper to ensure variant and var properties are marked.
    // ### FIXME: I hope that this can be removed once we have the proper scope chain
    // set up and the JS wrappers always exist.
    bool needsJSWrapper = (metaData->varPropertyCount > 0);

    // ### Optimize
    for (int ii = 0; ii < metaData->propertyCount - metaData->varPropertyCount; ++ii) {
        int t = (metaData->propertyData() + ii)->propertyType;
        if (t == list_type) {
            listProperties.append(List(methodOffset() + ii, this));
            data[ii].setValue(listProperties.count() - 1);
        } else if (!needsJSWrapper && (t == qobject_type || t == variant_type)) {
            needsJSWrapper = true;
        }
    }

    firstVarPropertyIndex = metaData->propertyCount - metaData->varPropertyCount;

    if (needsJSWrapper)
        ensureQObjectWrapper();

    if (qmlBindingContext && metaData->methodCount) {
        v8methods = new QV4::PersistentValue[metaData->methodCount];

        QV4::CompiledData::CompilationUnit *compilationUnit = compiledData->compilationUnit;
        QV4::Scope scope(QQmlEnginePrivate::get(ctxt->engine)->v4engine());
        QV4::ScopedObject o(scope);
        for (int index = 0; index < metaData->methodCount; ++index) {
            QQmlVMEMetaData::MethodData *data = metaData->methodData() + index;

            QV4::Function *runtimeFunction = compilationUnit->runtimeFunctions[data->runtimeFunctionIndex];
            o = QV4::FunctionObject::createScriptFunction(qmlBindingContext, runtimeFunction);
            v8methods[index] = o;
        }
    }
}

QQmlVMEMetaObject::~QQmlVMEMetaObject()
{
    if (parent.isT1()) parent.asT1()->objectDestroyed(object);
    delete [] data;
    delete [] aliasEndpoints;
    delete [] v8methods;

    qDeleteAll(varObjectGuards);
}

int QQmlVMEMetaObject::metaCall(QMetaObject::Call c, int _id, void **a)
{
    int id = _id;
    if (c == QMetaObject::WriteProperty && interceptors &&
       !(*reinterpret_cast<int*>(a[3]) & QQmlPropertyPrivate::BypassInterceptor)) {

        for (QQmlPropertyValueInterceptor *vi = interceptors; vi; vi = vi->m_next) {
            if (vi->m_coreIndex != id)
                continue;

            int valueIndex = vi->m_valueTypeCoreIndex;
            int type = QQmlData::get(object)->propertyCache->property(id)->propType;

            if (type != QVariant::Invalid) {
                if (valueIndex != -1) {
                    QQmlValueType *valueType = QQmlValueTypeFactory::valueType(type);
                    Q_ASSERT(valueType);

                    //
                    // Consider the following case:
                    //  color c = { 0.1, 0.2, 0.3 }
                    //  interceptor exists on c.r
                    //  write { 0.2, 0.4, 0.6 }
                    //
                    // The interceptor may choose not to update the r component at this
                    // point (for example, a behavior that creates an animation). But we
                    // need to ensure that the g and b components are updated correctly.
                    //
                    // So we need to perform a full write where the value type is:
                    //    r = old value, g = new value, b = new value
                    //
                    // And then call the interceptor which may or may not write the
                    // new value to the r component.
                    //
                    // This will ensure that the other components don't contain stale data
                    // and any relevant signals are emitted.
                    //
                    // To achieve this:
                    //   (1) Store the new value type as a whole (needed due to
                    //       aliasing between a[0] and static storage in value type).
                    //   (2) Read the entire existing value type from object -> valueType temp.
                    //   (3) Read the previous value of the component being changed
                    //       from the valueType temp.
                    //   (4) Write the entire new value type into the temp.
                    //   (5) Overwrite the component being changed with the old value.
                    //   (6) Perform a full write to the value type (which may emit signals etc).
                    //   (7) Issue the interceptor call with the new component value.
                    //

                    QMetaProperty valueProp = valueType->metaObject()->property(valueIndex);
                    QVariant newValue(type, a[0]);

                    valueType->read(object, id);
                    QVariant prevComponentValue = valueProp.read(valueType);

                    valueType->setValue(newValue);
                    QVariant newComponentValue = valueProp.read(valueType);

                    // Don't apply the interceptor if the intercepted value has not changed
                    bool updated = false;
                    if (newComponentValue != prevComponentValue) {
                        valueProp.write(valueType, prevComponentValue);
                        valueType->write(object, id, QQmlPropertyPrivate::DontRemoveBinding | QQmlPropertyPrivate::BypassInterceptor);

                        vi->write(newComponentValue);
                        updated = true;
                    }

                    if (updated)
                        return -1;
                } else {
                    vi->write(QVariant(type, a[0]));
                    return -1;
                }
            }
        }
    }
    if (c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty) {
        if (id >= propOffset()) {
            id -= propOffset();

            if (id < metaData->propertyCount) {
               int t = (metaData->propertyData() + id)->propertyType;
                bool needActivate = false;

                if (id >= firstVarPropertyIndex) {
                    Q_ASSERT(t == QMetaType::QVariant);
                    // the context can be null if accessing var properties from cpp after re-parenting an item.
                    QQmlEnginePrivate *ep = (ctxt == 0 || ctxt->engine == 0) ? 0 : QQmlEnginePrivate::get(ctxt->engine);
                    QV8Engine *v8e = (ep == 0) ? 0 : ep->v8engine();
                    if (v8e) {
                        if (c == QMetaObject::ReadProperty) {
                            *reinterpret_cast<QVariant *>(a[0]) = readPropertyAsVariant(id);
                        } else if (c == QMetaObject::WriteProperty) {
                            writeProperty(id, *reinterpret_cast<QVariant *>(a[0]));
                        }
                    } else if (c == QMetaObject::ReadProperty) {
                        // if the context was disposed, we just return an invalid variant from read.
                        *reinterpret_cast<QVariant *>(a[0]) = QVariant();
                    }

                } else {

                    if (c == QMetaObject::ReadProperty) {
                        switch(t) {
                        case QVariant::Int:
                            *reinterpret_cast<int *>(a[0]) = data[id].asInt();
                            break;
                        case QVariant::Bool:
                            *reinterpret_cast<bool *>(a[0]) = data[id].asBool();
                            break;
                        case QVariant::Double:
                            *reinterpret_cast<double *>(a[0]) = data[id].asDouble();
                            break;
                        case QVariant::String:
                            *reinterpret_cast<QString *>(a[0]) = data[id].asQString();
                            break;
                        case QVariant::Url:
                            *reinterpret_cast<QUrl *>(a[0]) = data[id].asQUrl();
                            break;
                        case QVariant::Date:
                            *reinterpret_cast<QDate *>(a[0]) = data[id].asQDate();
                            break;
                        case QVariant::DateTime:
                            *reinterpret_cast<QDateTime *>(a[0]) = data[id].asQDateTime();
                            break;
                        case QVariant::RectF:
                            *reinterpret_cast<QRectF *>(a[0]) = data[id].asQRectF();
                            break;
                        case QVariant::SizeF:
                            *reinterpret_cast<QSizeF *>(a[0]) = data[id].asQSizeF();
                            break;
                        case QVariant::PointF:
                            *reinterpret_cast<QPointF *>(a[0]) = data[id].asQPointF();
                            break;
                        case QMetaType::QObjectStar:
                            *reinterpret_cast<QObject **>(a[0]) = data[id].asQObject();
                            break;
                        case QMetaType::QVariant:
                            *reinterpret_cast<QVariant *>(a[0]) = readPropertyAsVariant(id);
                            break;
                        default:
                            QQml_valueTypeProvider()->readValueType(data[id].dataType(), data[id].dataPtr(), data->dataSize(), t, a[0]);
                            break;
                        }
                        if (t == qMetaTypeId<QQmlListProperty<QObject> >()) {
                            int listIndex = data[id].asInt();
                            const List *list = &listProperties.at(listIndex);
                            *reinterpret_cast<QQmlListProperty<QObject> *>(a[0]) =
                                QQmlListProperty<QObject>(object, (void *)list,
                                                                  list_append, list_count, list_at,
                                                                  list_clear);
                        }

                    } else if (c == QMetaObject::WriteProperty) {

                        switch(t) {
                        case QVariant::Int:
                            needActivate = *reinterpret_cast<int *>(a[0]) != data[id].asInt();
                            data[id].setValue(*reinterpret_cast<int *>(a[0]));
                            break;
                        case QVariant::Bool:
                            needActivate = *reinterpret_cast<bool *>(a[0]) != data[id].asBool();
                            data[id].setValue(*reinterpret_cast<bool *>(a[0]));
                            break;
                        case QVariant::Double:
                            needActivate = *reinterpret_cast<double *>(a[0]) != data[id].asDouble();
                            data[id].setValue(*reinterpret_cast<double *>(a[0]));
                            break;
                        case QVariant::String:
                            needActivate = *reinterpret_cast<QString *>(a[0]) != data[id].asQString();
                            data[id].setValue(*reinterpret_cast<QString *>(a[0]));
                            break;
                        case QVariant::Url:
                            needActivate = *reinterpret_cast<QUrl *>(a[0]) != data[id].asQUrl();
                            data[id].setValue(*reinterpret_cast<QUrl *>(a[0]));
                            break;
                        case QVariant::Date:
                            needActivate = *reinterpret_cast<QDate *>(a[0]) != data[id].asQDate();
                            data[id].setValue(*reinterpret_cast<QDate *>(a[0]));
                            break;
                        case QVariant::DateTime:
                            needActivate = *reinterpret_cast<QDateTime *>(a[0]) != data[id].asQDateTime();
                            data[id].setValue(*reinterpret_cast<QDateTime *>(a[0]));
                            break;
                        case QVariant::RectF:
                            needActivate = *reinterpret_cast<QRectF *>(a[0]) != data[id].asQRectF();
                            data[id].setValue(*reinterpret_cast<QRectF *>(a[0]));
                            break;
                        case QVariant::SizeF:
                            needActivate = *reinterpret_cast<QSizeF *>(a[0]) != data[id].asQSizeF();
                            data[id].setValue(*reinterpret_cast<QSizeF *>(a[0]));
                            break;
                        case QVariant::PointF:
                            needActivate = *reinterpret_cast<QPointF *>(a[0]) != data[id].asQPointF();
                            data[id].setValue(*reinterpret_cast<QPointF *>(a[0]));
                            break;
                        case QMetaType::QObjectStar:
                            needActivate = *reinterpret_cast<QObject **>(a[0]) != data[id].asQObject();
                            data[id].setValue(*reinterpret_cast<QObject **>(a[0]), this, id);
                            break;
                        case QMetaType::QVariant:
                            writeProperty(id, *reinterpret_cast<QVariant *>(a[0]));
                            break;
                        default:
                            data[id].ensureValueType(t);
                            needActivate = !QQml_valueTypeProvider()->equalValueType(t, a[0], data[id].dataPtr(), data[id].dataSize());
                            QQml_valueTypeProvider()->writeValueType(t, a[0], data[id].dataPtr(), data[id].dataSize());
                            break;
                        }
                    }

                }

                if (c == QMetaObject::WriteProperty && needActivate) {
                    activate(object, methodOffset() + id, 0);
                }

                return -1;
            }

            id -= metaData->propertyCount;

            if (id < metaData->aliasCount) {

                QQmlVMEMetaData::AliasData *d = metaData->aliasData() + id;

                if (d->flags & QML_ALIAS_FLAG_PTR && c == QMetaObject::ReadProperty)
                        *reinterpret_cast<void **>(a[0]) = 0;

                if (!ctxt) return -1;

                QQmlContext *context = ctxt->asQQmlContext();
                QQmlContextPrivate *ctxtPriv = QQmlContextPrivate::get(context);

                QObject *target = ctxtPriv->data->idValues[d->contextIdx].data();
                if (!target)
                    return -1;

                connectAlias(id);

                if (d->isObjectAlias()) {
                    *reinterpret_cast<QObject **>(a[0]) = target;
                    return -1;
                }

                // Remove binding (if any) on write
                if(c == QMetaObject::WriteProperty) {
                    int flags = *reinterpret_cast<int*>(a[3]);
                    if (flags & QQmlPropertyPrivate::RemoveBindingOnAliasWrite) {
                        QQmlData *targetData = QQmlData::get(target);
                        if (targetData && targetData->hasBindingBit(d->propertyIndex())) {
                            QQmlAbstractBinding *binding = QQmlPropertyPrivate::setBinding(target, d->propertyIndex(), d->isValueTypeAlias()?d->valueTypeIndex():-1, 0);
                            if (binding) binding->destroy();
                        }
                    }
                }

                if (d->isValueTypeAlias()) {
                    // Value type property
                    QQmlValueType *valueType = QQmlValueTypeFactory::valueType(d->valueType());
                    Q_ASSERT(valueType);

                    valueType->read(target, d->propertyIndex());
                    int rv = QMetaObject::metacall(valueType, c, d->valueTypeIndex(), a);

                    if (c == QMetaObject::WriteProperty)
                        valueType->write(target, d->propertyIndex(), 0x00);

                    return rv;

                } else {
                    return QMetaObject::metacall(target, c, d->propertyIndex(), a);
                }

            }
            return -1;

        }

    } else if(c == QMetaObject::InvokeMetaMethod) {

        if (id >= methodOffset()) {

            id -= methodOffset();
            int plainSignals = metaData->signalCount + metaData->propertyCount +
                               metaData->aliasCount;
            if (id < plainSignals) {
                activate(object, _id, a);
                return -1;
            }

            id -= plainSignals;

            if (id < metaData->methodCount) {
                if (!ctxt->engine)
                    return -1; // We can't run the method

                QQmlEnginePrivate *ep = QQmlEnginePrivate::get(ctxt->engine);
                ep->referenceScarceResources(); // "hold" scarce resources in memory during evaluation.
                QV4::Scope scope(ep->v4engine());


                QV4::Scoped<QV4::FunctionObject> function(scope, method(id));
                if (!function) {
                    // The function was not compiled.  There are some exceptional cases which the
                    // expression rewriter does not rewrite properly (e.g., \r-terminated lines
                    // are not rewritten correctly but this bug is deemed out-of-scope to fix for
                    // performance reasons; see QTBUG-24064) and thus compilation will have failed.
                    QQmlError e;
                    e.setDescription(QString(QLatin1String("Exception occurred during compilation of function: %1")).
                                     arg(QLatin1String(QMetaObject::method(_id).methodSignature().constData())));
                    ep->warning(e);
                    return -1; // The dynamic method with that id is not available.
                }

                QQmlVMEMetaData::MethodData *data = metaData->methodData() + id;

                QV4::ScopedCallData callData(scope, data->parameterCount);
                callData->thisObject = ep->v8engine()->global();

                for (int ii = 0; ii < data->parameterCount; ++ii)
                    callData->args[ii] = ep->v8engine()->fromVariant(*(QVariant *)a[ii + 1]);

                QV4::ScopedValue result(scope);
                QV4::ExecutionContext *ctx = function->engine()->currentContext();
                result = function->call(callData);
                if (scope.hasException()) {
                    QQmlError error = QV4::ExecutionEngine::catchExceptionAsQmlError(ctx);
                    if (error.isValid())
                        ep->warning(error);
                    if (a[0]) *(QVariant *)a[0] = QVariant();
                } else {
                    if (a[0]) *(QVariant *)a[0] = ep->v8engine()->toVariant(result, 0);
                }

                ep->dereferenceScarceResources(); // "release" scarce resources if top-level expression evaluation is complete.
                return -1;
            }
            return -1;
        }
    }

    if (parent.isT1())
        return parent.asT1()->metaCall(object, c, _id, a);
    else
        return object->qt_metacall(c, _id, a);
}

QV4::ReturnedValue QQmlVMEMetaObject::method(int index)
{
    if (!ctxt || !ctxt->isValid()) {
        qWarning("QQmlVMEMetaObject: Internal error - attempted to evaluate a function in an invalid context");
        return QV4::Primitive::undefinedValue().asReturnedValue();
    }

    if (!v8methods)
        v8methods = new QV4::PersistentValue[metaData->methodCount];

    return v8methods[index].value();
}

QV4::ReturnedValue QQmlVMEMetaObject::readVarProperty(int id)
{
    Q_ASSERT(id >= firstVarPropertyIndex);

    if (ensureVarPropertiesAllocated()) {
        QV4::Scope scope(varProperties.engine());
        QV4::ScopedObject o(scope, varProperties.value());
        return o->getIndexed(id - firstVarPropertyIndex);
    }
    return QV4::Primitive::undefinedValue().asReturnedValue();
}

QVariant QQmlVMEMetaObject::readPropertyAsVariant(int id)
{
    if (id >= firstVarPropertyIndex) {
        if (ensureVarPropertiesAllocated()) {
            QV4::ExecutionEngine *v4 = varProperties.engine();
            QV4::Scope scope(v4);
            QV4::ScopedObject o(scope, varProperties.value());
            QV4::ScopedValue val(scope, o->getIndexed(id - firstVarPropertyIndex));
            return v4->v8Engine->toVariant(val, -1);
        }
        return QVariant();
    } else {
        if (data[id].dataType() == QMetaType::QObjectStar) {
            return QVariant::fromValue(data[id].asQObject());
        } else {
            return data[id].asQVariant();
        }
    }
}

void QQmlVMEMetaObject::writeVarProperty(int id, const QV4::ValueRef value)
{
    Q_ASSERT(id >= firstVarPropertyIndex);
    if (!ensureVarPropertiesAllocated())
        return;

    QV4::Scope scope(varProperties.engine());
    // Importantly, if the current value is a scarce resource, we need to ensure that it
    // gets automatically released by the engine if no other references to it exist.
    QV4::ScopedObject vp(scope, varProperties.value());
    QV4::Scoped<QV4::VariantObject> oldv(scope, vp->getIndexed(id - firstVarPropertyIndex));
    if (!!oldv)
        oldv->removeVmePropertyReference();

    QObject *valueObject = 0;
    QQmlVMEVariantQObjectPtr *guard = getQObjectGuardForProperty(id);

    QV4::ScopedObject o(scope, value);
    if (o) {
        // And, if the new value is a scarce resource, we need to ensure that it does not get
        // automatically released by the engine until no other references to it exist.
        if (QV4::VariantObject *v = o->as<QV4::VariantObject>()) {
            v->addVmePropertyReference();
        } else if (QV4::QObjectWrapper *wrapper = o->as<QV4::QObjectWrapper>()) {
            // We need to track this QObject to signal its deletion
            valueObject = wrapper->object();

            // Do we already have a QObject guard for this property?
            if (valueObject && !guard) {
                guard = new QQmlVMEVariantQObjectPtr(true);
                varObjectGuards.append(guard);
            }
        }
    }

    if (guard) {
        guard->setGuardedValue(valueObject, this, id);
    }

    // Write the value and emit change signal as appropriate.
    vp->putIndexed(id - firstVarPropertyIndex, value);
    activate(object, methodOffset() + id, 0);
}

void QQmlVMEMetaObject::writeProperty(int id, const QVariant &value)
{
    if (id >= firstVarPropertyIndex) {
        if (!ensureVarPropertiesAllocated())
            return;

        QV4::Scope scope(varProperties.engine());

        // Importantly, if the current value is a scarce resource, we need to ensure that it
        // gets automatically released by the engine if no other references to it exist.
        QV4::ScopedObject vp(scope, varProperties.value());
        QV4::Scoped<QV4::VariantObject> oldv(scope, vp->getIndexed(id - firstVarPropertyIndex));
        if (!!oldv)
            oldv->removeVmePropertyReference();

        // And, if the new value is a scarce resource, we need to ensure that it does not get
        // automatically released by the engine until no other references to it exist.
        QV4::ScopedValue newv(scope, QQmlEnginePrivate::get(ctxt->engine)->v8engine()->fromVariant(value));
        QV4::Scoped<QV4::VariantObject> v(scope, newv);
        if (!!v)
            v->addVmePropertyReference();

        // Write the value and emit change signal as appropriate.
        QVariant currentValue = readPropertyAsVariant(id);
        vp->putIndexed(id - firstVarPropertyIndex, newv);
        if ((currentValue.userType() != value.userType() || currentValue != value))
            activate(object, methodOffset() + id, 0);
    } else {
        bool needActivate = false;
        if (value.userType() == QMetaType::QObjectStar) {
            QObject *o = *(QObject **)value.data();
            needActivate = (data[id].dataType() != QMetaType::QObjectStar || data[id].asQObject() != o);
            data[id].setValue(o, this, id);
        } else {
            needActivate = (data[id].dataType() != qMetaTypeId<QVariant>() ||
                            data[id].asQVariant().userType() != value.userType() ||
                            data[id].asQVariant() != value);
            data[id].setValue(value);
        }

        if (needActivate)
            activate(object, methodOffset() + id, 0);
    }
}

void QQmlVMEMetaObject::listChanged(int id)
{
    activate(object, methodOffset() + id, 0);
}

void QQmlVMEMetaObject::list_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    List *list = static_cast<List *>(prop->data);
    list->append(o);
    list->mo->activate(prop->object, list->notifyIndex, 0);
}

int QQmlVMEMetaObject::list_count(QQmlListProperty<QObject> *prop)
{
    return static_cast<List *>(prop->data)->count();
}

QObject *QQmlVMEMetaObject::list_at(QQmlListProperty<QObject> *prop, int index)
{
    return static_cast<List *>(prop->data)->at(index);
}

void QQmlVMEMetaObject::list_clear(QQmlListProperty<QObject> *prop)
{
    List *list = static_cast<List *>(prop->data);
    list->clear();
    list->mo->activate(prop->object, list->notifyIndex, 0);
}

void QQmlVMEMetaObject::registerInterceptor(int index, int valueIndex, QQmlPropertyValueInterceptor *interceptor)
{
    interceptor->m_coreIndex = index;
    interceptor->m_valueTypeCoreIndex = valueIndex;
    interceptor->m_next = interceptors;
    interceptors = interceptor;
}

quint16 QQmlVMEMetaObject::vmeMethodLineNumber(int index)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeMethodLineNumber(index);
    }

    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));

    int rawIndex = index - methodOffset() - plainSignals;

    QQmlVMEMetaData::MethodData *data = metaData->methodData() + rawIndex;
    return data->lineNumber;
}

QV4::ReturnedValue QQmlVMEMetaObject::vmeMethod(int index)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeMethod(index);
    }
    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));
    return method(index - methodOffset() - plainSignals);
}

// Used by debugger
void QQmlVMEMetaObject::setVmeMethod(int index, QV4::ValueRef function)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->setVmeMethod(index, function);
    }
    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));

    if (!v8methods)
        v8methods = new QV4::PersistentValue[metaData->methodCount];

    int methodIndex = index - methodOffset() - plainSignals;
    v8methods[methodIndex] = function;
}

QV4::ReturnedValue QQmlVMEMetaObject::vmeProperty(int index)
{
    if (index < propOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeProperty(index);
    }
    return readVarProperty(index - propOffset());
}

void QQmlVMEMetaObject::setVMEProperty(int index, const QV4::ValueRef v)
{
    if (index < propOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        parentVMEMetaObject()->setVMEProperty(index, v);
        return;
    }
    return writeVarProperty(index - propOffset(), v);
}

bool QQmlVMEMetaObject::ensureVarPropertiesAllocated()
{
    if (!varPropertiesInitialized)
        allocateVarPropertiesArray();

    // in some situations, the QObject's v8object (and associated v8 data,
    // such as the varProperties array) will have been cleaned up, but the
    // QObject ptr will not yet have been deleted (eg, waiting on deleteLater).
    // In this situation, the varProperties handle will be (and should remain)
    // empty.
    return !varProperties.isUndefined();
}

void QQmlVMEMetaObject::ensureQObjectWrapper()
{
    QQmlEnginePrivate *ep = (ctxt == 0 || ctxt->engine == 0) ? 0 : QQmlEnginePrivate::get(ctxt->engine);
    QV4::ExecutionEngine *v4 = (ep == 0) ? 0 : ep->v4engine();
    QV4::QObjectWrapper::wrap(v4, object);
}

void QQmlVMEMetaObject::mark(QV4::ExecutionEngine *e)
{
    varProperties.markOnce(e);

    // add references created by VMEVariant properties
    int maxDataIdx = metaData->propertyCount - metaData->varPropertyCount;
    for (int ii = 0; ii < maxDataIdx; ++ii) { // XXX TODO: optimize?
        if (data[ii].dataType() == QMetaType::QObjectStar) {
            // possible QObject reference.
            QObject *ref = data[ii].asQObject();
            if (ref) {
                QQmlData *ddata = QQmlData::get(ref);
                if (ddata)
                    ddata->jsWrapper.markOnce(e);
            }
        }
    }

    if (QQmlVMEMetaObject *parent = parentVMEMetaObject())
        parent->mark(e);
}

void QQmlVMEMetaObject::allocateVarPropertiesArray()
{
    QQmlEngine *qml = qmlEngine(object);
    assert(qml);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(qml->handle());
    QV4::Scope scope(v4);
    varProperties = QV4::ScopedValue(scope, v4->newArrayObject(metaData->varPropertyCount));
    varPropertiesInitialized = true;
}

bool QQmlVMEMetaObject::aliasTarget(int index, QObject **target, int *coreIndex, int *valueTypeIndex) const
{
    Q_ASSERT(index >= propOffset() + metaData->propertyCount);

    *target = 0;
    *coreIndex = -1;
    *valueTypeIndex = -1;

    if (!ctxt)
        return false;

    QQmlVMEMetaData::AliasData *d = metaData->aliasData() + (index - propOffset() - metaData->propertyCount);
    QQmlContext *context = ctxt->asQQmlContext();
    QQmlContextPrivate *ctxtPriv = QQmlContextPrivate::get(context);

    *target = ctxtPriv->data->idValues[d->contextIdx].data();
    if (!*target)
        return false;

    if (d->isObjectAlias()) {
    } else if (d->isValueTypeAlias()) {
        *coreIndex = d->propertyIndex();
        *valueTypeIndex = d->valueTypeIndex();
    } else {
        *coreIndex = d->propertyIndex();
    }

    return true;
}

void QQmlVMEMetaObject::connectAlias(int aliasId)
{
    if (!aConnected.testBit(aliasId)) {

        if (!aliasEndpoints)
            aliasEndpoints = new QQmlVMEMetaObjectEndpoint[metaData->aliasCount];

        aConnected.setBit(aliasId);

        QQmlVMEMetaData::AliasData *d = metaData->aliasData() + aliasId;

        QQmlVMEMetaObjectEndpoint *endpoint = aliasEndpoints + aliasId;
        endpoint->metaObject = this;

        endpoint->connect(&ctxt->idValues[d->contextIdx].bindings);

        endpoint->tryConnect();
    }
}

void QQmlVMEMetaObject::connectAliasSignal(int index, bool indexInSignalRange)
{
    int aliasId = (index - (indexInSignalRange ? signalOffset() : methodOffset())) - metaData->propertyCount;
    if (aliasId < 0 || aliasId >= metaData->aliasCount)
        return;

    connectAlias(aliasId);
}

/*! \internal
    \a index is in the method index range (QMetaMethod::methodIndex()).
*/
void QQmlVMEMetaObject::activate(QObject *object, int index, void **args)
{
    QMetaObject::activate(object, signalOffset(), index - methodOffset(), args);
}

QQmlVMEMetaObject *QQmlVMEMetaObject::getForProperty(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->propOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

QQmlVMEMetaObject *QQmlVMEMetaObject::getForMethod(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->methodOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

/*! \internal
    \a coreIndex is in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QQmlVMEMetaObject *QQmlVMEMetaObject::getForSignal(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->signalOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

QQmlVMEVariantQObjectPtr *QQmlVMEMetaObject::getQObjectGuardForProperty(int index) const
{
    QList<QQmlVMEVariantQObjectPtr *>::ConstIterator it = varObjectGuards.constBegin(), end = varObjectGuards.constEnd();
    for ( ; it != end; ++it) {
        if ((*it)->m_index == index) {
            return *it;
        }
    }

    return 0;
}

QT_END_NAMESPACE
