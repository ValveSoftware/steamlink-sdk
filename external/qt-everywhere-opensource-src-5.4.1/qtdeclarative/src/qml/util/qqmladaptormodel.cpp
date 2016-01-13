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

#include "qqmladaptormodel_p.h"

#include <private/qqmldelegatemodel_p_p.h>
#include <private/qmetaobjectbuilder_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qv8engine_p.h>

#include <private/qv4value_inl_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlAdaptorModelEngineData : public QV8Engine::Deletable
{
public:
    QQmlAdaptorModelEngineData(QV8Engine *engine);
    ~QQmlAdaptorModelEngineData();

    QV4::ExecutionEngine *v4;
    QV4::PersistentValue listItemProto;
};

V8_DEFINE_EXTENSION(QQmlAdaptorModelEngineData, engineData)

static QV4::ReturnedValue get_index(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));

    return QV4::Encode(o->d()->item->index);
}

template <typename T, typename M> static void setModelDataType(QMetaObjectBuilder *builder, M *metaType)
{
    builder->setFlags(QMetaObjectBuilder::DynamicMetaObject);
    builder->setClassName(T::staticMetaObject.className());
    builder->setSuperClass(&T::staticMetaObject);
    metaType->propertyOffset = T::staticMetaObject.propertyCount();
    metaType->signalOffset = T::staticMetaObject.methodCount();
}

static void addProperty(QMetaObjectBuilder *builder, int propertyId, const QByteArray &propertyName, const QByteArray &propertyType)
{
    builder->addSignal("__" + QByteArray::number(propertyId) + "()");
    QMetaPropertyBuilder property = builder->addProperty(
            propertyName, propertyType, propertyId);
    property.setWritable(true);
}

class VDMModelDelegateDataType;

class QQmlDMCachedModelData : public QQmlDelegateModelItem
{
public:
    QQmlDMCachedModelData(
            QQmlDelegateModelItemMetaType *metaType,
            VDMModelDelegateDataType *dataType,
            int index);

    int metaCall(QMetaObject::Call call, int id, void **arguments);

    virtual QVariant value(int role) const = 0;
    virtual void setValue(int role, const QVariant &value) = 0;

    void setValue(const QString &role, const QVariant &value);
    bool resolveIndex(const QQmlAdaptorModel &model, int idx);

    static QV4::ReturnedValue get_property(QV4::CallContext *ctx, uint propertyId);
    static QV4::ReturnedValue set_property(QV4::CallContext *ctx, uint propertyId);

    VDMModelDelegateDataType *type;
    QVector<QVariant> cachedData;
};

class VDMModelDelegateDataType
        : public QQmlRefCount
        , public QQmlAdaptorModel::Accessors
        , public QAbstractDynamicMetaObject
{
public:
    VDMModelDelegateDataType(QQmlAdaptorModel *model)
        : model(model)
        , metaObject(0)
        , propertyCache(0)
        , propertyOffset(0)
        , signalOffset(0)
        , hasModelData(false)
    {
    }

    ~VDMModelDelegateDataType()
    {
        if (propertyCache)
            propertyCache->release();
        free(metaObject);
    }

    bool notify(
            const QQmlAdaptorModel &,
            const QList<QQmlDelegateModelItem *> &items,
            int index,
            int count,
            const QVector<int> &roles) const
    {
        bool changed = roles.isEmpty() && !watchedRoles.isEmpty();
        if (!changed && !watchedRoles.isEmpty() && watchedRoleIds.isEmpty()) {
            QList<int> roleIds;
            foreach (const QByteArray &r, watchedRoles) {
                QHash<QByteArray, int>::const_iterator it = roleNames.find(r);
                if (it != roleNames.end())
                    roleIds << it.value();
            }
            const_cast<VDMModelDelegateDataType *>(this)->watchedRoleIds = roleIds;
        }

        QVector<int> signalIndexes;
        for (int i = 0; i < roles.count(); ++i) {
            const int role = roles.at(i);
            if (!changed && watchedRoleIds.contains(role))
                changed = true;

            int propertyId = propertyRoles.indexOf(role);
            if (propertyId != -1)
                signalIndexes.append(propertyId + signalOffset);
        }
        if (roles.isEmpty()) {
            for (int propertyId = 0; propertyId < propertyRoles.count(); ++propertyId)
                signalIndexes.append(propertyId + signalOffset);
        }

        for (int i = 0, c = items.count();  i < c; ++i) {
            QQmlDelegateModelItem *item = items.at(i);
            const int idx = item->modelIndex();
            if (idx >= index && idx < index + count) {
                for (int i = 0; i < signalIndexes.count(); ++i)
                    QMetaObject::activate(item, signalIndexes.at(i), 0);
            }
        }
        return changed;
    }

    void replaceWatchedRoles(
            QQmlAdaptorModel &,
            const QList<QByteArray> &oldRoles,
            const QList<QByteArray> &newRoles) const
    {
        VDMModelDelegateDataType *dataType = const_cast<VDMModelDelegateDataType *>(this);

        dataType->watchedRoleIds.clear();
        foreach (const QByteArray &oldRole, oldRoles)
            dataType->watchedRoles.removeOne(oldRole);
        dataType->watchedRoles += newRoles;
    }

    static QV4::ReturnedValue get_hasModelChildren(QV4::CallContext *ctx)
    {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
        if (!o)
            return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));

        const QQmlAdaptorModel *const model = static_cast<QQmlDMCachedModelData *>(o->d()->item)->type->model;
        if (o->d()->item->index >= 0 && *model) {
            const QAbstractItemModel * const aim = model->aim();
            return QV4::Encode(aim->hasChildren(aim->index(o->d()->item->index, 0, model->rootIndex)));
        } else {
            return QV4::Encode(false);
        }
    }


    void initializeConstructor(QQmlAdaptorModelEngineData *const data)
    {
        QV4::ExecutionEngine *v4 = data->v4;
        QV4::Scope scope(v4);
        QV4::Scoped<QV4::Object> proto(scope, v4->newObject());
        proto->defineAccessorProperty(QStringLiteral("index"), get_index, 0);
        proto->defineAccessorProperty(QStringLiteral("hasModelChildren"), get_hasModelChildren, 0);
        QV4::ScopedProperty p(scope);

        typedef QHash<QByteArray, int>::const_iterator iterator;
        for (iterator it = roleNames.constBegin(), end = roleNames.constEnd(); it != end; ++it) {
            const int propertyId = propertyRoles.indexOf(it.value());
            const QByteArray &propertyName = it.key();

            QV4::ScopedString name(scope, v4->newString(QString::fromUtf8(propertyName)));
            QV4::ScopedFunctionObject g(scope, v4->memoryManager->alloc<QV4::IndexedBuiltinFunction>(v4->rootContext, propertyId, QQmlDMCachedModelData::get_property));
            QV4::ScopedFunctionObject s(scope, v4->memoryManager->alloc<QV4::IndexedBuiltinFunction>(v4->rootContext, propertyId, QQmlDMCachedModelData::set_property));
            p->setGetter(g);
            p->setSetter(s);
            proto->insertMember(name.getPointer(), p, QV4::Attr_Accessor|QV4::Attr_NotEnumerable|QV4::Attr_NotConfigurable);
        }
        prototype = proto;
    }

    // QAbstractDynamicMetaObject

    void objectDestroyed(QObject *)
    {
        release();
    }

    int metaCall(QObject *object, QMetaObject::Call call, int id, void **arguments)
    {
        return static_cast<QQmlDMCachedModelData *>(object)->metaCall(call, id, arguments);
    }

    QV4::PersistentValue prototype;
    QList<int> propertyRoles;
    QList<int> watchedRoleIds;
    QList<QByteArray> watchedRoles;
    QHash<QByteArray, int> roleNames;
    QQmlAdaptorModel *model;
    QMetaObject *metaObject;
    QQmlPropertyCache *propertyCache;
    int propertyOffset;
    int signalOffset;
    bool hasModelData;
};

QQmlDMCachedModelData::QQmlDMCachedModelData(
        QQmlDelegateModelItemMetaType *metaType, VDMModelDelegateDataType *dataType, int index)
    : QQmlDelegateModelItem(metaType, index)
    , type(dataType)
{
    if (index == -1)
        cachedData.resize(type->hasModelData ? 1 : type->propertyRoles.count());

    QObjectPrivate::get(this)->metaObject = type;

    type->addref();

    QQmlData *qmldata = QQmlData::get(this, true);
    qmldata->propertyCache = dataType->propertyCache;
    qmldata->propertyCache->addref();
}

int QQmlDMCachedModelData::metaCall(QMetaObject::Call call, int id, void **arguments)
{
    if (call == QMetaObject::ReadProperty && id >= type->propertyOffset) {
        const int propertyIndex = id - type->propertyOffset;
        if (index == -1) {
            if (!cachedData.isEmpty()) {
                *static_cast<QVariant *>(arguments[0]) = cachedData.at(
                    type->hasModelData ? 0 : propertyIndex);
            }
        } else  if (*type->model) {
            *static_cast<QVariant *>(arguments[0]) = value(type->propertyRoles.at(propertyIndex));
        }
        return -1;
    } else if (call == QMetaObject::WriteProperty && id >= type->propertyOffset) {
        const int propertyIndex = id - type->propertyOffset;
        if (index == -1) {
            const QMetaObject *meta = metaObject();
            if (cachedData.count() > 1) {
                cachedData[propertyIndex] = *static_cast<QVariant *>(arguments[0]);
                QMetaObject::activate(this, meta, propertyIndex, 0);
            } else if (cachedData.count() == 1) {
                cachedData[0] = *static_cast<QVariant *>(arguments[0]);
                QMetaObject::activate(this, meta, 0, 0);
                QMetaObject::activate(this, meta, 1, 0);
            }
        } else if (*type->model) {
            setValue(type->propertyRoles.at(propertyIndex), *static_cast<QVariant *>(arguments[0]));
        }
        return -1;
    } else {
        return qt_metacall(call, id, arguments);
    }
}

void QQmlDMCachedModelData::setValue(const QString &role, const QVariant &value)
{
    QHash<QByteArray, int>::iterator it = type->roleNames.find(role.toUtf8());
    if (it != type->roleNames.end()) {
        for (int i = 0; i < type->propertyRoles.count(); ++i) {
            if (type->propertyRoles.at(i) == *it) {
                cachedData[i] = value;
                return;
            }
        }
    }
}

bool QQmlDMCachedModelData::resolveIndex(const QQmlAdaptorModel &, int idx)
{
    if (index == -1) {
        Q_ASSERT(idx >= 0);
        index = idx;
        cachedData.clear();
        emit modelIndexChanged();
        const QMetaObject *meta = metaObject();
        const int propertyCount = type->propertyRoles.count();
        for (int i = 0; i < propertyCount; ++i)
            QMetaObject::activate(this, meta, i, 0);
        return true;
    } else {
        return false;
    }
}

QV4::ReturnedValue QQmlDMCachedModelData::get_property(QV4::CallContext *ctx, uint propertyId)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));

    QQmlDMCachedModelData *modelData = static_cast<QQmlDMCachedModelData *>(o->d()->item);
    if (o->d()->item->index == -1) {
        if (!modelData->cachedData.isEmpty()) {
            return ctx->d()->engine->v8Engine->fromVariant(
                    modelData->cachedData.at(modelData->type->hasModelData ? 0 : propertyId));
        }
    } else if (*modelData->type->model) {
        return ctx->d()->engine->v8Engine->fromVariant(
                modelData->value(modelData->type->propertyRoles.at(propertyId)));
    }
    return QV4::Encode::undefined();
}

QV4::ReturnedValue QQmlDMCachedModelData::set_property(QV4::CallContext *ctx, uint propertyId)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));
    if (!ctx->d()->callData->argc)
        return ctx->throwTypeError();

    if (o->d()->item->index == -1) {
        QQmlDMCachedModelData *modelData = static_cast<QQmlDMCachedModelData *>(o->d()->item);
        if (!modelData->cachedData.isEmpty()) {
            if (modelData->cachedData.count() > 1) {
                modelData->cachedData[propertyId] = scope.engine->v8Engine->toVariant(ctx->d()->callData->args[0], QVariant::Invalid);
                QMetaObject::activate(o->d()->item, o->d()->item->metaObject(), propertyId, 0);
            } else if (modelData->cachedData.count() == 1) {
                modelData->cachedData[0] = scope.engine->v8Engine->toVariant(ctx->d()->callData->args[0], QVariant::Invalid);
                QMetaObject::activate(o->d()->item, o->d()->item->metaObject(), 0, 0);
                QMetaObject::activate(o->d()->item, o->d()->item->metaObject(), 1, 0);
            }
        }
    }
    return QV4::Encode::undefined();
}

//-----------------------------------------------------------------
// QAbstractItemModel
//-----------------------------------------------------------------

class QQmlDMAbstractItemModelData : public QQmlDMCachedModelData
{
    Q_OBJECT
    Q_PROPERTY(bool hasModelChildren READ hasModelChildren CONSTANT)
public:
    QQmlDMAbstractItemModelData(
            QQmlDelegateModelItemMetaType *metaType,
            VDMModelDelegateDataType *dataType,
            int index)
        : QQmlDMCachedModelData(metaType, dataType, index)
    {
    }

    bool hasModelChildren() const
    {
        if (index >= 0 && *type->model) {
            const QAbstractItemModel * const model = type->model->aim();
            return model->hasChildren(model->index(index, 0, type->model->rootIndex));
        } else {
            return false;
        }
    }

    QVariant value(int role) const
    {
        return type->model->aim()->index(index, 0, type->model->rootIndex).data(role);
    }

    void setValue(int role, const QVariant &value)
    {
        type->model->aim()->setData(
                type->model->aim()->index(index, 0, type->model->rootIndex), value, role);
    }

    QV4::ReturnedValue get()
    {
        if (type->prototype.isUndefined()) {
            QQmlAdaptorModelEngineData * const data = engineData(v4->v8Engine);
            type->initializeConstructor(data);
        }
        QV4::Scope scope(v4);
        QV4::ScopedObject proto(scope, type->prototype.value());
        QV4::ScopedObject o(scope, proto->engine()->memoryManager->alloc<QQmlDelegateModelItemObject>(proto->engine(), this));
        o->setPrototype(proto.getPointer());
        ++scriptRef;
        return o.asReturnedValue();
    }
};

class VDMAbstractItemModelDataType : public VDMModelDelegateDataType
{
public:
    VDMAbstractItemModelDataType(QQmlAdaptorModel *model)
        : VDMModelDelegateDataType(model)
    {
    }

    int count(const QQmlAdaptorModel &model) const
    {
        return model.aim()->rowCount(model.rootIndex);
    }

    void cleanup(QQmlAdaptorModel &model, QQmlDelegateModel *vdm) const
    {
        QAbstractItemModel * const aim = model.aim();
        if (aim && vdm) {
            QObject::disconnect(aim, SIGNAL(rowsInserted(QModelIndex,int,int)),
                                vdm, SLOT(_q_rowsInserted(QModelIndex,int,int)));
            QObject::disconnect(aim, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                                vdm, SLOT(_q_rowsAboutToBeRemoved(QModelIndex,int,int)));
            QObject::disconnect(aim, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                                vdm, SLOT(_q_rowsRemoved(QModelIndex,int,int)));
            QObject::disconnect(aim, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                                vdm, SLOT(_q_dataChanged(QModelIndex,QModelIndex,QVector<int>)));
            QObject::disconnect(aim, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                                vdm, SLOT(_q_rowsMoved(QModelIndex,int,int,QModelIndex,int)));
            QObject::disconnect(aim, SIGNAL(modelReset()),
                                vdm, SLOT(_q_modelReset()));
            QObject::disconnect(aim, SIGNAL(layoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
                                vdm, SLOT(_q_layoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
            QObject::disconnect(aim, SIGNAL(layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
                                vdm, SLOT(_q_layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
        }

        const_cast<VDMAbstractItemModelDataType *>(this)->release();
    }

    QVariant value(const QQmlAdaptorModel &model, int index, const QString &role) const
    {
        QHash<QByteArray, int>::const_iterator it = roleNames.find(role.toUtf8());
        if (it != roleNames.end()) {
            return model.aim()->index(index, 0, model.rootIndex).data(*it);
        } else if (role == QLatin1String("hasModelChildren")) {
            return QVariant(model.aim()->hasChildren(model.aim()->index(index, 0, model.rootIndex)));
        } else {
            return QVariant();
        }
    }

    QVariant parentModelIndex(const QQmlAdaptorModel &model) const
    {
        return model
                ? QVariant::fromValue(model.aim()->parent(model.rootIndex))
                : QVariant();
    }

    QVariant modelIndex(const QQmlAdaptorModel &model, int index) const
    {
        return model
                ? QVariant::fromValue(model.aim()->index(index, 0, model.rootIndex))
                : QVariant();
    }

    bool canFetchMore(const QQmlAdaptorModel &model) const
    {
        return model && model.aim()->canFetchMore(model.rootIndex);
    }

    void fetchMore(QQmlAdaptorModel &model) const
    {
        if (model)
            model.aim()->fetchMore(model.rootIndex);
    }

    QQmlDelegateModelItem *createItem(
            QQmlAdaptorModel &model,
            QQmlDelegateModelItemMetaType *metaType,
            QQmlEngine *engine,
            int index) const
    {
        VDMAbstractItemModelDataType *dataType = const_cast<VDMAbstractItemModelDataType *>(this);
        if (!metaObject)
            dataType->initializeMetaType(model, engine);
        return new QQmlDMAbstractItemModelData(metaType, dataType, index);
    }

    void initializeMetaType(QQmlAdaptorModel &model, QQmlEngine *engine)
    {
        QMetaObjectBuilder builder;
        setModelDataType<QQmlDMAbstractItemModelData>(&builder, this);

        const QByteArray propertyType = QByteArrayLiteral("QVariant");
        const QHash<int, QByteArray> names = model.aim()->roleNames();
        for (QHash<int, QByteArray>::const_iterator it = names.begin(); it != names.end(); ++it) {
            const int propertyId = propertyRoles.count();
            propertyRoles.append(it.key());
            roleNames.insert(it.value(), it.key());
            addProperty(&builder, propertyId, it.value(), propertyType);
        }
        if (propertyRoles.count() == 1) {
            hasModelData = true;
            const int role = names.begin().key();
            const QByteArray propertyName = QByteArrayLiteral("modelData");

            propertyRoles.append(role);
            roleNames.insert(propertyName, role);
            addProperty(&builder, 1, propertyName, propertyType);
        }

        metaObject = builder.toMetaObject();
        *static_cast<QMetaObject *>(this) = *metaObject;
        propertyCache = new QQmlPropertyCache(engine, metaObject);
    }
};

//-----------------------------------------------------------------
// QQmlListAccessor
//-----------------------------------------------------------------

class QQmlDMListAccessorData : public QQmlDelegateModelItem
{
    Q_OBJECT
    Q_PROPERTY(QVariant modelData READ modelData WRITE setModelData NOTIFY modelDataChanged)
public:
    QQmlDMListAccessorData(QQmlDelegateModelItemMetaType *metaType, int index, const QVariant &value)
        : QQmlDelegateModelItem(metaType, index)
        , cachedData(value)
    {
    }

    QVariant modelData() const
    {
        return cachedData;
    }

    void setModelData(const QVariant &data)
    {
        if (index == -1 && data != cachedData) {
            cachedData = data;
            emit modelDataChanged();
        }
    }

    static QV4::ReturnedValue get_modelData(QV4::CallContext *ctx)
    {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
        if (!o)
            return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));

        return scope.engine->v8Engine->fromVariant(static_cast<QQmlDMListAccessorData *>(o->d()->item)->cachedData);
    }

    static QV4::ReturnedValue set_modelData(QV4::CallContext *ctx)
    {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->d()->callData->thisObject.as<QQmlDelegateModelItemObject>());
        if (!o)
            return ctx->throwTypeError(QStringLiteral("Not a valid VisualData object"));
        if (!ctx->d()->callData->argc)
            return ctx->throwTypeError();

        static_cast<QQmlDMListAccessorData *>(o->d()->item)->setModelData(scope.engine->v8Engine->toVariant(ctx->d()->callData->args[0], QVariant::Invalid));
        return QV4::Encode::undefined();
    }

    QV4::ReturnedValue get()
    {
        QQmlAdaptorModelEngineData *data = engineData(v4->v8Engine);
        QV4::Scope scope(v4);
        QV4::ScopedObject o(scope, v4->memoryManager->alloc<QQmlDelegateModelItemObject>(v4, this));
        QV4::ScopedObject p(scope, data->listItemProto.value());
        o->setPrototype(p.getPointer());
        ++scriptRef;
        return o.asReturnedValue();
    }

    void setValue(const QString &role, const QVariant &value)
    {
        if (role == QLatin1String("modelData"))
            cachedData = value;
    }

    bool resolveIndex(const QQmlAdaptorModel &model, int idx)
    {
        if (index == -1) {
            index = idx;
            cachedData = model.list.at(idx);
            emit modelIndexChanged();
            emit modelDataChanged();
            return true;
        } else {
            return false;
        }
    }


Q_SIGNALS:
    void modelDataChanged();

private:
    QVariant cachedData;
};


class VDMListDelegateDataType : public QQmlAdaptorModel::Accessors
{
public:
    inline VDMListDelegateDataType() {}

    int count(const QQmlAdaptorModel &model) const
    {
        return model.list.count();
    }

    QVariant value(const QQmlAdaptorModel &model, int index, const QString &role) const
    {
        return role == QLatin1String("modelData")
                ? model.list.at(index)
                : QVariant();
    }

    QQmlDelegateModelItem *createItem(
            QQmlAdaptorModel &model,
            QQmlDelegateModelItemMetaType *metaType,
            QQmlEngine *,
            int index) const
    {
        return new QQmlDMListAccessorData(
                metaType,
                index,
                index >= 0 && index < model.list.count() ? model.list.at(index) : QVariant());
    }
};

//-----------------------------------------------------------------
// QObject
//-----------------------------------------------------------------

class VDMObjectDelegateDataType;
class QQmlDMObjectData : public QQmlDelegateModelItem, public QQmlAdaptorModelProxyInterface
{
    Q_OBJECT
    Q_PROPERTY(QObject *modelData READ modelData CONSTANT)
    Q_INTERFACES(QQmlAdaptorModelProxyInterface)
public:
    QQmlDMObjectData(
            QQmlDelegateModelItemMetaType *metaType,
            VDMObjectDelegateDataType *dataType,
            int index,
            QObject *object);

    QObject *modelData() const { return object; }
    QObject *proxiedObject() { return object; }

    QPointer<QObject> object;
};

class VDMObjectDelegateDataType : public QQmlRefCount, public QQmlAdaptorModel::Accessors
{
public:
    QMetaObject *metaObject;
    int propertyOffset;
    int signalOffset;
    bool shared;
    QMetaObjectBuilder builder;

    VDMObjectDelegateDataType()
        : metaObject(0)
        , propertyOffset(0)
        , signalOffset(0)
        , shared(true)
    {
    }

    VDMObjectDelegateDataType(const VDMObjectDelegateDataType &type)
        : QQmlRefCount()
        , QQmlAdaptorModel::Accessors()
        , metaObject(0)
        , propertyOffset(type.propertyOffset)
        , signalOffset(type.signalOffset)
        , shared(false)
        , builder(type.metaObject, QMetaObjectBuilder::Properties
                | QMetaObjectBuilder::Signals
                | QMetaObjectBuilder::SuperClass
                | QMetaObjectBuilder::ClassName)
    {
        builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
    }

    ~VDMObjectDelegateDataType()
    {
        free(metaObject);
    }

    int count(const QQmlAdaptorModel &model) const
    {
        return model.list.count();
    }

    QVariant value(const QQmlAdaptorModel &model, int index, const QString &role) const
    {
        if (QObject *object = model.list.at(index).value<QObject *>())
            return object->property(role.toUtf8());
        return QVariant();
    }

    QQmlDelegateModelItem *createItem(
            QQmlAdaptorModel &model,
            QQmlDelegateModelItemMetaType *metaType,
            QQmlEngine *,
            int index) const
    {
        VDMObjectDelegateDataType *dataType = const_cast<VDMObjectDelegateDataType *>(this);
        if (!metaObject)
            dataType->initializeMetaType(model);
        return index >= 0 && index < model.list.count()
                ? new QQmlDMObjectData(metaType, dataType, index, qvariant_cast<QObject *>(model.list.at(index)))
                : 0;
    }

    void initializeMetaType(QQmlAdaptorModel &)
    {
        setModelDataType<QQmlDMObjectData>(&builder, this);

        metaObject = builder.toMetaObject();
    }

    void cleanup(QQmlAdaptorModel &, QQmlDelegateModel *) const
    {
        const_cast<VDMObjectDelegateDataType *>(this)->release();
    }
};

class QQmlDMObjectDataMetaObject : public QAbstractDynamicMetaObject
{
public:
    QQmlDMObjectDataMetaObject(QQmlDMObjectData *data, VDMObjectDelegateDataType *type)
        : m_data(data)
        , m_type(type)
    {
        QObjectPrivate *op = QObjectPrivate::get(m_data);
        *static_cast<QMetaObject *>(this) = *type->metaObject;
        op->metaObject = this;
        m_type->addref();
    }

    ~QQmlDMObjectDataMetaObject()
    {
        m_type->release();
    }

    int metaCall(QMetaObject::Call call, int id, void **arguments)
    {
        static const int objectPropertyOffset = QObject::staticMetaObject.propertyCount();
        if (id >= m_type->propertyOffset
                && (call == QMetaObject::ReadProperty
                || call == QMetaObject::WriteProperty
                || call == QMetaObject::ResetProperty)) {
            if (m_data->object)
                QMetaObject::metacall(m_data->object, call, id - m_type->propertyOffset + objectPropertyOffset, arguments);
            return -1;
        } else if (id >= m_type->signalOffset && call == QMetaObject::InvokeMetaMethod) {
            QMetaObject::activate(m_data, this, id - m_type->signalOffset, 0);
            return -1;
        } else {
            return m_data->qt_metacall(call, id, arguments);
        }
    }

    int createProperty(const char *name, const char *)
    {
        if (!m_data->object)
            return -1;
        const QMetaObject *metaObject = m_data->object->metaObject();
        static const int objectPropertyOffset = QObject::staticMetaObject.propertyCount();

        const int previousPropertyCount = propertyCount() - propertyOffset();
        int propertyIndex = metaObject->indexOfProperty(name);
        if (propertyIndex == -1)
            return -1;
        if (previousPropertyCount + objectPropertyOffset == metaObject->propertyCount())
            return propertyIndex + m_type->propertyOffset - objectPropertyOffset;

        if (m_type->shared) {
            VDMObjectDelegateDataType *type = m_type;
            m_type = new VDMObjectDelegateDataType(*m_type);
            type->release();
        }

        const int previousMethodCount = methodCount();
        int notifierId = previousMethodCount - methodOffset();
        for (int propertyId = previousPropertyCount; propertyId < metaObject->propertyCount() - objectPropertyOffset; ++propertyId) {
            QMetaProperty property = metaObject->property(propertyId + objectPropertyOffset);
            QMetaPropertyBuilder propertyBuilder;
            if (property.hasNotifySignal()) {
                m_type->builder.addSignal("__" + QByteArray::number(propertyId) + "()");
                propertyBuilder = m_type->builder.addProperty(property.name(), property.typeName(), notifierId);
                ++notifierId;
            } else {
                propertyBuilder = m_type->builder.addProperty(property.name(), property.typeName());
            }
            propertyBuilder.setWritable(property.isWritable());
            propertyBuilder.setResettable(property.isResettable());
            propertyBuilder.setConstant(property.isConstant());
        }

        if (m_type->metaObject)
            free(m_type->metaObject);
        m_type->metaObject = m_type->builder.toMetaObject();
        *static_cast<QMetaObject *>(this) = *m_type->metaObject;

        notifierId = previousMethodCount;
        for (int i = previousPropertyCount; i < metaObject->propertyCount() - objectPropertyOffset; ++i) {
            QMetaProperty property = metaObject->property(i + objectPropertyOffset);
            if (property.hasNotifySignal()) {
                QQmlPropertyPrivate::connect(
                        m_data->object, property.notifySignalIndex(), m_data, notifierId);
                ++notifierId;
            }
        }
        return propertyIndex + m_type->propertyOffset - objectPropertyOffset;
    }

    QQmlDMObjectData *m_data;
    VDMObjectDelegateDataType *m_type;
};

QQmlDMObjectData::QQmlDMObjectData(
        QQmlDelegateModelItemMetaType *metaType,
        VDMObjectDelegateDataType *dataType,
        int index,
        QObject *object)
    : QQmlDelegateModelItem(metaType, index)
    , object(object)
{
    new QQmlDMObjectDataMetaObject(this, dataType);
}

//-----------------------------------------------------------------
// QQmlAdaptorModel
//-----------------------------------------------------------------

static const QQmlAdaptorModel::Accessors qt_vdm_null_accessors;
static const VDMListDelegateDataType qt_vdm_list_accessors;

QQmlAdaptorModel::Accessors::~Accessors()
{
}

QQmlAdaptorModel::QQmlAdaptorModel()
    : accessors(&qt_vdm_null_accessors)
{
}

QQmlAdaptorModel::~QQmlAdaptorModel()
{
    accessors->cleanup(*this);
}

void QQmlAdaptorModel::setModel(const QVariant &variant, QQmlDelegateModel *vdm, QQmlEngine *engine)
{
    accessors->cleanup(*this, vdm);

    list.setList(variant, engine);

    if (QObject *object = qvariant_cast<QObject *>(list.list())) {
        setObject(object);
        if (QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(object)) {
            accessors = new VDMAbstractItemModelDataType(this);

            qmlobject_connect(model, QAbstractItemModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
                              vdm, QQmlDelegateModel, SLOT(_q_rowsInserted(QModelIndex,int,int)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                              vdm,  QQmlDelegateModel, SLOT(_q_rowsRemoved(QModelIndex,int,int)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                              vdm,  QQmlDelegateModel, SLOT(_q_rowsAboutToBeRemoved(QModelIndex,int,int)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                              vdm, QQmlDelegateModel, SLOT(_q_dataChanged(QModelIndex,QModelIndex,QVector<int>)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                              vdm, QQmlDelegateModel, SLOT(_q_rowsMoved(QModelIndex,int,int,QModelIndex,int)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(modelReset()),
                              vdm, QQmlDelegateModel, SLOT(_q_modelReset()));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(layoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
                              vdm, QQmlDelegateModel, SLOT(_q_layoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
            qmlobject_connect(model, QAbstractItemModel, SIGNAL(layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
                              vdm, QQmlDelegateModel, SLOT(_q_layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
        } else {
            accessors = new VDMObjectDelegateDataType;
        }
    } else if (list.type() == QQmlListAccessor::ListProperty) {
        setObject(static_cast<const QQmlListReference *>(variant.constData())->object());
        accessors = new VDMObjectDelegateDataType;
    } else if (list.type() != QQmlListAccessor::Invalid
            && list.type() != QQmlListAccessor::Instance) { // Null QObject
        setObject(0);
        accessors = &qt_vdm_list_accessors;
    } else {
        setObject(0);
        accessors = &qt_vdm_null_accessors;
    }
}

void QQmlAdaptorModel::invalidateModel(QQmlDelegateModel *vdm)
{
    accessors->cleanup(*this, vdm);
    accessors = &qt_vdm_null_accessors;
    // Don't clear the model object as we still need the guard to clear the list variant if the
    // object is destroyed.
}

bool QQmlAdaptorModel::isValid() const
{
    return accessors != &qt_vdm_null_accessors;
}

void QQmlAdaptorModel::objectDestroyed(QObject *)
{
    setModel(QVariant(), 0, 0);
}

QQmlAdaptorModelEngineData::QQmlAdaptorModelEngineData(QV8Engine *e)
    : v4(QV8Engine::getV4(e))
{
    QV4::Scope scope(v4);
    QV4::Scoped<QV4::Object> proto(scope, v4->newObject());
    proto->defineAccessorProperty(QStringLiteral("index"), get_index, 0);
    proto->defineAccessorProperty(QStringLiteral("modelData"),
                                  QQmlDMListAccessorData::get_modelData, QQmlDMListAccessorData::set_modelData);
    listItemProto = proto;
}

QQmlAdaptorModelEngineData::~QQmlAdaptorModelEngineData()
{
}

QT_END_NAMESPACE

#include <qqmladaptormodel.moc>
