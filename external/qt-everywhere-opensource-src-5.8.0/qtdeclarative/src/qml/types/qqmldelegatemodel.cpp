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

#include "qqmldelegatemodel_p_p.h"

#include <QtQml/qqmlinfo.h>

#include <private/qquickpackage_p.h>
#include <private/qmetaobjectbuilder_p.h>
#include <private/qqmladaptormodel_p.h>
#include <private/qqmlchangeset_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlincubator_p.h>

#include <private/qv4value_p.h>
#include <private/qv4functionobject_p.h>
#include <qv4objectiterator_p.h>

QT_BEGIN_NAMESPACE

class QQmlDelegateModelItem;

namespace QV4 {

namespace Heap {

struct DelegateModelGroupFunction : FunctionObject {
    void init(QV4::ExecutionContext *scope, uint flag, QV4::ReturnedValue (*code)(QQmlDelegateModelItem *item, uint flag, const QV4::Value &arg));

    QV4::ReturnedValue (*code)(QQmlDelegateModelItem *item, uint flag, const QV4::Value &arg);
    uint flag;
};

struct QQmlDelegateModelGroupChange : Object {
    void init() { Object::init(); }

    QQmlChangeSet::ChangeData change;
};

struct QQmlDelegateModelGroupChangeArray : Object {
    void init(const QVector<QQmlChangeSet::Change> &changes);
    void destroy() {
        delete changes;
        Object::destroy();
    }

    QVector<QQmlChangeSet::Change> *changes;
};


}

struct DelegateModelGroupFunction : QV4::FunctionObject
{
    V4_OBJECT2(DelegateModelGroupFunction, FunctionObject)

    static Heap::DelegateModelGroupFunction *create(QV4::ExecutionContext *scope, uint flag, QV4::ReturnedValue (*code)(QQmlDelegateModelItem *item, uint flag, const QV4::Value &arg))
    {
        return scope->engine()->memoryManager->allocObject<DelegateModelGroupFunction>(scope, flag, code);
    }

    static void call(const QV4::Managed *that, QV4::Scope &scope, QV4::CallData *callData)
    {
        QV4::Scoped<DelegateModelGroupFunction> f(scope, static_cast<const DelegateModelGroupFunction *>(that));
        QV4::Scoped<QQmlDelegateModelItemObject> o(scope, callData->thisObject);
        if (!o) {
            scope.result = scope.engine->throwTypeError(QStringLiteral("Not a valid VisualData object"));
            return;
        }

        QV4::ScopedValue v(scope, callData->argument(0));
        scope.result = f->d()->code(o->d()->item, f->d()->flag, v);
    }
};

void Heap::DelegateModelGroupFunction::init(QV4::ExecutionContext *scope, uint flag, QV4::ReturnedValue (*code)(QQmlDelegateModelItem *item, uint flag, const QV4::Value &arg))
{
    QV4::Heap::FunctionObject::init(scope, QStringLiteral("DelegateModelGroupFunction"));
    this->flag = flag;
    this->code = code;
}

}

DEFINE_OBJECT_VTABLE(QV4::DelegateModelGroupFunction);



class QQmlDelegateModelEngineData : public QV8Engine::Deletable
{
public:
    QQmlDelegateModelEngineData(QV4::ExecutionEngine *v4);
    ~QQmlDelegateModelEngineData();

    QV4::ReturnedValue array(QV8Engine *engine, const QVector<QQmlChangeSet::Change> &changes);

    QV4::PersistentValue changeProto;
};

V4_DEFINE_EXTENSION(QQmlDelegateModelEngineData, engineData)


void QQmlDelegateModelPartsMetaObject::propertyCreated(int, QMetaPropertyBuilder &prop)
{
    prop.setWritable(false);
}

QVariant QQmlDelegateModelPartsMetaObject::initialValue(int id)
{
    QQmlDelegateModelParts *parts = static_cast<QQmlDelegateModelParts *>(object());
    QQmlPartsModel *m = new QQmlPartsModel(
            parts->model, QString::fromUtf8(name(id)), parts);
    parts->models.append(m);
    return QVariant::fromValue(static_cast<QObject *>(m));
}

QQmlDelegateModelParts::QQmlDelegateModelParts(QQmlDelegateModel *parent)
: QObject(parent), model(parent)
{
    new QQmlDelegateModelPartsMetaObject(this);
}

//---------------------------------------------------------------------------

/*!
    \qmltype VisualDataModel
    \instantiates QQmlDelegateModel
    \inqmlmodule QtQuick
    \ingroup qtquick-models
    \brief Encapsulates a model and delegate

    The VisualDataModel type encapsulates a model and the delegate that will
    be instantiated for items in a model.

    This type is provided by the \l{Qt QML} module due to compatibility reasons.
    The same implementation is now primarily available as DelegateModel in the
    \l{Qt QML Models QML Types}{Qt QML Models} module.

    \sa {QtQml.Models::DelegateModel}
*/
/*!
    \qmltype DelegateModel
    \instantiates QQmlDelegateModel
    \inqmlmodule QtQml.Models
    \brief Encapsulates a model and delegate

    The DelegateModel type encapsulates a model and the delegate that will
    be instantiated for items in the model.

    It is usually not necessary to create a DelegateModel.
    However, it can be useful for manipulating and accessing the \l modelIndex
    when a QAbstractItemModel subclass is used as the
    model. Also, DelegateModel is used together with \l Package to
    provide delegates to multiple views, and with DelegateModelGroup to sort and filter
    delegate items.

    The example below illustrates using a DelegateModel with a ListView.

    \snippet delegatemodel/visualdatamodel.qml 0

    \note This type is also available as \l VisualDataModel in the \l{Qt QML}
    module due to compatibility reasons.
*/

QQmlDelegateModelPrivate::QQmlDelegateModelPrivate(QQmlContext *ctxt)
    : m_delegate(0)
    , m_cacheMetaType(0)
    , m_context(ctxt)
    , m_parts(0)
    , m_filterGroup(QStringLiteral("items"))
    , m_count(0)
    , m_groupCount(Compositor::MinimumGroupCount)
    , m_compositorGroup(Compositor::Cache)
    , m_complete(false)
    , m_delegateValidated(false)
    , m_reset(false)
    , m_transaction(false)
    , m_incubatorCleanupScheduled(false)
    , m_waitingToFetchMore(false)
    , m_cacheItems(0)
    , m_items(0)
    , m_persistedItems(0)
{
}

QQmlDelegateModelPrivate::~QQmlDelegateModelPrivate()
{
    qDeleteAll(m_finishedIncubating);

    if (m_cacheMetaType)
        m_cacheMetaType->release();
}

void QQmlDelegateModelPrivate::requestMoreIfNecessary()
{
    Q_Q(QQmlDelegateModel);
    if (!m_waitingToFetchMore && m_adaptorModel.canFetchMore()) {
        m_waitingToFetchMore = true;
        QCoreApplication::postEvent(q, new QEvent(QEvent::UpdateRequest));
    }
}

void QQmlDelegateModelPrivate::init()
{
    Q_Q(QQmlDelegateModel);
    m_compositor.setRemoveGroups(Compositor::GroupMask & ~Compositor::PersistedFlag);

    m_items = new QQmlDelegateModelGroup(QStringLiteral("items"), q, Compositor::Default, q);
    m_items->setDefaultInclude(true);
    m_persistedItems = new QQmlDelegateModelGroup(QStringLiteral("persistedItems"), q, Compositor::Persisted, q);
    QQmlDelegateModelGroupPrivate::get(m_items)->emitters.insert(this);
}

QQmlDelegateModel::QQmlDelegateModel()
    : QQmlDelegateModel(nullptr, nullptr)
{
}

QQmlDelegateModel::QQmlDelegateModel(QQmlContext *ctxt, QObject *parent)
: QQmlInstanceModel(*(new QQmlDelegateModelPrivate(ctxt)), parent)
{
    Q_D(QQmlDelegateModel);
    d->init();
}

QQmlDelegateModel::~QQmlDelegateModel()
{
    Q_D(QQmlDelegateModel);

    foreach (QQmlDelegateModelItem *cacheItem, d->m_cache) {
        if (cacheItem->object) {
            delete cacheItem->object;

            cacheItem->object = 0;
            cacheItem->contextData->destroy();
            cacheItem->contextData = 0;
            cacheItem->scriptRef -= 1;
        }
        cacheItem->groups &= ~Compositor::UnresolvedFlag;
        cacheItem->objectRef = 0;
        if (!cacheItem->isReferenced())
            delete cacheItem;
        else if (cacheItem->incubationTask)
            cacheItem->incubationTask->vdm = 0;
    }
}


void QQmlDelegateModel::classBegin()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_context)
        d->m_context = qmlContext(this);
}

void QQmlDelegateModel::componentComplete()
{
    Q_D(QQmlDelegateModel);
    d->m_complete = true;

    int defaultGroups = 0;
    QStringList groupNames;
    groupNames.append(QStringLiteral("items"));
    groupNames.append(QStringLiteral("persistedItems"));
    if (QQmlDelegateModelGroupPrivate::get(d->m_items)->defaultInclude)
        defaultGroups |= Compositor::DefaultFlag;
    if (QQmlDelegateModelGroupPrivate::get(d->m_persistedItems)->defaultInclude)
        defaultGroups |= Compositor::PersistedFlag;
    for (int i = Compositor::MinimumGroupCount; i < d->m_groupCount; ++i) {
        QString name = d->m_groups[i]->name();
        if (name.isEmpty()) {
            d->m_groups[i] = d->m_groups[d->m_groupCount - 1];
            --d->m_groupCount;
            --i;
        } else if (name.at(0).isUpper()) {
            qmlInfo(d->m_groups[i]) << QQmlDelegateModelGroup::tr("Group names must start with a lower case letter");
            d->m_groups[i] = d->m_groups[d->m_groupCount - 1];
            --d->m_groupCount;
            --i;
        } else {
            groupNames.append(name);

            QQmlDelegateModelGroupPrivate *group = QQmlDelegateModelGroupPrivate::get(d->m_groups[i]);
            group->setModel(this, Compositor::Group(i));
            if (group->defaultInclude)
                defaultGroups |= (1 << i);
        }
    }

    d->m_cacheMetaType = new QQmlDelegateModelItemMetaType(
            QQmlEnginePrivate::getV8Engine(d->m_context->engine()), this, groupNames);

    d->m_compositor.setGroupCount(d->m_groupCount);
    d->m_compositor.setDefaultGroups(defaultGroups);
    d->updateFilterGroup();

    while (!d->m_pendingParts.isEmpty())
        static_cast<QQmlPartsModel *>(d->m_pendingParts.first())->updateFilterGroup();

    QVector<Compositor::Insert> inserts;
    d->m_count = d->m_adaptorModel.count();
    d->m_compositor.append(
            &d->m_adaptorModel,
            0,
            d->m_count,
            defaultGroups | Compositor::AppendFlag | Compositor::PrependFlag,
            &inserts);
    d->itemsInserted(inserts);
    d->emitChanges();
    d->requestMoreIfNecessary();
}

/*!
    \qmlproperty model QtQml.Models::DelegateModel::model
    This property holds the model providing data for the DelegateModel.

    The model provides a set of data that is used to create the items
    for a view.  For large or dynamic datasets the model is usually
    provided by a C++ model object.  The C++ model object must be a \l
    {QAbstractItemModel} subclass or a simple list.

    Models can also be created directly in QML, using a \l{ListModel} or
    \l{XmlListModel}.

    \sa {qml-data-models}{Data Models}
*/
QVariant QQmlDelegateModel::model() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.model();
}

void QQmlDelegateModel::setModel(const QVariant &model)
{
    Q_D(QQmlDelegateModel);

    if (d->m_complete)
        _q_itemsRemoved(0, d->m_count);

    d->m_adaptorModel.setModel(model, this, d->m_context->engine());
    d->m_adaptorModel.replaceWatchedRoles(QList<QByteArray>(), d->m_watchedRoles);
    for (int i = 0; d->m_parts && i < d->m_parts->models.count(); ++i) {
        d->m_adaptorModel.replaceWatchedRoles(
                QList<QByteArray>(), d->m_parts->models.at(i)->watchedRoles());
    }

    if (d->m_complete) {
        _q_itemsInserted(0, d->m_adaptorModel.count());
        d->requestMoreIfNecessary();
    }
}

/*!
    \qmlproperty Component QtQml.Models::DelegateModel::delegate

    The delegate provides a template defining each item instantiated by a view.
    The index is exposed as an accessible \c index property.  Properties of the
    model are also available depending upon the type of \l {qml-data-models}{Data Model}.
*/
QQmlComponent *QQmlDelegateModel::delegate() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_delegate;
}

void QQmlDelegateModel::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQmlDelegateModel);
    if (d->m_transaction) {
        qmlInfo(this) << tr("The delegate of a DelegateModel cannot be changed within onUpdated.");
        return;
    }
    bool wasValid = d->m_delegate != 0;
    d->m_delegate = delegate;
    d->m_delegateValidated = false;
    if (wasValid && d->m_complete) {
        for (int i = 1; i < d->m_groupCount; ++i) {
            QQmlDelegateModelGroupPrivate::get(d->m_groups[i])->changeSet.remove(
                    0, d->m_compositor.count(Compositor::Group(i)));
        }
    }
    if (d->m_complete && d->m_delegate) {
        for (int i = 1; i < d->m_groupCount; ++i) {
            QQmlDelegateModelGroupPrivate::get(d->m_groups[i])->changeSet.insert(
                    0, d->m_compositor.count(Compositor::Group(i)));
        }
    }
    d->emitChanges();
}

/*!
    \qmlproperty QModelIndex QtQml.Models::DelegateModel::rootIndex

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  \c rootIndex allows the children of
    any node in a QAbstractItemModel to be provided by this model.

    This property only affects models of type QAbstractItemModel that
    are hierarchical (e.g, a tree model).

    For example, here is a simple interactive file system browser.
    When a directory name is clicked, the view's \c rootIndex is set to the
    QModelIndex node of the clicked directory, thus updating the view to show
    the new directory's contents.

    \c main.cpp:
    \snippet delegatemodel/visualdatamodel_rootindex/main.cpp 0

    \c view.qml:
    \snippet delegatemodel/visualdatamodel_rootindex/view.qml 0

    If the \l model is a QAbstractItemModel subclass, the delegate can also
    reference a \c hasModelChildren property (optionally qualified by a
    \e model. prefix) that indicates whether the delegate's model item has
    any child nodes.


    \sa modelIndex(), parentModelIndex()
*/
QVariant QQmlDelegateModel::rootIndex() const
{
    Q_D(const QQmlDelegateModel);
    return QVariant::fromValue(QModelIndex(d->m_adaptorModel.rootIndex));
}

void QQmlDelegateModel::setRootIndex(const QVariant &root)
{
    Q_D(QQmlDelegateModel);

    QModelIndex modelIndex = qvariant_cast<QModelIndex>(root);
    const bool changed = d->m_adaptorModel.rootIndex != modelIndex;
    if (changed || !d->m_adaptorModel.isValid()) {
        const int oldCount = d->m_count;
        d->m_adaptorModel.rootIndex = modelIndex;
        if (!d->m_adaptorModel.isValid() && d->m_adaptorModel.aim())  // The previous root index was invalidated, so we need to reconnect the model.
            d->m_adaptorModel.setModel(d->m_adaptorModel.list.list(), this, d->m_context->engine());
        if (d->m_adaptorModel.canFetchMore())
            d->m_adaptorModel.fetchMore();
        if (d->m_complete) {
            const int newCount = d->m_adaptorModel.count();
            if (oldCount)
                _q_itemsRemoved(0, oldCount);
            if (newCount)
                _q_itemsInserted(0, newCount);
        }
        if (changed)
            emit rootIndexChanged();
    }
}

/*!
    \qmlmethod QModelIndex QtQml.Models::DelegateModel::modelIndex(int index)

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  This function assists in using
    tree models in QML.

    Returns a QModelIndex for the specified index.
    This value can be assigned to rootIndex.

    \sa rootIndex
*/
QVariant QQmlDelegateModel::modelIndex(int idx) const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.modelIndex(idx);
}

/*!
    \qmlmethod QModelIndex QtQml.Models::DelegateModel::parentModelIndex()

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  This function assists in using
    tree models in QML.

    Returns a QModelIndex for the parent of the current rootIndex.
    This value can be assigned to rootIndex.

    \sa rootIndex
*/
QVariant QQmlDelegateModel::parentModelIndex() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.parentModelIndex();
}

/*!
    \qmlproperty int QtQml.Models::DelegateModel::count
*/

int QQmlDelegateModel::count() const
{
    Q_D(const QQmlDelegateModel);
    if (!d->m_delegate)
        return 0;
    return d->m_compositor.count(d->m_compositorGroup);
}

QQmlDelegateModel::ReleaseFlags QQmlDelegateModelPrivate::release(QObject *object)
{
    QQmlDelegateModel::ReleaseFlags stat = 0;
    if (!object)
        return stat;

    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(object)) {
        if (cacheItem->releaseObject()) {
            cacheItem->destroyObject();
            emitDestroyingItem(object);
            if (cacheItem->incubationTask) {
                releaseIncubator(cacheItem->incubationTask);
                cacheItem->incubationTask = 0;
            }
            cacheItem->Dispose();
            stat |= QQmlInstanceModel::Destroyed;
        } else {
            stat |= QQmlDelegateModel::Referenced;
        }
    }
    return stat;
}

/*
  Returns ReleaseStatus flags.
*/

QQmlDelegateModel::ReleaseFlags QQmlDelegateModel::release(QObject *item)
{
    Q_D(QQmlDelegateModel);
    QQmlInstanceModel::ReleaseFlags stat = d->release(item);
    return stat;
}

// Cancel a requested async item
void QQmlDelegateModel::cancel(int index)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate || index < 0 || index >= d->m_compositor.count(d->m_compositorGroup)) {
        qWarning() << "DelegateModel::cancel: index out range" << index << d->m_compositor.count(d->m_compositorGroup);
        return;
    }

    Compositor::iterator it = d->m_compositor.find(d->m_compositorGroup, index);
    QQmlDelegateModelItem *cacheItem = it->inCache() ? d->m_cache.at(it.cacheIndex) : 0;
    if (cacheItem) {
        if (cacheItem->incubationTask && !cacheItem->isObjectReferenced()) {
            d->releaseIncubator(cacheItem->incubationTask);
            cacheItem->incubationTask = 0;

            if (cacheItem->object) {
                QObject *object = cacheItem->object;
                cacheItem->destroyObject();
                if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object))
                    d->emitDestroyingPackage(package);
                else
                    d->emitDestroyingItem(object);
            }

            cacheItem->scriptRef -= 1;
        }
        if (!cacheItem->isReferenced()) {
            d->m_compositor.clearFlags(Compositor::Cache, it.cacheIndex, 1, Compositor::CacheFlag);
            d->m_cache.removeAt(it.cacheIndex);
            delete cacheItem;
            Q_ASSERT(d->m_cache.count() == d->m_compositor.count(Compositor::Cache));
        }
    }
}

void QQmlDelegateModelPrivate::group_append(
        QQmlListProperty<QQmlDelegateModelGroup> *property, QQmlDelegateModelGroup *group)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    if (d->m_complete)
        return;
    if (d->m_groupCount == Compositor::MaximumGroupCount) {
        qmlInfo(d->q_func()) << QQmlDelegateModel::tr("The maximum number of supported DelegateModelGroups is 8");
        return;
    }
    d->m_groups[d->m_groupCount] = group;
    d->m_groupCount += 1;
}

int QQmlDelegateModelPrivate::group_count(
        QQmlListProperty<QQmlDelegateModelGroup> *property)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    return d->m_groupCount - 1;
}

QQmlDelegateModelGroup *QQmlDelegateModelPrivate::group_at(
        QQmlListProperty<QQmlDelegateModelGroup> *property, int index)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    return index >= 0 && index < d->m_groupCount - 1
            ? d->m_groups[index + 1]
            : 0;
}

/*!
    \qmlproperty list<DelegateModelGroup> QtQml.Models::DelegateModel::groups

    This property holds a delegate model's group definitions.

    Groups define a sub-set of the items in a delegate model and can be used to filter
    a model.

    For every group defined in a DelegateModel two attached properties are added to each
    delegate item.  The first of the form DelegateModel.in\e{GroupName} holds whether the
    item belongs to the group and the second DelegateModel.\e{groupName}Index holds the
    index of the item in that group.

    The following example illustrates using groups to select items in a model.

    \snippet delegatemodel/visualdatagroup.qml 0
*/

QQmlListProperty<QQmlDelegateModelGroup> QQmlDelegateModel::groups()
{
    Q_D(QQmlDelegateModel);
    return QQmlListProperty<QQmlDelegateModelGroup>(
            this,
            d,
            QQmlDelegateModelPrivate::group_append,
            QQmlDelegateModelPrivate::group_count,
            QQmlDelegateModelPrivate::group_at,
            0);
}

/*!
    \qmlproperty DelegateModelGroup QtQml.Models::DelegateModel::items

    This property holds visual data model's default group to which all new items are added.
*/

QQmlDelegateModelGroup *QQmlDelegateModel::items()
{
    Q_D(QQmlDelegateModel);
    return d->m_items;
}

/*!
    \qmlproperty DelegateModelGroup QtQml.Models::DelegateModel::persistedItems

    This property holds visual data model's persisted items group.

    Items in this group are not destroyed when released by a view, instead they are persisted
    until removed from the group.

    An item can be removed from the persistedItems group by setting the
    DelegateModel.inPersistedItems property to false.  If the item is not referenced by a view
    at that time it will be destroyed.  Adding an item to this group will not create a new
    instance.

    Items returned by the \l QtQml.Models::DelegateModelGroup::create() function are automatically added
    to this group.
*/

QQmlDelegateModelGroup *QQmlDelegateModel::persistedItems()
{
    Q_D(QQmlDelegateModel);
    return d->m_persistedItems;
}

/*!
    \qmlproperty string QtQml.Models::DelegateModel::filterOnGroup

    This property holds the name of the group used to filter the visual data model.

    Only items which belong to this group are visible to a view.

    By default this is the \l items group.
*/

QString QQmlDelegateModel::filterGroup() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_filterGroup;
}

void QQmlDelegateModel::setFilterGroup(const QString &group)
{
    Q_D(QQmlDelegateModel);

    if (d->m_transaction) {
        qmlInfo(this) << tr("The group of a DelegateModel cannot be changed within onChanged");
        return;
    }

    if (d->m_filterGroup != group) {
        d->m_filterGroup = group;
        d->updateFilterGroup();
        emit filterGroupChanged();
    }
}

void QQmlDelegateModel::resetFilterGroup()
{
    setFilterGroup(QStringLiteral("items"));
}

void QQmlDelegateModelPrivate::updateFilterGroup()
{
    Q_Q(QQmlDelegateModel);
    if (!m_cacheMetaType)
        return;

    QQmlListCompositor::Group previousGroup = m_compositorGroup;
    m_compositorGroup = Compositor::Default;
    for (int i = 1; i < m_groupCount; ++i) {
        if (m_filterGroup == m_cacheMetaType->groupNames.at(i - 1)) {
            m_compositorGroup = Compositor::Group(i);
            break;
        }
    }

    QQmlDelegateModelGroupPrivate::get(m_groups[m_compositorGroup])->emitters.insert(this);
    if (m_compositorGroup != previousGroup) {
        QVector<QQmlChangeSet::Change> removes;
        QVector<QQmlChangeSet::Change> inserts;
        m_compositor.transition(previousGroup, m_compositorGroup, &removes, &inserts);

        QQmlChangeSet changeSet;
        changeSet.move(removes, inserts);
        emit q->modelUpdated(changeSet, false);

        if (changeSet.difference() != 0)
            emit q->countChanged();

        if (m_parts) {
            foreach (QQmlPartsModel *model, m_parts->models)
                model->updateFilterGroup(m_compositorGroup, changeSet);
        }
    }
}

/*!
    \qmlproperty object QtQml.Models::DelegateModel::parts

    The \a parts property selects a DelegateModel which creates
    delegates from the part named.  This is used in conjunction with
    the \l Package type.

    For example, the code below selects a model which creates
    delegates named \e list from a \l Package:

    \code
    DelegateModel {
        id: visualModel
        delegate: Package {
            Item { Package.name: "list" }
        }
        model: myModel
    }

    ListView {
        width: 200; height:200
        model: visualModel.parts.list
    }
    \endcode

    \sa Package
*/

QObject *QQmlDelegateModel::parts()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_parts)
        d->m_parts = new QQmlDelegateModelParts(this);
    return d->m_parts;
}

void QQmlDelegateModelPrivate::emitCreatedPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->createdPackage(incubationTask->index[i], package);
}

void QQmlDelegateModelPrivate::emitInitPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->initPackage(incubationTask->index[i], package);
}

void QQmlDelegateModelPrivate::emitDestroyingPackage(QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->destroyingPackage(package);
}

static bool isDoneIncubating(QQmlIncubator::Status status)
{
     return status == QQmlIncubator::Ready || status == QQmlIncubator::Error;
}

void QQDMIncubationTask::statusChanged(Status status)
{
    if (vdm) {
        vdm->incubatorStatusChanged(this, status);
    } else if (isDoneIncubating(status)) {
        Q_ASSERT(incubating);
        // The model was deleted from under our feet, cleanup ourselves
        if (incubating->object) {
            delete incubating->object;

            incubating->object = 0;
            incubating->contextData->destroy();
            incubating->contextData = 0;
        }
        incubating->scriptRef = 0;
        incubating->deleteLater();
    }
}

void QQmlDelegateModelPrivate::releaseIncubator(QQDMIncubationTask *incubationTask)
{
    Q_Q(QQmlDelegateModel);
    if (!incubationTask->isError())
        incubationTask->clear();
    m_finishedIncubating.append(incubationTask);
    if (!m_incubatorCleanupScheduled) {
        m_incubatorCleanupScheduled = true;
        QCoreApplication::postEvent(q, new QEvent(QEvent::User));
    }
}

void QQmlDelegateModelPrivate::removeCacheItem(QQmlDelegateModelItem *cacheItem)
{
    int cidx = m_cache.indexOf(cacheItem);
    if (cidx >= 0) {
        m_compositor.clearFlags(Compositor::Cache, cidx, 1, Compositor::CacheFlag);
        m_cache.removeAt(cidx);
    }
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
}

void QQmlDelegateModelPrivate::incubatorStatusChanged(QQDMIncubationTask *incubationTask, QQmlIncubator::Status status)
{
    Q_Q(QQmlDelegateModel);
    if (!isDoneIncubating(status))
        return;

    QQmlDelegateModelItem *cacheItem = incubationTask->incubating;
    cacheItem->incubationTask = 0;
    incubationTask->incubating = 0;
    releaseIncubator(incubationTask);

    if (status == QQmlIncubator::Ready) {
        cacheItem->referenceObject();
        if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
            emitCreatedPackage(incubationTask, package);
        else
            emitCreatedItem(incubationTask, cacheItem->object);
        cacheItem->releaseObject();
    } else if (status == QQmlIncubator::Error) {
        qmlInfo(q, m_delegate->errors()) << "Error creating delegate";
    }

    if (!cacheItem->isObjectReferenced()) {
        if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
            emitDestroyingPackage(package);
        else
            emitDestroyingItem(cacheItem->object);
        delete cacheItem->object;
        cacheItem->object = 0;
        cacheItem->scriptRef -= 1;
        if (cacheItem->contextData)
            cacheItem->contextData->destroy();
        cacheItem->contextData = 0;

        if (!cacheItem->isReferenced()) {
            removeCacheItem(cacheItem);
            delete cacheItem;
        }
    }
}

void QQDMIncubationTask::setInitialState(QObject *o)
{
    vdm->setInitialState(this, o);
}

void QQmlDelegateModelPrivate::setInitialState(QQDMIncubationTask *incubationTask, QObject *o)
{
    QQmlDelegateModelItem *cacheItem = incubationTask->incubating;
    cacheItem->object = o;

    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
        emitInitPackage(incubationTask, package);
    else
        emitInitItem(incubationTask, cacheItem->object);
}

QObject *QQmlDelegateModelPrivate::object(Compositor::Group group, int index, bool asynchronous)
{
    if (!m_delegate || index < 0 || index >= m_compositor.count(group)) {
        qWarning() << "DelegateModel::item: index out range" << index << m_compositor.count(group);
        return 0;
    } else if (!m_context || !m_context->isValid()) {
        return 0;
    }

    Compositor::iterator it = m_compositor.find(group, index);

    QQmlDelegateModelItem *cacheItem = it->inCache() ? m_cache.at(it.cacheIndex) : 0;

    if (!cacheItem) {
        cacheItem = m_adaptorModel.createItem(m_cacheMetaType, m_context->engine(), it.modelIndex());
        if (!cacheItem)
            return 0;

        cacheItem->groups = it->flags;

        m_cache.insert(it.cacheIndex, cacheItem);
        m_compositor.setFlags(it, 1, Compositor::CacheFlag);
        Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    }

    // Bump the reference counts temporarily so neither the content data or the delegate object
    // are deleted if incubatorStatusChanged() is called synchronously.
    cacheItem->scriptRef += 1;
    cacheItem->referenceObject();

    if (cacheItem->incubationTask) {
        if (!asynchronous && cacheItem->incubationTask->incubationMode() == QQmlIncubator::Asynchronous) {
            // previously requested async - now needed immediately
            cacheItem->incubationTask->forceCompletion();
        }
    } else if (!cacheItem->object) {
        QQmlContext *creationContext = m_delegate->creationContext();

        cacheItem->scriptRef += 1;

        cacheItem->incubationTask = new QQDMIncubationTask(this, asynchronous ? QQmlIncubator::Asynchronous : QQmlIncubator::AsynchronousIfNested);
        cacheItem->incubationTask->incubating = cacheItem;
        cacheItem->incubationTask->clear();

        for (int i = 1; i < m_groupCount; ++i)
            cacheItem->incubationTask->index[i] = it.index[i];

        QQmlContextData *ctxt = new QQmlContextData;
        ctxt->setParent(QQmlContextData::get(creationContext  ? creationContext : m_context.data()));
        ctxt->contextObject = cacheItem;
        cacheItem->contextData = ctxt;

        if (m_adaptorModel.hasProxyObject()) {
            if (QQmlAdaptorModelProxyInterface *proxy
                    = qobject_cast<QQmlAdaptorModelProxyInterface *>(cacheItem)) {
                ctxt = new QQmlContextData;
                ctxt->setParent(cacheItem->contextData, true);
                ctxt->contextObject = proxy->proxiedObject();
            }
        }

        cacheItem->incubateObject(
                    m_delegate,
                    m_context->engine(),
                    ctxt,
                    QQmlContextData::get(m_context));
    }

    if (index == m_compositor.count(group) - 1)
        requestMoreIfNecessary();

    // Remove the temporary reference count.
    cacheItem->scriptRef -= 1;
    if (cacheItem->object && (!cacheItem->incubationTask || isDoneIncubating(cacheItem->incubationTask->status())))
        return cacheItem->object;

    cacheItem->releaseObject();
    if (!cacheItem->isReferenced()) {
        removeCacheItem(cacheItem);
        delete cacheItem;
    }

    return 0;
}

/*
  If asynchronous is true or the component is being loaded asynchronously due
  to an ancestor being loaded asynchronously, item() may return 0.  In this
  case createdItem() will be emitted when the item is available.  The item
  at this stage does not have any references, so item() must be called again
  to ensure a reference is held.  Any call to item() which returns a valid item
  must be matched by a call to release() in order to destroy the item.
*/
QObject *QQmlDelegateModel::object(int index, bool asynchronous)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate || index < 0 || index >= d->m_compositor.count(d->m_compositorGroup)) {
        qWarning() << "DelegateModel::item: index out range" << index << d->m_compositor.count(d->m_compositorGroup);
        return 0;
    }

    QObject *object = d->object(d->m_compositorGroup, index, asynchronous);
    if (!object)
        return 0;

    return object;
}

QString QQmlDelegateModelPrivate::stringValue(Compositor::Group group, int index, const QString &name)
{
    Compositor::iterator it = m_compositor.find(group, index);
    if (QQmlAdaptorModel *model = it.list<QQmlAdaptorModel>()) {
        QString role = name;
        int dot = name.indexOf(QLatin1Char('.'));
        if (dot > 0)
            role = name.left(dot);
        QVariant value = model->value(it.modelIndex(), role);
        while (dot > 0) {
            QObject *obj = qvariant_cast<QObject*>(value);
            if (!obj)
                return QString();
            int from = dot+1;
            dot = name.indexOf(QLatin1Char('.'), from);
            value = obj->property(name.midRef(from, dot - from).toUtf8());
        }
        return value.toString();
    }
    return QString();
}

QString QQmlDelegateModel::stringValue(int index, const QString &name)
{
    Q_D(QQmlDelegateModel);
    return d->stringValue(d->m_compositorGroup, index, name);
}

int QQmlDelegateModel::indexOf(QObject *item, QObject *) const
{
    Q_D(const QQmlDelegateModel);
    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(item))
        return cacheItem->groupIndex(d->m_compositorGroup);
    return -1;
}

void QQmlDelegateModel::setWatchedRoles(const QList<QByteArray> &roles)
{
    Q_D(QQmlDelegateModel);
    d->m_adaptorModel.replaceWatchedRoles(d->m_watchedRoles, roles);
    d->m_watchedRoles = roles;
}

void QQmlDelegateModelPrivate::addGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Insert> inserts;
    m_compositor.setFlags(from, count, group, groupFlags, &inserts);
    itemsInserted(inserts);
    emitChanges();
}

void QQmlDelegateModelPrivate::removeGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Remove> removes;
    m_compositor.clearFlags(from, count, group, groupFlags, &removes);
    itemsRemoved(removes);
    emitChanges();
}

void QQmlDelegateModelPrivate::setGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Remove> removes;
    QVector<Compositor::Insert> inserts;

    m_compositor.setFlags(from, count, group, groupFlags, &inserts);
    itemsInserted(inserts);
    const int removeFlags = ~groupFlags & Compositor::GroupMask;

    from = m_compositor.find(from.group, from.index[from.group]);
    m_compositor.clearFlags(from, count, group, removeFlags, &removes);
    itemsRemoved(removes);
    emitChanges();
}

bool QQmlDelegateModel::event(QEvent *e)
{
    Q_D(QQmlDelegateModel);
    if (e->type() == QEvent::UpdateRequest) {
        d->m_waitingToFetchMore = false;
        d->m_adaptorModel.fetchMore();
    } else if (e->type() == QEvent::User) {
        d->m_incubatorCleanupScheduled = false;
        qDeleteAll(d->m_finishedIncubating);
        d->m_finishedIncubating.clear();
    }
    return QQmlInstanceModel::event(e);
}

void QQmlDelegateModelPrivate::itemsChanged(const QVector<Compositor::Change> &changes)
{
    if (!m_delegate)
        return;

    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedChanges(m_groupCount);

    foreach (const Compositor::Change &change, changes) {
        for (int i = 1; i < m_groupCount; ++i) {
            if (change.inGroup(i)) {
                translatedChanges[i].append(QQmlChangeSet::Change(change.index[i], change.count));
            }
        }
    }

    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->changeSet.change(translatedChanges.at(i));
}

void QQmlDelegateModel::_q_itemsChanged(int index, int count, const QVector<int> &roles)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    if (d->m_adaptorModel.notify(d->m_cache, index, count, roles)) {
        QVector<Compositor::Change> changes;
        d->m_compositor.listItemsChanged(&d->m_adaptorModel, index, count, &changes);
        d->itemsChanged(changes);
        d->emitChanges();
    }
}

static void incrementIndexes(QQmlDelegateModelItem *cacheItem, int count, const int *deltas)
{
    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
        for (int i = 1; i < count; ++i)
            incubationTask->index[i] += deltas[i];
    }
    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
        for (int i = 1; i < qMin<int>(count, Compositor::MaximumGroupCount); ++i)
            attached->m_currentIndex[i] += deltas[i];
    }
}

void QQmlDelegateModelPrivate::itemsInserted(
        const QVector<Compositor::Insert> &inserts,
        QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> *translatedInserts,
        QHash<int, QList<QQmlDelegateModelItem *> > *movedItems)
{
    int cacheIndex = 0;

    int inserted[Compositor::MaximumGroupCount];
    for (int i = 1; i < m_groupCount; ++i)
        inserted[i] = 0;

    foreach (const Compositor::Insert &insert, inserts) {
        for (; cacheIndex < insert.cacheIndex; ++cacheIndex)
            incrementIndexes(m_cache.at(cacheIndex), m_groupCount, inserted);

        for (int i = 1; i < m_groupCount; ++i) {
            if (insert.inGroup(i)) {
                (*translatedInserts)[i].append(
                        QQmlChangeSet::Change(insert.index[i], insert.count, insert.moveId));
                inserted[i] += insert.count;
            }
        }

        if (!insert.inCache())
            continue;

        if (movedItems && insert.isMove()) {
            QList<QQmlDelegateModelItem *> items = movedItems->take(insert.moveId);
            Q_ASSERT(items.count() == insert.count);
            m_cache = m_cache.mid(0, insert.cacheIndex) + items + m_cache.mid(insert.cacheIndex);
        }
        if (insert.inGroup()) {
            for (int offset = 0; cacheIndex < insert.cacheIndex + insert.count; ++cacheIndex, ++offset) {
                QQmlDelegateModelItem *cacheItem = m_cache.at(cacheIndex);
                cacheItem->groups |= insert.flags & Compositor::GroupMask;

                if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                    for (int i = 1; i < m_groupCount; ++i)
                        incubationTask->index[i] = cacheItem->groups & (1 << i)
                                ? insert.index[i] + offset
                                : insert.index[i];
                }
                if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                    for (int i = 1; i < m_groupCount; ++i)
                        attached->m_currentIndex[i] = cacheItem->groups & (1 << i)
                                ? insert.index[i] + offset
                                : insert.index[i];
                }
            }
        } else {
            cacheIndex = insert.cacheIndex + insert.count;
        }
    }
    for (const QList<QQmlDelegateModelItem *> cache = m_cache; cacheIndex < cache.count(); ++cacheIndex)
        incrementIndexes(cache.at(cacheIndex), m_groupCount, inserted);
}

void QQmlDelegateModelPrivate::itemsInserted(const QVector<Compositor::Insert> &inserts)
{
    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedInserts(m_groupCount);
    itemsInserted(inserts, &translatedInserts);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->changeSet.insert(translatedInserts.at(i));
}

void QQmlDelegateModel::_q_itemsInserted(int index, int count)
{

    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    d->m_count += count;

    const QList<QQmlDelegateModelItem *> cache = d->m_cache;
    for (int i = 0, c = cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = cache.at(i);
        if (item->modelIndex() >= index)
            item->setModelIndex(item->modelIndex() + count);
    }

    QVector<Compositor::Insert> inserts;
    d->m_compositor.listItemsInserted(&d->m_adaptorModel, index, count, &inserts);
    d->itemsInserted(inserts);
    d->emitChanges();
}

void QQmlDelegateModelPrivate::itemsRemoved(
        const QVector<Compositor::Remove> &removes,
        QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> *translatedRemoves,
        QHash<int, QList<QQmlDelegateModelItem *> > *movedItems)
{
    int cacheIndex = 0;
    int removedCache = 0;

    int removed[Compositor::MaximumGroupCount];
    for (int i = 1; i < m_groupCount; ++i)
        removed[i] = 0;

    foreach (const Compositor::Remove &remove, removes) {
        for (; cacheIndex < remove.cacheIndex; ++cacheIndex)
            incrementIndexes(m_cache.at(cacheIndex), m_groupCount, removed);

        for (int i = 1; i < m_groupCount; ++i) {
            if (remove.inGroup(i)) {
                (*translatedRemoves)[i].append(
                        QQmlChangeSet::Change(remove.index[i], remove.count, remove.moveId));
                removed[i] -= remove.count;
            }
        }

        if (!remove.inCache())
            continue;

        if (movedItems && remove.isMove()) {
            movedItems->insert(remove.moveId, m_cache.mid(remove.cacheIndex, remove.count));
            QList<QQmlDelegateModelItem *>::iterator begin = m_cache.begin() + remove.cacheIndex;
            QList<QQmlDelegateModelItem *>::iterator end = begin + remove.count;
            m_cache.erase(begin, end);
        } else {
            for (; cacheIndex < remove.cacheIndex + remove.count - removedCache; ++cacheIndex) {
                QQmlDelegateModelItem *cacheItem = m_cache.at(cacheIndex);
                if (remove.inGroup(Compositor::Persisted) && cacheItem->objectRef == 0 && cacheItem->object) {
                    QObject *object = cacheItem->object;
                    cacheItem->destroyObject();
                    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object))
                        emitDestroyingPackage(package);
                    else
                        emitDestroyingItem(object);
                    cacheItem->scriptRef -= 1;
                }
                if (!cacheItem->isReferenced()) {
                    m_compositor.clearFlags(Compositor::Cache, cacheIndex, 1, Compositor::CacheFlag);
                    m_cache.removeAt(cacheIndex);
                    delete cacheItem;
                    --cacheIndex;
                    ++removedCache;
                    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
                } else if (remove.groups() == cacheItem->groups) {
                    cacheItem->groups = 0;
                    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                        for (int i = 1; i < m_groupCount; ++i)
                            incubationTask->index[i] = -1;
                    }
                    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                        for (int i = 1; i < m_groupCount; ++i)
                            attached->m_currentIndex[i] = -1;
                    }
                } else {
                    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                        if (!cacheItem->isObjectReferenced()) {
                            releaseIncubator(cacheItem->incubationTask);
                            cacheItem->incubationTask = 0;
                            if (cacheItem->object) {
                                QObject *object = cacheItem->object;
                                cacheItem->destroyObject();
                                if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object))
                                    emitDestroyingPackage(package);
                                else
                                    emitDestroyingItem(object);
                            }
                            cacheItem->scriptRef -= 1;
                        } else {
                            for (int i = 1; i < m_groupCount; ++i) {
                                if (remove.inGroup(i))
                                    incubationTask->index[i] = remove.index[i];
                            }
                        }
                    }
                    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                        for (int i = 1; i < m_groupCount; ++i) {
                            if (remove.inGroup(i))
                                attached->m_currentIndex[i] = remove.index[i];
                        }
                    }
                    cacheItem->groups &= ~remove.flags;
                }
            }
        }
    }

    for (const QList<QQmlDelegateModelItem *> cache = m_cache; cacheIndex < cache.count(); ++cacheIndex)
        incrementIndexes(cache.at(cacheIndex), m_groupCount, removed);
}

void QQmlDelegateModelPrivate::itemsRemoved(const QVector<Compositor::Remove> &removes)
{
    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedRemoves(m_groupCount);
    itemsRemoved(removes, &translatedRemoves);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i)
       QQmlDelegateModelGroupPrivate::get(m_groups[i])->changeSet.remove(translatedRemoves.at(i));
}

void QQmlDelegateModel::_q_itemsRemoved(int index, int count)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0|| !d->m_complete)
        return;

    d->m_count -= count;
    const QList<QQmlDelegateModelItem *> cache = d->m_cache;
    for (int i = 0, c = cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = cache.at(i);
        // layout change triggered by removal of a previous item might have
        // already invalidated this item in d->m_cache and deleted it
        if (!d->m_cache.contains(item))
            continue;

        if (item->modelIndex() >= index + count)
            item->setModelIndex(item->modelIndex() - count);
        else  if (item->modelIndex() >= index)
            item->setModelIndex(-1);
    }

    QVector<Compositor::Remove> removes;
    d->m_compositor.listItemsRemoved(&d->m_adaptorModel, index, count, &removes);
    d->itemsRemoved(removes);

    d->emitChanges();
}

void QQmlDelegateModelPrivate::itemsMoved(
        const QVector<Compositor::Remove> &removes, const QVector<Compositor::Insert> &inserts)
{
    QHash<int, QList<QQmlDelegateModelItem *> > movedItems;

    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedRemoves(m_groupCount);
    itemsRemoved(removes, &translatedRemoves, &movedItems);

    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedInserts(m_groupCount);
    itemsInserted(inserts, &translatedInserts, &movedItems);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    Q_ASSERT(movedItems.isEmpty());
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i) {
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->changeSet.move(
                    translatedRemoves.at(i),
                    translatedInserts.at(i));
    }
}

void QQmlDelegateModel::_q_itemsMoved(int from, int to, int count)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    const int minimum = qMin(from, to);
    const int maximum = qMax(from, to) + count;
    const int difference = from > to ? count : -count;

    const QList<QQmlDelegateModelItem *> cache = d->m_cache;
    for (int i = 0, c = cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = cache.at(i);
        if (item->modelIndex() >= from && item->modelIndex() < from + count)
            item->setModelIndex(item->modelIndex() - from + to);
        else if (item->modelIndex() >= minimum && item->modelIndex() < maximum)
            item->setModelIndex(item->modelIndex() + difference);
    }

    QVector<Compositor::Remove> removes;
    QVector<Compositor::Insert> inserts;
    d->m_compositor.listItemsMoved(&d->m_adaptorModel, from, to, count, &removes, &inserts);
    d->itemsMoved(removes, inserts);
    d->emitChanges();
}

void QQmlDelegateModelPrivate::emitModelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_Q(QQmlDelegateModel);
    emit q->modelUpdated(changeSet, reset);
    if (changeSet.difference() != 0)
        emit q->countChanged();
}

void QQmlDelegateModelPrivate::emitChanges()
{
    if (m_transaction || !m_complete || !m_context || !m_context->isValid())
        return;

    m_transaction = true;
    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(m_context->engine());
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->emitChanges(engine);
    m_transaction = false;

    const bool reset = m_reset;
    m_reset = false;
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDelegateModelGroupPrivate::get(m_groups[i])->emitModelUpdated(reset);

    foreach (QQmlDelegateModelItem *cacheItem, m_cache) {
        if (cacheItem->attached)
            cacheItem->attached->emitChanges();
    }
}

void QQmlDelegateModel::_q_modelReset()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate)
        return;

    int oldCount = d->m_count;
    d->m_adaptorModel.rootIndex = QModelIndex();

    if (d->m_complete) {
        d->m_count = d->m_adaptorModel.count();

        const QList<QQmlDelegateModelItem *> cache = d->m_cache;
        for (int i = 0, c = cache.count();  i < c; ++i) {
            QQmlDelegateModelItem *item = cache.at(i);
            if (item->modelIndex() != -1)
                item->setModelIndex(-1);
        }

        QVector<Compositor::Remove> removes;
        QVector<Compositor::Insert> inserts;
        if (oldCount)
            d->m_compositor.listItemsRemoved(&d->m_adaptorModel, 0, oldCount, &removes);
        if (d->m_count)
            d->m_compositor.listItemsInserted(&d->m_adaptorModel, 0, d->m_count, &inserts);
        d->itemsMoved(removes, inserts);
        d->m_reset = true;

        if (d->m_adaptorModel.canFetchMore())
            d->m_adaptorModel.fetchMore();

        d->emitChanges();
    }
    emit rootIndexChanged();
}

void QQmlDelegateModel::_q_rowsInserted(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (parent == d->m_adaptorModel.rootIndex)
        _q_itemsInserted(begin, end - begin + 1);
}

void QQmlDelegateModel::_q_rowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_adaptorModel.rootIndex.isValid())
        return;
    const QModelIndex index = d->m_adaptorModel.rootIndex;
    if (index.parent() == parent && index.row() >= begin && index.row() <= end) {
        const int oldCount = d->m_count;
        d->m_count = 0;
        d->m_adaptorModel.invalidateModel(this);

        if (d->m_complete && oldCount > 0) {
            QVector<Compositor::Remove> removes;
            d->m_compositor.listItemsRemoved(&d->m_adaptorModel, 0, oldCount, &removes);
            d->itemsRemoved(removes);
            d->emitChanges();
        }
    }
}

void QQmlDelegateModel::_q_rowsRemoved(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (parent == d->m_adaptorModel.rootIndex)
        _q_itemsRemoved(begin, end - begin + 1);
}

void QQmlDelegateModel::_q_rowsMoved(
        const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
        const QModelIndex &destinationParent, int destinationRow)
{
   Q_D(QQmlDelegateModel);
    const int count = sourceEnd - sourceStart + 1;
    if (destinationParent == d->m_adaptorModel.rootIndex && sourceParent == d->m_adaptorModel.rootIndex) {
        _q_itemsMoved(sourceStart, sourceStart > destinationRow ? destinationRow : destinationRow - count, count);
    } else if (sourceParent == d->m_adaptorModel.rootIndex) {
        _q_itemsRemoved(sourceStart, count);
    } else if (destinationParent == d->m_adaptorModel.rootIndex) {
        _q_itemsInserted(destinationRow, count);
    }
}

void QQmlDelegateModel::_q_dataChanged(const QModelIndex &begin, const QModelIndex &end, const QVector<int> &roles)
{
    Q_D(QQmlDelegateModel);
    if (begin.parent() == d->m_adaptorModel.rootIndex)
        _q_itemsChanged(begin.row(), end.row() - begin.row() + 1, roles);
}

bool QQmlDelegateModel::isDescendantOf(const QPersistentModelIndex& desc, const QList< QPersistentModelIndex >& parents) const
{
    for (int i = 0, c = parents.count(); i < c; ++i) {
        for (QPersistentModelIndex parent = desc; parent.isValid(); parent = parent.parent()) {
            if (parent == parents[i])
                return true;
        }
    }

    return false;
}

void QQmlDelegateModel::_q_layoutChanged(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_complete)
        return;

    if (hint == QAbstractItemModel::VerticalSortHint) {
        if (!parents.isEmpty() && d->m_adaptorModel.rootIndex.isValid() && !isDescendantOf(d->m_adaptorModel.rootIndex, parents)) {
            return;
        }

        // mark all items as changed
        _q_itemsChanged(0, d->m_count, QVector<int>());

    } else if (hint == QAbstractItemModel::HorizontalSortHint) {
        // Ignored
    } else {
        // We don't know what's going on, so reset the model
        _q_modelReset();
    }
}

QQmlDelegateModelAttached *QQmlDelegateModel::qmlAttachedProperties(QObject *obj)
{
    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(obj)) {
        if (cacheItem->object == obj) { // Don't create attached item for child objects.
            cacheItem->attached = new QQmlDelegateModelAttached(cacheItem, obj);
            return cacheItem->attached;
        }
    }
    return new QQmlDelegateModelAttached(obj);
}

bool QQmlDelegateModelPrivate::insert(Compositor::insert_iterator &before, const QV4::Value &object, int groups)
{
    if (!m_context || !m_context->isValid())
        return false;

    QQmlDelegateModelItem *cacheItem = m_adaptorModel.createItem(m_cacheMetaType, m_context->engine(), -1);
    if (!cacheItem)
        return false;
    if (!object.isObject())
        return false;

    QV4::ExecutionEngine *v4 = object.as<QV4::Object>()->engine();
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, object);
    if (!o)
        return false;

    QV4::ObjectIterator it(scope, o, QV4::ObjectIterator::EnumerableOnly|QV4::ObjectIterator::WithProtoChain);
    QV4::ScopedValue propertyName(scope);
    QV4::ScopedValue v(scope);
    while (1) {
        propertyName = it.nextPropertyNameAsString(v);
        if (propertyName->isNull())
            break;
        cacheItem->setValue(propertyName->toQStringNoThrow(), scope.engine->toVariant(v, QVariant::Invalid));
    }

    cacheItem->groups = groups | Compositor::UnresolvedFlag | Compositor::CacheFlag;

    // Must be before the new object is inserted into the cache or its indexes will be adjusted too.
    itemsInserted(QVector<Compositor::Insert>() << Compositor::Insert(before, 1, cacheItem->groups & ~Compositor::CacheFlag));

    before = m_compositor.insert(before, 0, 0, 1, cacheItem->groups);
    m_cache.insert(before.cacheIndex, cacheItem);

    return true;
}

//============================================================================

QQmlDelegateModelItemMetaType::QQmlDelegateModelItemMetaType(
        QV8Engine *engine, QQmlDelegateModel *model, const QStringList &groupNames)
    : model(model)
    , groupCount(groupNames.count() + 1)
    , v8Engine(engine)
    , metaObject(0)
    , groupNames(groupNames)
{
}

QQmlDelegateModelItemMetaType::~QQmlDelegateModelItemMetaType()
{
    if (metaObject)
        metaObject->release();
}

void QQmlDelegateModelItemMetaType::initializeMetaObject()
{
    QMetaObjectBuilder builder;
    builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
    builder.setClassName(QQmlDelegateModelAttached::staticMetaObject.className());
    builder.setSuperClass(&QQmlDelegateModelAttached::staticMetaObject);

    int notifierId = 0;
    for (int i = 0; i < groupNames.count(); ++i, ++notifierId) {
        QString propertyName = QLatin1String("in") + groupNames.at(i);
        propertyName.replace(2, 1, propertyName.at(2).toUpper());
        builder.addSignal("__" + propertyName.toUtf8() + "Changed()");
        QMetaPropertyBuilder propertyBuilder = builder.addProperty(
                propertyName.toUtf8(), "bool", notifierId);
        propertyBuilder.setWritable(true);
    }
    for (int i = 0; i < groupNames.count(); ++i, ++notifierId) {
        const QString propertyName = groupNames.at(i) + QLatin1String("Index");
        builder.addSignal("__" + propertyName.toUtf8() + "Changed()");
        QMetaPropertyBuilder propertyBuilder = builder.addProperty(
                propertyName.toUtf8(), "int", notifierId);
        propertyBuilder.setWritable(true);
    }

    metaObject = new QQmlDelegateModelAttachedMetaObject(this, builder.toMetaObject());
}

void QQmlDelegateModelItemMetaType::initializePrototype()
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8Engine);
    QV4::Scope scope(v4);

    QV4::ScopedObject proto(scope, v4->newObject());
    proto->defineAccessorProperty(QStringLiteral("model"), QQmlDelegateModelItem::get_model, 0);
    proto->defineAccessorProperty(QStringLiteral("groups"), QQmlDelegateModelItem::get_groups, QQmlDelegateModelItem::set_groups);
    QV4::ScopedString s(scope);
    QV4::ScopedProperty p(scope);

    s = v4->newString(QStringLiteral("isUnresolved"));
    QV4::ScopedFunctionObject f(scope);
    QV4::ExecutionContext *global = scope.engine->rootContext();
    p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, 30, QQmlDelegateModelItem::get_member)));
    p->setSetter(0);
    proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);

    s = v4->newString(QStringLiteral("inItems"));
    p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Default, QQmlDelegateModelItem::get_member)));
    p->setSetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Default, QQmlDelegateModelItem::set_member)));
    proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);

    s = v4->newString(QStringLiteral("inPersistedItems"));
    p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Persisted, QQmlDelegateModelItem::get_member)));
    p->setSetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Persisted, QQmlDelegateModelItem::set_member)));
    proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);

    s = v4->newString(QStringLiteral("itemsIndex"));
    p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Default, QQmlDelegateModelItem::get_index)));
    proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);

    s = v4->newString(QStringLiteral("persistedItemsIndex"));
    p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, QQmlListCompositor::Persisted, QQmlDelegateModelItem::get_index)));
    p->setSetter(0);
    proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);

    for (int i = 2; i < groupNames.count(); ++i) {
        QString propertyName = QLatin1String("in") + groupNames.at(i);
        propertyName.replace(2, 1, propertyName.at(2).toUpper());
        s = v4->newString(propertyName);
        p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, i + 1, QQmlDelegateModelItem::get_member)));
        p->setSetter((f = QV4::DelegateModelGroupFunction::create(global, i + 1, QQmlDelegateModelItem::set_member)));
        proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);
    }
    for (int i = 2; i < groupNames.count(); ++i) {
        const QString propertyName = groupNames.at(i) + QLatin1String("Index");
        s = v4->newString(propertyName);
        p->setGetter((f = QV4::DelegateModelGroupFunction::create(global, i + 1, QQmlDelegateModelItem::get_index)));
        p->setSetter(0);
        proto->insertMember(s, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);
    }
    modelItemProto.set(v4, proto);
}

int QQmlDelegateModelItemMetaType::parseGroups(const QStringList &groups) const
{
    int groupFlags = 0;
    foreach (const QString &groupName, groups) {
        int index = groupNames.indexOf(groupName);
        if (index != -1)
            groupFlags |= 2 << index;
    }
    return groupFlags;
}

int QQmlDelegateModelItemMetaType::parseGroups(const QV4::Value &groups) const
{
    int groupFlags = 0;
    QV4::Scope scope(QV8Engine::getV4(v8Engine));

    QV4::ScopedString s(scope, groups);
    if (s) {
        const QString groupName = s->toQString();
        int index = groupNames.indexOf(groupName);
        if (index != -1)
            groupFlags |= 2 << index;
        return groupFlags;
    }

    QV4::ScopedArrayObject array(scope, groups);
    if (array) {
        QV4::ScopedValue v(scope);
        uint arrayLength = array->getLength();
        for (uint i = 0; i < arrayLength; ++i) {
            v = array->getIndexed(i);
            const QString groupName = v->toQString();
            int index = groupNames.indexOf(groupName);
            if (index != -1)
                groupFlags |= 2 << index;
        }
    }
    return groupFlags;
}

QV4::ReturnedValue QQmlDelegateModelItem::get_model(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->thisObject().as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->engine()->throwTypeError(QStringLiteral("Not a valid VisualData object"));
    if (!o->d()->item->metaType->model)
        return QV4::Encode::undefined();

    return o->d()->item->get();
}

QV4::ReturnedValue QQmlDelegateModelItem::get_groups(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->thisObject().as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->engine()->throwTypeError(QStringLiteral("Not a valid VisualData object"));

    QStringList groups;
    for (int i = 1; i < o->d()->item->metaType->groupCount; ++i) {
        if (o->d()->item->groups & (1 << i))
            groups.append(o->d()->item->metaType->groupNames.at(i - 1));
    }

    return scope.engine->fromVariant(groups);
}

QV4::ReturnedValue QQmlDelegateModelItem::set_groups(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlDelegateModelItemObject> o(scope, ctx->thisObject().as<QQmlDelegateModelItemObject>());
    if (!o)
        return ctx->engine()->throwTypeError(QStringLiteral("Not a valid VisualData object"));
    if (!ctx->argc())
        return ctx->engine()->throwTypeError();

    if (!o->d()->item->metaType->model)
        return QV4::Encode::undefined();
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(o->d()->item->metaType->model);

    const int groupFlags = model->m_cacheMetaType->parseGroups(ctx->args()[0]);
    const int cacheIndex = model->m_cache.indexOf(o->d()->item);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    model->setGroups(it, 1, Compositor::Cache, groupFlags);
    return QV4::Encode::undefined();
}

QV4::ReturnedValue QQmlDelegateModelItem::get_member(QQmlDelegateModelItem *thisItem, uint flag, const QV4::Value &)
{
    return QV4::Encode(bool(thisItem->groups & (1 << flag)));
}

QV4::ReturnedValue QQmlDelegateModelItem::set_member(QQmlDelegateModelItem *cacheItem, uint flag, const QV4::Value &arg)
{
    if (!cacheItem->metaType->model)
        return QV4::Encode::undefined();

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(cacheItem->metaType->model);

    bool member = arg.toBoolean();
    uint groupFlag = (1 << flag);
    if (member == ((cacheItem->groups & groupFlag) != 0))
        return QV4::Encode::undefined();

    const int cacheIndex = model->m_cache.indexOf(cacheItem);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    if (member)
        model->addGroups(it, 1, Compositor::Cache, groupFlag);
    else
        model->removeGroups(it, 1, Compositor::Cache, groupFlag);
    return QV4::Encode::undefined();
}

QV4::ReturnedValue QQmlDelegateModelItem::get_index(QQmlDelegateModelItem *thisItem, uint flag, const QV4::Value &)
{
    return QV4::Encode((int)thisItem->groupIndex(Compositor::Group(flag)));
}


//---------------------------------------------------------------------------

DEFINE_OBJECT_VTABLE(QQmlDelegateModelItemObject);

void QV4::Heap::QQmlDelegateModelItemObject::destroy()
{
    item->Dispose();
    Object::destroy();
}


QQmlDelegateModelItem::QQmlDelegateModelItem(
        QQmlDelegateModelItemMetaType *metaType, int modelIndex)
    : v4(QV8Engine::getV4(metaType->v8Engine))
    , metaType(metaType)
    , contextData(0)
    , object(0)
    , attached(0)
    , incubationTask(0)
    , objectRef(0)
    , scriptRef(0)
    , groups(0)
    , index(modelIndex)
{
    metaType->addref();
}

QQmlDelegateModelItem::~QQmlDelegateModelItem()
{
    Q_ASSERT(scriptRef == 0);
    Q_ASSERT(objectRef == 0);
    Q_ASSERT(!object);

    if (incubationTask) {
        if (metaType->model)
            QQmlDelegateModelPrivate::get(metaType->model)->releaseIncubator(incubationTask);
        else
            delete incubationTask;
    }

    metaType->release();

}

void QQmlDelegateModelItem::Dispose()
{
    --scriptRef;
    if (isReferenced())
        return;

    if (metaType->model) {
        QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(metaType->model);
        model->removeCacheItem(this);
    }
    delete this;
}

/*
    This is essentially a copy of QQmlComponent::create(); except it takes the QQmlContextData
    arguments instead of QQmlContext which means we don't have to construct the rather weighty
    wrapper class for every delegate item.
*/
void QQmlDelegateModelItem::incubateObject(
        QQmlComponent *component,
        QQmlEngine *engine,
        QQmlContextData *context,
        QQmlContextData *forContext)
{
    QQmlIncubatorPrivate *incubatorPriv = QQmlIncubatorPrivate::get(incubationTask);
    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(engine);
    QQmlComponentPrivate *componentPriv = QQmlComponentPrivate::get(component);

    incubatorPriv->compilationUnit = componentPriv->compilationUnit;
    incubatorPriv->compilationUnit->addref();
    incubatorPriv->enginePriv = enginePriv;
    incubatorPriv->creator.reset(new QQmlObjectCreator(context, componentPriv->compilationUnit, componentPriv->creationContext));
    incubatorPriv->subComponentToCreate = componentPriv->start;

    enginePriv->incubate(*incubationTask, forContext);
}

void QQmlDelegateModelItem::destroyObject()
{
    Q_ASSERT(object);
    Q_ASSERT(contextData);

    QObjectPrivate *p = QObjectPrivate::get(object);
    Q_ASSERT(p->declarativeData);
    QQmlData *data = static_cast<QQmlData*>(p->declarativeData);
    if (data->ownContext && data->context)
        data->context->clearContext();
    object->deleteLater();

    if (attached) {
        attached->m_cacheItem = 0;
        attached = 0;
    }

    contextData->destroy();
    contextData = 0;
    object = 0;
}

QQmlDelegateModelItem *QQmlDelegateModelItem::dataForObject(QObject *object)
{
    QObjectPrivate *p = QObjectPrivate::get(object);
    QQmlContextData *context = p->declarativeData
            ? static_cast<QQmlData *>(p->declarativeData)->context
            : 0;
    for (context = context ? context->parent : 0; context; context = context->parent) {
        if (QQmlDelegateModelItem *cacheItem = qobject_cast<QQmlDelegateModelItem *>(
                context->contextObject)) {
            return cacheItem;
        }
    }
    return 0;
}

int QQmlDelegateModelItem::groupIndex(Compositor::Group group)
{
    if (QQmlDelegateModelPrivate * const model = metaType->model
            ? QQmlDelegateModelPrivate::get(metaType->model)
            : 0) {
        return model->m_compositor.find(Compositor::Cache, model->m_cache.indexOf(this)).index[group];
    }
    return -1;
}

//---------------------------------------------------------------------------

QQmlDelegateModelAttachedMetaObject::QQmlDelegateModelAttachedMetaObject(
        QQmlDelegateModelItemMetaType *metaType, QMetaObject *metaObject)
    : metaType(metaType)
    , metaObject(metaObject)
    , memberPropertyOffset(QQmlDelegateModelAttached::staticMetaObject.propertyCount())
    , indexPropertyOffset(QQmlDelegateModelAttached::staticMetaObject.propertyCount() + metaType->groupNames.count())
{
    // Don't reference count the meta-type here as that would create a circular reference.
    // Instead we rely the fact that the meta-type's reference count can't reach 0 without first
    // destroying all delegates with attached objects.
    *static_cast<QMetaObject *>(this) = *metaObject;
}

QQmlDelegateModelAttachedMetaObject::~QQmlDelegateModelAttachedMetaObject()
{
    ::free(metaObject);
}

void QQmlDelegateModelAttachedMetaObject::objectDestroyed(QObject *)
{
    release();
}

int QQmlDelegateModelAttachedMetaObject::metaCall(QObject *object, QMetaObject::Call call, int _id, void **arguments)
{
    QQmlDelegateModelAttached *attached = static_cast<QQmlDelegateModelAttached *>(object);
    if (call == QMetaObject::ReadProperty) {
        if (_id >= indexPropertyOffset) {
            Compositor::Group group = Compositor::Group(_id - indexPropertyOffset + 1);
            *static_cast<int *>(arguments[0]) = attached->m_currentIndex[group];
            return -1;
        } else if (_id >= memberPropertyOffset) {
            Compositor::Group group = Compositor::Group(_id - memberPropertyOffset + 1);
            *static_cast<bool *>(arguments[0]) = attached->m_cacheItem->groups & (1 << group);
            return -1;
        }
    } else if (call == QMetaObject::WriteProperty) {
        if (_id >= memberPropertyOffset) {
            if (!metaType->model)
                return -1;
            QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(metaType->model);
            Compositor::Group group = Compositor::Group(_id - memberPropertyOffset + 1);
            const int groupFlag = 1 << group;
            const bool member = attached->m_cacheItem->groups & groupFlag;
            if (member && !*static_cast<bool *>(arguments[0])) {
                Compositor::iterator it = model->m_compositor.find(
                        group, attached->m_currentIndex[group]);
                model->removeGroups(it, 1, group, groupFlag);
            } else if (!member && *static_cast<bool *>(arguments[0])) {
                for (int i = 1; i < metaType->groupCount; ++i) {
                    if (attached->m_cacheItem->groups & (1 << i)) {
                        Compositor::iterator it = model->m_compositor.find(
                                Compositor::Group(i), attached->m_currentIndex[i]);
                        model->addGroups(it, 1, Compositor::Group(i), groupFlag);
                        break;
                    }
                }
            }
            return -1;
        }
    }
    return attached->qt_metacall(call, _id, arguments);
}

QQmlDelegateModelAttached::QQmlDelegateModelAttached(QObject *parent)
    : m_cacheItem(0)
    , m_previousGroups(0)
{
    QQml_setParent_noEvent(this, parent);
}

QQmlDelegateModelAttached::QQmlDelegateModelAttached(
        QQmlDelegateModelItem *cacheItem, QObject *parent)
    : m_cacheItem(cacheItem)
    , m_previousGroups(cacheItem->groups)
{
    QQml_setParent_noEvent(this, parent);
    if (QQDMIncubationTask *incubationTask = m_cacheItem->incubationTask) {
        for (int i = 1; i < qMin<int>(m_cacheItem->metaType->groupCount, Compositor::MaximumGroupCount); ++i)
            m_currentIndex[i] = m_previousIndex[i] = incubationTask->index[i];
    } else {
        QQmlDelegateModelPrivate * const model = QQmlDelegateModelPrivate::get(m_cacheItem->metaType->model);
        Compositor::iterator it = model->m_compositor.find(
                Compositor::Cache, model->m_cache.indexOf(m_cacheItem));
        for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i)
            m_currentIndex[i] = m_previousIndex[i] = it.index[i];
    }

    if (!cacheItem->metaType->metaObject)
        cacheItem->metaType->initializeMetaObject();

    QObjectPrivate::get(this)->metaObject = cacheItem->metaType->metaObject;
    cacheItem->metaType->metaObject->addref();
}

/*!
    \qmlattachedproperty int QtQml.Models::DelegateModel::model

    This attached property holds the visual data model this delegate instance belongs to.

    It is attached to each instance of the delegate.
*/

QQmlDelegateModel *QQmlDelegateModelAttached::model() const
{
    return m_cacheItem ? m_cacheItem->metaType->model : 0;
}

/*!
    \qmlattachedproperty stringlist QtQml.Models::DelegateModel::groups

    This attached property holds the name of DelegateModelGroups the item belongs to.

    It is attached to each instance of the delegate.
*/

QStringList QQmlDelegateModelAttached::groups() const
{
    QStringList groups;

    if (!m_cacheItem)
        return groups;
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i) {
        if (m_cacheItem->groups & (1 << i))
            groups.append(m_cacheItem->metaType->groupNames.at(i - 1));
    }
    return groups;
}

void QQmlDelegateModelAttached::setGroups(const QStringList &groups)
{
    if (!m_cacheItem)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_cacheItem->metaType->model);

    const int groupFlags = model->m_cacheMetaType->parseGroups(groups);
    const int cacheIndex = model->m_cache.indexOf(m_cacheItem);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    model->setGroups(it, 1, Compositor::Cache, groupFlags);
}

/*!
    \qmlattachedproperty bool QtQml.Models::DelegateModel::isUnresolved

    This attached property holds whether the visual item is bound to a data model index.
    Returns true if the item is not bound to the model, and false if it is.

    An unresolved item can be bound to the data model using the DelegateModelGroup::resolve()
    function.

    It is attached to each instance of the delegate.
*/

bool QQmlDelegateModelAttached::isUnresolved() const
{
    if (!m_cacheItem)
        return false;

    return m_cacheItem->groups & Compositor::UnresolvedFlag;
}

/*!
    \qmlattachedproperty int QtQml.Models::DelegateModel::inItems

    This attached property holds whether the item belongs to the default \l items DelegateModelGroup.

    Changing this property will add or remove the item from the items group.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQml.Models::DelegateModel::itemsIndex

    This attached property holds the index of the item in the default \l items DelegateModelGroup.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQml.Models::DelegateModel::inPersistedItems

    This attached property holds whether the item belongs to the \l persistedItems DelegateModelGroup.

    Changing this property will add or remove the item from the items group.  Change with caution
    as removing an item from the persistedItems group will destroy the current instance if it is
    not referenced by a model.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQml.Models::DelegateModel::persistedItemsIndex

    This attached property holds the index of the item in the \l persistedItems DelegateModelGroup.

    It is attached to each instance of the delegate.
*/

void QQmlDelegateModelAttached::emitChanges()
{
    const int groupChanges = m_previousGroups ^ m_cacheItem->groups;
    m_previousGroups = m_cacheItem->groups;

    int indexChanges = 0;
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i) {
        if (m_previousIndex[i] != m_currentIndex[i]) {
            m_previousIndex[i] = m_currentIndex[i];
            indexChanges |= (1 << i);
        }
    }

    int notifierId = 0;
    const QMetaObject *meta = metaObject();
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i, ++notifierId) {
        if (groupChanges & (1 << i))
            QMetaObject::activate(this, meta, notifierId, 0);
    }
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i, ++notifierId) {
        if (indexChanges & (1 << i))
            QMetaObject::activate(this, meta, notifierId, 0);
    }

    if (groupChanges)
        emit groupsChanged();
}

//============================================================================

void QQmlDelegateModelGroupPrivate::setModel(QQmlDelegateModel *m, Compositor::Group g)
{
    Q_ASSERT(!model);
    model = m;
    group = g;
}

bool QQmlDelegateModelGroupPrivate::isChangedConnected()
{
    Q_Q(QQmlDelegateModelGroup);
    IS_SIGNAL_CONNECTED(q, QQmlDelegateModelGroup, changed, (const QQmlV4Handle &,const QQmlV4Handle &));
}

void QQmlDelegateModelGroupPrivate::emitChanges(QV8Engine *engine)
{
    Q_Q(QQmlDelegateModelGroup);
    if (isChangedConnected() && !changeSet.isEmpty()) {
        QV4::Scope scope(QV8Engine::getV4(engine));
        QV4::ScopedValue removed(scope, engineData(scope.engine)->array(engine, changeSet.removes()));
        QV4::ScopedValue inserted(scope, engineData(scope.engine)->array(engine, changeSet.inserts()));
        emit q->changed(QQmlV4Handle(removed), QQmlV4Handle(inserted));
    }
    if (changeSet.difference() != 0)
        emit q->countChanged();
}

void QQmlDelegateModelGroupPrivate::emitModelUpdated(bool reset)
{
    for (QQmlDelegateModelGroupEmitterList::iterator it = emitters.begin(); it != emitters.end(); ++it)
        it->emitModelUpdated(changeSet, reset);
    changeSet.clear();
}

typedef QQmlDelegateModelGroupEmitterList::iterator GroupEmitterListIt;

void QQmlDelegateModelGroupPrivate::createdPackage(int index, QQuickPackage *package)
{
    for (GroupEmitterListIt it = emitters.begin(), end = emitters.end(); it != end; ++it)
        it->createdPackage(index, package);
}

void QQmlDelegateModelGroupPrivate::initPackage(int index, QQuickPackage *package)
{
    for (GroupEmitterListIt it = emitters.begin(), end = emitters.end(); it != end; ++it)
        it->initPackage(index, package);
}

void QQmlDelegateModelGroupPrivate::destroyingPackage(QQuickPackage *package)
{
    for (GroupEmitterListIt it = emitters.begin(), end = emitters.end(); it != end; ++it)
        it->destroyingPackage(package);
}

/*!
    \qmltype VisualDataGroup
    \instantiates QQmlDelegateModelGroup
    \inqmlmodule QtQuick
    \ingroup qtquick-models
    \brief Encapsulates a filtered set of visual data items

    The VisualDataGroup type provides a means to address the model data of a
    model's delegate items, as well as sort and filter these delegate items.

    This type is provided by the \l{Qt QML} module due to compatibility reasons.
    The same implementation is now primarily available as \l DelegateModelGroup
    in the \l{Qt QML Models QML Types}{Qt QML Models} module.

    \sa {QtQml.Models::DelegateModelGroup}
*/
/*!
    \qmltype DelegateModelGroup
    \instantiates QQmlDelegateModelGroup
    \inqmlmodule QtQml.Models
    \ingroup qtquick-models
    \brief Encapsulates a filtered set of visual data items

    The DelegateModelGroup type provides a means to address the model data of a
    DelegateModel's delegate items, as well as sort and filter these delegate
    items.

    The initial set of instantiable delegate items in a DelegateModel is represented
    by its \l {QtQml.Models::DelegateModel::items}{items} group, which normally directly reflects
    the contents of the model assigned to DelegateModel::model.  This set can be changed to
    the contents of any other member of DelegateModel::groups by assigning the  \l name of that
    DelegateModelGroup to the DelegateModel::filterOnGroup property.

    The data of an item in a DelegateModelGroup can be accessed using the get() function, which returns
    information about group membership and indexes as well as model data.  In combination
    with the move() function this can be used to implement view sorting, with remove() to filter
    items out of a view, or with setGroups() and \l Package delegates to categorize items into
    different views.

    Data from models can be supplemented by inserting data directly into a DelegateModelGroup
    with the insert() function.  This can be used to introduce mock items into a view, or
    placeholder items that are later \l {resolve()}{resolved} to real model data when it becomes
    available.

    Delegate items can also be instantiated directly from a DelegateModelGroup using the
    create() function, making it possible to use DelegateModel without an accompanying view
    type or to cherry-pick specific items that should be instantiated irregardless of whether
    they're currently within a view's visible area.

    \note This type is also available as \l VisualDataGroup in the \l{Qt QML}
    module due to compatibility reasons.

    \sa {QML Dynamic View Ordering Tutorial}
*/
QQmlDelegateModelGroup::QQmlDelegateModelGroup(QObject *parent)
    : QObject(*new QQmlDelegateModelGroupPrivate, parent)
{
}

QQmlDelegateModelGroup::QQmlDelegateModelGroup(
        const QString &name, QQmlDelegateModel *model, int index, QObject *parent)
    : QQmlDelegateModelGroup(parent)
{
    Q_D(QQmlDelegateModelGroup);
    d->name = name;
    d->setModel(model, Compositor::Group(index));
}

QQmlDelegateModelGroup::~QQmlDelegateModelGroup()
{
}

/*!
    \qmlproperty string QtQml.Models::DelegateModelGroup::name

    This property holds the name of the group.

    Each group in a model must have a unique name starting with a lower case letter.
*/

QString QQmlDelegateModelGroup::name() const
{
    Q_D(const QQmlDelegateModelGroup);
    return d->name;
}

void QQmlDelegateModelGroup::setName(const QString &name)
{
    Q_D(QQmlDelegateModelGroup);
    if (d->model)
        return;
    if (d->name != name) {
        d->name = name;
        emit nameChanged();
    }
}

/*!
    \qmlproperty int QtQml.Models::DelegateModelGroup::count

    This property holds the number of items in the group.
*/

int QQmlDelegateModelGroup::count() const
{
    Q_D(const QQmlDelegateModelGroup);
    if (!d->model)
        return 0;
    return QQmlDelegateModelPrivate::get(d->model)->m_compositor.count(d->group);
}

/*!
    \qmlproperty bool QtQml.Models::DelegateModelGroup::includeByDefault

    This property holds whether new items are assigned to this group by default.
*/

bool QQmlDelegateModelGroup::defaultInclude() const
{
    Q_D(const QQmlDelegateModelGroup);
    return d->defaultInclude;
}

void QQmlDelegateModelGroup::setDefaultInclude(bool include)
{
    Q_D(QQmlDelegateModelGroup);
    if (d->defaultInclude != include) {
        d->defaultInclude = include;

        if (d->model) {
            if (include)
                QQmlDelegateModelPrivate::get(d->model)->m_compositor.setDefaultGroup(d->group);
            else
                QQmlDelegateModelPrivate::get(d->model)->m_compositor.clearDefaultGroup(d->group);
        }
        emit defaultIncludeChanged();
    }
}

/*!
    \qmlmethod object QtQml.Models::DelegateModelGroup::get(int index)

    Returns a javascript object describing the item at \a index in the group.

    The returned object contains the same information that is available to a delegate from the
    DelegateModel attached as well as the model for that item.  It has the properties:

    \list
    \li \b model The model data of the item.  This is the same as the model context property in
    a delegate
    \li \b groups A list the of names of groups the item is a member of.  This property can be
    written to change the item's membership.
    \li \b inItems Whether the item belongs to the \l {QtQml.Models::DelegateModel::items}{items} group.
    Writing to this property will add or remove the item from the group.
    \li \b itemsIndex The index of the item within the \l {QtQml.Models::DelegateModel::items}{items} group.
    \li \b {in<GroupName>} Whether the item belongs to the dynamic group \e groupName.  Writing to
    this property will add or remove the item from the group.
    \li \b {<groupName>Index} The index of the item within the dynamic group \e groupName.
    \li \b isUnresolved Whether the item is bound to an index in the model assigned to
    DelegateModel::model.  Returns true if the item is not bound to the model, and false if it is.
    \endlist
*/

QQmlV4Handle QQmlDelegateModelGroup::get(int index)
{
    Q_D(QQmlDelegateModelGroup);
    if (!d->model)
        return QQmlV4Handle(QV4::Encode::undefined());

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (!model->m_context || !model->m_context->isValid()) {
        return QQmlV4Handle(QV4::Encode::undefined());
    } else if (index < 0 || index >= model->m_compositor.count(d->group)) {
        qmlInfo(this) << tr("get: index out of range");
        return QQmlV4Handle(QV4::Encode::undefined());
    }

    Compositor::iterator it = model->m_compositor.find(d->group, index);
    QQmlDelegateModelItem *cacheItem = it->inCache()
            ? model->m_cache.at(it.cacheIndex)
            : 0;

    if (!cacheItem) {
        cacheItem = model->m_adaptorModel.createItem(
                model->m_cacheMetaType, model->m_context->engine(), it.modelIndex());
        if (!cacheItem)
            return QQmlV4Handle(QV4::Encode::undefined());
        cacheItem->groups = it->flags;

        model->m_cache.insert(it.cacheIndex, cacheItem);
        model->m_compositor.setFlags(it, 1, Compositor::CacheFlag);
    }

    if (model->m_cacheMetaType->modelItemProto.isUndefined())
        model->m_cacheMetaType->initializePrototype();
    QV8Engine *v8 = model->m_cacheMetaType->v8Engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8);
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, v4->memoryManager->allocObject<QQmlDelegateModelItemObject>(cacheItem));
    QV4::ScopedObject p(scope, model->m_cacheMetaType->modelItemProto.value());
    o->setPrototype(p);
    ++cacheItem->scriptRef;

    return QQmlV4Handle(o);
}

bool QQmlDelegateModelGroupPrivate::parseIndex(const QV4::Value &value, int *index, Compositor::Group *group) const
{
    if (value.isNumber()) {
        *index = value.toInt32();
        return true;
    }

    if (!value.isObject())
        return false;

    QV4::ExecutionEngine *v4 = value.as<QV4::Object>()->engine();
    QV4::Scope scope(v4);
    QV4::Scoped<QQmlDelegateModelItemObject> object(scope, value);

    if (object) {
        QQmlDelegateModelItem * const cacheItem = object->d()->item;
        if (QQmlDelegateModelPrivate *model = cacheItem->metaType->model
                ? QQmlDelegateModelPrivate::get(cacheItem->metaType->model)
                : 0) {
            *index = model->m_cache.indexOf(cacheItem);
            *group = Compositor::Cache;
            return true;
        }
    }
    return false;
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::insert(int index, jsdict data, array groups = undefined)
    \qmlmethod QtQml.Models::DelegateModelGroup::insert(jsdict data, var groups = undefined)

    Creates a new entry at \a index in a DelegateModel with the values from \a data that
    correspond to roles in the model assigned to DelegateModel::model.

    If no index is supplied the data is appended to the model.

    The optional \a groups parameter identifies the groups the new entry should belong to,
    if unspecified this is equal to the group insert was called on.

    Data inserted into a DelegateModel can later be merged with an existing entry in
    DelegateModel::model using the \l resolve() function.  This can be used to create placeholder
    items that are later replaced by actual data.
*/

void QQmlDelegateModelGroup::insert(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    int index = model->m_compositor.count(d->group);
    Compositor::Group group = d->group;

    if (args->length() == 0)
        return;

    int  i = 0;
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[i]);
    if (d->parseIndex(v, &index, &group)) {
        if (index < 0 || index > model->m_compositor.count(group)) {
            qmlInfo(this) << tr("insert: index out of range");
            return;
        }
        if (++i == args->length())
            return;
        v = (*args)[i];
    }

    Compositor::insert_iterator before = index < model->m_compositor.count(group)
            ? model->m_compositor.findInsertPosition(group, index)
            : model->m_compositor.end();

    int groups = 1 << d->group;
    if (++i < args->length()) {
        QV4::ScopedValue val(scope, (*args)[i]);
        groups |= model->m_cacheMetaType->parseGroups(val);
    }

    if (v->as<QV4::ArrayObject>()) {
        return;
    } else if (v->as<QV4::Object>()) {
        model->insert(before, v, groups);
        model->emitChanges();
    }
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::create(int index)
    \qmlmethod QtQml.Models::DelegateModelGroup::create(int index, jsdict data, array groups = undefined)
    \qmlmethod QtQml.Models::DelegateModelGroup::create(jsdict data, array groups = undefined)

    Returns a reference to the instantiated item at \a index in the group.

    If a \a data object is provided it will be \l {insert}{inserted} at \a index and an item
    referencing this new entry will be returned.  The optional \a groups parameter identifies
    the groups the new entry should belong to, if unspecified this is equal to the group create()
    was called on.

    All items returned by create are added to the
    \l {QtQml.Models::DelegateModel::persistedItems}{persistedItems} group.  Items in this
    group remain instantiated when not referenced by any view.
*/

void QQmlDelegateModelGroup::create(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    if (!d->model)
        return;

    if (args->length() == 0)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    int index = model->m_compositor.count(d->group);
    Compositor::Group group = d->group;

    int  i = 0;
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[i]);
    if (d->parseIndex(v, &index, &group))
        ++i;

    if (i < args->length() && index >= 0 && index <= model->m_compositor.count(group)) {
        v = (*args)[i];
        if (v->as<QV4::Object>()) {
            int groups = 1 << d->group;
            if (++i < args->length()) {
                QV4::ScopedValue val(scope, (*args)[i]);
                groups |= model->m_cacheMetaType->parseGroups(val);
            }

            Compositor::insert_iterator before = index < model->m_compositor.count(group)
                    ? model->m_compositor.findInsertPosition(group, index)
                    : model->m_compositor.end();

            index = before.index[d->group];
            group = d->group;

            if (!model->insert(before, v, groups)) {
                return;
            }
        }
    }
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("create: index out of range");
        return;
    }

    QObject *object = model->object(group, index, false);
    if (object) {
        QVector<Compositor::Insert> inserts;
        Compositor::iterator it = model->m_compositor.find(group, index);
        model->m_compositor.setFlags(it, 1, d->group, Compositor::PersistedFlag, &inserts);
        model->itemsInserted(inserts);
        model->m_cache.at(it.cacheIndex)->releaseObject();
    }

    args->setReturnValue(QV4::QObjectWrapper::wrap(args->v4engine(), object));
    model->emitChanges();
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::resolve(int from, int to)

    Binds an unresolved item at \a from to an item in DelegateModel::model at index \a to.

    Unresolved items are entries whose data has been \l {insert()}{inserted} into a DelegateModelGroup
    instead of being derived from a DelegateModel::model index.  Resolving an item will replace
    the item at the target index with the unresolved item. A resolved an item will reflect the data
    of the source model at its bound index and will move when that index moves like any other item.

    If a new item is replaced in the DelegateModelGroup onChanged() handler its insertion and
    replacement will be communicated to views as an atomic operation, creating the appearance
    that the model contents have not changed, or if the unresolved and model item are not adjacent
    that the previously unresolved item has simply moved.

*/
void QQmlDelegateModelGroup::resolve(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    if (!d->model)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    if (args->length() < 2)
        return;

    int from = -1;
    int to = -1;
    Compositor::Group fromGroup = d->group;
    Compositor::Group toGroup = d->group;

    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[0]);
    if (d->parseIndex(v, &from, &fromGroup)) {
        if (from < 0 || from >= model->m_compositor.count(fromGroup)) {
            qmlInfo(this) << tr("resolve: from index out of range");
            return;
        }
    } else {
        qmlInfo(this) << tr("resolve: from index invalid");
        return;
    }

    v = (*args)[1];
    if (d->parseIndex(v, &to, &toGroup)) {
        if (to < 0 || to >= model->m_compositor.count(toGroup)) {
            qmlInfo(this) << tr("resolve: to index out of range");
            return;
        }
    } else {
        qmlInfo(this) << tr("resolve: to index invalid");
        return;
    }

    Compositor::iterator fromIt = model->m_compositor.find(fromGroup, from);
    Compositor::iterator toIt = model->m_compositor.find(toGroup, to);

    if (!fromIt->isUnresolved()) {
        qmlInfo(this) << tr("resolve: from is not an unresolved item");
        return;
    }
    if (!toIt->list) {
        qmlInfo(this) << tr("resolve: to is not a model item");
        return;
    }

    const int unresolvedFlags = fromIt->flags;
    const int resolvedFlags = toIt->flags;
    const int resolvedIndex = toIt.modelIndex();
    void * const resolvedList = toIt->list;

    QQmlDelegateModelItem *cacheItem = model->m_cache.at(fromIt.cacheIndex);
    cacheItem->groups &= ~Compositor::UnresolvedFlag;

    if (toIt.cacheIndex > fromIt.cacheIndex)
        toIt.decrementIndexes(1, unresolvedFlags);
    if (!toIt->inGroup(fromGroup) || toIt.index[fromGroup] > from)
        from += 1;

    model->itemsMoved(
            QVector<Compositor::Remove>() << Compositor::Remove(fromIt, 1, unresolvedFlags, 0),
            QVector<Compositor::Insert>() << Compositor::Insert(toIt, 1, unresolvedFlags, 0));
    model->itemsInserted(
            QVector<Compositor::Insert>() << Compositor::Insert(toIt, 1, (resolvedFlags & ~unresolvedFlags) | Compositor::CacheFlag));
    toIt.incrementIndexes(1, resolvedFlags | unresolvedFlags);
    model->itemsRemoved(QVector<Compositor::Remove>() << Compositor::Remove(toIt, 1, resolvedFlags));

    model->m_compositor.setFlags(toGroup, to, 1, unresolvedFlags & ~Compositor::UnresolvedFlag);
    model->m_compositor.clearFlags(fromGroup, from, 1, unresolvedFlags);

    if (resolvedFlags & Compositor::CacheFlag)
        model->m_compositor.insert(Compositor::Cache, toIt.cacheIndex, resolvedList, resolvedIndex, 1, Compositor::CacheFlag);

    Q_ASSERT(model->m_cache.count() == model->m_compositor.count(Compositor::Cache));

    if (!cacheItem->isReferenced()) {
        Q_ASSERT(toIt.cacheIndex == model->m_cache.indexOf(cacheItem));
        model->m_cache.removeAt(toIt.cacheIndex);
        model->m_compositor.clearFlags(Compositor::Cache, toIt.cacheIndex, 1, Compositor::CacheFlag);
        delete cacheItem;
        Q_ASSERT(model->m_cache.count() == model->m_compositor.count(Compositor::Cache));
    } else {
        cacheItem->resolveIndex(model->m_adaptorModel, resolvedIndex);
        if (cacheItem->attached)
            cacheItem->attached->emitUnresolvedChanged();
    }

    model->emitChanges();
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::remove(int index, int count)

    Removes \a count items starting at \a index from the group.
*/

void QQmlDelegateModelGroup::remove(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    if (!d->model)
        return;
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;

    if (args->length() == 0)
        return;

    int i = 0;
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[0]);
    if (!d->parseIndex(v, &index, &group)) {
        qmlInfo(this) << tr("remove: invalid index");
        return;
    }

    if (++i < args->length()) {
        v = (*args)[i];
        if (v->isNumber())
            count = v->toInt32();
    }

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("remove: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("remove: invalid count");
        } else {
            model->removeGroups(it, count, d->group, 1 << d->group);
        }
    }
}

bool QQmlDelegateModelGroupPrivate::parseGroupArgs(
        QQmlV4Function *args, Compositor::Group *group, int *index, int *count, int *groups) const
{
    if (!model || !QQmlDelegateModelPrivate::get(model)->m_cacheMetaType)
        return false;

    if (args->length() < 2)
        return false;

    int i = 0;
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[i]);
    if (!parseIndex(v, index, group))
        return false;

    v = (*args)[++i];
    if (v->isNumber()) {
        *count = v->toInt32();

        if (++i == args->length())
            return false;
        v = (*args)[i];
    }

    *groups = QQmlDelegateModelPrivate::get(model)->m_cacheMetaType->parseGroups(v);

    return true;
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::addGroups(int index, int count, stringlist groups)

    Adds \a count items starting at \a index to \a groups.
*/

void QQmlDelegateModelGroup::addGroups(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("addGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("addGroups: invalid count");
        } else {
            model->addGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::removeGroups(int index, int count, stringlist groups)

    Removes \a count items starting at \a index from \a groups.
*/

void QQmlDelegateModelGroup::removeGroups(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("removeGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("removeGroups: invalid count");
        } else {
            model->removeGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::setGroups(int index, int count, stringlist groups)

    Sets the \a groups \a count items starting at \a index belong to.
*/

void QQmlDelegateModelGroup::setGroups(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("setGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("setGroups: invalid count");
        } else {
            model->setGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::setGroups(int index, int count, stringlist groups)

    Sets the \a groups \a count items starting at \a index belong to.
*/

/*!
    \qmlmethod QtQml.Models::DelegateModelGroup::move(var from, var to, int count)

    Moves \a count at \a from in a group \a to a new position.
*/

void QQmlDelegateModelGroup::move(QQmlV4Function *args)
{
    Q_D(QQmlDelegateModelGroup);

    if (args->length() < 2)
        return;

    Compositor::Group fromGroup = d->group;
    Compositor::Group toGroup = d->group;
    int from = -1;
    int to = -1;
    int count = 1;

    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[0]);
    if (!d->parseIndex(v, &from, &fromGroup)) {
        qmlInfo(this) << tr("move: invalid from index");
        return;
    }

    v = (*args)[1];
    if (!d->parseIndex(v, &to, &toGroup)) {
        qmlInfo(this) << tr("move: invalid to index");
        return;
    }

    if (args->length() > 2) {
        v = (*args)[2];
        if (v->isNumber())
            count = v->toInt32();
    }

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    if (count < 0) {
        qmlInfo(this) << tr("move: invalid count");
    } else if (from < 0 || from + count > model->m_compositor.count(fromGroup)) {
        qmlInfo(this) << tr("move: from index out of range");
    } else if (!model->m_compositor.verifyMoveTo(fromGroup, from, toGroup, to, count, d->group)) {
        qmlInfo(this) << tr("move: to index out of range");
    } else if (count > 0) {
        QVector<Compositor::Remove> removes;
        QVector<Compositor::Insert> inserts;

        model->m_compositor.move(fromGroup, from, toGroup, to, count, d->group, &removes, &inserts);
        model->itemsMoved(removes, inserts);
        model->emitChanges();
    }

}

/*!
    \qmlsignal QtQml.Models::DelegateModelGroup::changed(array removed, array inserted)

    This signal is emitted when items have been removed from or inserted into the group.

    Each object in the \a removed and \a inserted arrays has two values; the \e index of the first
    item inserted or removed and a \e count of the number of consecutive items inserted or removed.

    Each index is adjusted for previous changes with all removed items preceding any inserted
    items.

    The corresponding handler is \c onChanged.
*/

//============================================================================

QQmlPartsModel::QQmlPartsModel(QQmlDelegateModel *model, const QString &part, QObject *parent)
    : QQmlInstanceModel(*new QObjectPrivate, parent)
    , m_model(model)
    , m_part(part)
    , m_compositorGroup(Compositor::Cache)
    , m_inheritGroup(true)
{
    QQmlDelegateModelPrivate *d = QQmlDelegateModelPrivate::get(m_model);
    if (d->m_cacheMetaType) {
        QQmlDelegateModelGroupPrivate::get(d->m_groups[1])->emitters.insert(this);
        m_compositorGroup = Compositor::Default;
    } else {
        d->m_pendingParts.insert(this);
    }
}

QQmlPartsModel::~QQmlPartsModel()
{
}

QString QQmlPartsModel::filterGroup() const
{
    if (m_inheritGroup)
        return m_model->filterGroup();
    return m_filterGroup;
}

void QQmlPartsModel::setFilterGroup(const QString &group)
{
    if (QQmlDelegateModelPrivate::get(m_model)->m_transaction) {
        qmlInfo(this) << tr("The group of a DelegateModel cannot be changed within onChanged");
        return;
    }

    if (m_filterGroup != group || m_inheritGroup) {
        m_filterGroup = group;
        m_inheritGroup = false;
        updateFilterGroup();

        emit filterGroupChanged();
    }
}

void QQmlPartsModel::resetFilterGroup()
{
    if (!m_inheritGroup) {
        m_inheritGroup = true;
        updateFilterGroup();
        emit filterGroupChanged();
    }
}

void QQmlPartsModel::updateFilterGroup()
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    if (!model->m_cacheMetaType)
        return;

    if (m_inheritGroup) {
        if (m_filterGroup == model->m_filterGroup)
            return;
        m_filterGroup = model->m_filterGroup;
    }

    QQmlListCompositor::Group previousGroup = m_compositorGroup;
    m_compositorGroup = Compositor::Default;
    QQmlDelegateModelGroupPrivate::get(model->m_groups[Compositor::Default])->emitters.insert(this);
    for (int i = 1; i < model->m_groupCount; ++i) {
        if (m_filterGroup == model->m_cacheMetaType->groupNames.at(i - 1)) {
            m_compositorGroup = Compositor::Group(i);
            break;
        }
    }

    QQmlDelegateModelGroupPrivate::get(model->m_groups[m_compositorGroup])->emitters.insert(this);
    if (m_compositorGroup != previousGroup) {
        QVector<QQmlChangeSet::Change> removes;
        QVector<QQmlChangeSet::Change> inserts;
        model->m_compositor.transition(previousGroup, m_compositorGroup, &removes, &inserts);

        QQmlChangeSet changeSet;
        changeSet.move(removes, inserts);
        if (!changeSet.isEmpty())
            emit modelUpdated(changeSet, false);

        if (changeSet.difference() != 0)
            emit countChanged();
    }
}

void QQmlPartsModel::updateFilterGroup(
        Compositor::Group group, const QQmlChangeSet &changeSet)
{
    if (!m_inheritGroup)
        return;

    m_compositorGroup = group;
    QQmlDelegateModelGroupPrivate::get(QQmlDelegateModelPrivate::get(m_model)->m_groups[m_compositorGroup])->emitters.insert(this);

    if (!changeSet.isEmpty())
        emit modelUpdated(changeSet, false);

    if (changeSet.difference() != 0)
        emit countChanged();

    emit filterGroupChanged();
}

int QQmlPartsModel::count() const
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    return model->m_delegate
            ? model->m_compositor.count(m_compositorGroup)
            : 0;
}

bool QQmlPartsModel::isValid() const
{
    return m_model->isValid();
}

QObject *QQmlPartsModel::object(int index, bool asynchronous)
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);

    if (!model->m_delegate || index < 0 || index >= model->m_compositor.count(m_compositorGroup)) {
        qWarning() << "DelegateModel::item: index out range" << index << model->m_compositor.count(m_compositorGroup);
        return 0;
    }

    QObject *object = model->object(m_compositorGroup, index, asynchronous);

    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object)) {
        QObject *part = package->part(m_part);
        if (!part)
            return 0;
        m_packaged.insertMulti(part, package);
        return part;
    }

    model->release(object);
    if (!model->m_delegateValidated) {
        if (object)
            qmlInfo(model->m_delegate) << tr("Delegate component must be Package type.");
        model->m_delegateValidated = true;
    }

    return 0;
}

QQmlInstanceModel::ReleaseFlags QQmlPartsModel::release(QObject *item)
{
    QQmlInstanceModel::ReleaseFlags flags = 0;

    QHash<QObject *, QQuickPackage *>::iterator it = m_packaged.find(item);
    if (it != m_packaged.end()) {
        QQuickPackage *package = *it;
        QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
        flags = model->release(package);
        m_packaged.erase(it);
        if (!m_packaged.contains(item))
            flags &= ~Referenced;
        if (flags & Destroyed)
            QQmlDelegateModelPrivate::get(m_model)->emitDestroyingPackage(package);
    }
    return flags;
}

QString QQmlPartsModel::stringValue(int index, const QString &role)
{
    return QQmlDelegateModelPrivate::get(m_model)->stringValue(m_compositorGroup, index, role);
}

void QQmlPartsModel::setWatchedRoles(const QList<QByteArray> &roles)
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    model->m_adaptorModel.replaceWatchedRoles(m_watchedRoles, roles);
    m_watchedRoles = roles;
}

int QQmlPartsModel::indexOf(QObject *item, QObject *) const
{
    QHash<QObject *, QQuickPackage *>::const_iterator it = m_packaged.find(item);
    if (it != m_packaged.end()) {
        if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(*it))
            return cacheItem->groupIndex(m_compositorGroup);
    }
    return -1;
}

void QQmlPartsModel::createdPackage(int index, QQuickPackage *package)
{
    emit createdItem(index, package->part(m_part));
}

void QQmlPartsModel::initPackage(int index, QQuickPackage *package)
{
    emit initItem(index, package->part(m_part));
}

void QQmlPartsModel::destroyingPackage(QQuickPackage *package)
{
    QObject *item = package->part(m_part);
    Q_ASSERT(!m_packaged.contains(item));
    emit destroyingItem(item);
}

void QQmlPartsModel::emitModelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    emit modelUpdated(changeSet, reset);
    if (changeSet.difference() != 0)
        emit countChanged();
}

//============================================================================

struct QQmlDelegateModelGroupChange : QV4::Object
{
    V4_OBJECT2(QQmlDelegateModelGroupChange, QV4::Object)

    static QV4::Heap::QQmlDelegateModelGroupChange *create(QV4::ExecutionEngine *e) {
        return e->memoryManager->allocObject<QQmlDelegateModelGroupChange>();
    }

    static QV4::ReturnedValue method_get_index(QV4::CallContext *ctx) {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelGroupChange> that(scope, ctx->thisObject().as<QQmlDelegateModelGroupChange>());
        if (!that)
            return ctx->engine()->throwTypeError();
        return QV4::Encode(that->d()->change.index);
    }
    static QV4::ReturnedValue method_get_count(QV4::CallContext *ctx) {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelGroupChange> that(scope, ctx->thisObject().as<QQmlDelegateModelGroupChange>());
        if (!that)
            return ctx->engine()->throwTypeError();
        return QV4::Encode(that->d()->change.count);
    }
    static QV4::ReturnedValue method_get_moveId(QV4::CallContext *ctx) {
        QV4::Scope scope(ctx);
        QV4::Scoped<QQmlDelegateModelGroupChange> that(scope, ctx->thisObject().as<QQmlDelegateModelGroupChange>());
        if (!that)
            return ctx->engine()->throwTypeError();
        if (that->d()->change.moveId < 0)
            return QV4::Encode::undefined();
        return QV4::Encode(that->d()->change.moveId);
    }
};

DEFINE_OBJECT_VTABLE(QQmlDelegateModelGroupChange);

struct QQmlDelegateModelGroupChangeArray : public QV4::Object
{
    V4_OBJECT2(QQmlDelegateModelGroupChangeArray, QV4::Object)
    V4_NEEDS_DESTROY
public:
    static QV4::Heap::QQmlDelegateModelGroupChangeArray *create(QV4::ExecutionEngine *engine, const QVector<QQmlChangeSet::Change> &changes)
    {
        return engine->memoryManager->allocObject<QQmlDelegateModelGroupChangeArray>(changes);
    }

    quint32 count() const { return d()->changes->count(); }
    const QQmlChangeSet::Change &at(int index) const { return d()->changes->at(index); }

    static QV4::ReturnedValue getIndexed(const QV4::Managed *m, uint index, bool *hasProperty)
    {
        Q_ASSERT(m->as<QQmlDelegateModelGroupChangeArray>());
        QV4::ExecutionEngine *v4 = static_cast<const QQmlDelegateModelGroupChangeArray *>(m)->engine();
        QV4::Scope scope(v4);
        QV4::Scoped<QQmlDelegateModelGroupChangeArray> array(scope, static_cast<const QQmlDelegateModelGroupChangeArray *>(m));

        if (index >= array->count()) {
            if (hasProperty)
                *hasProperty = false;
            return QV4::Primitive::undefinedValue().asReturnedValue();
        }

        const QQmlChangeSet::Change &change = array->at(index);

        QV4::ScopedObject changeProto(scope, engineData(v4)->changeProto.value());
        QV4::Scoped<QQmlDelegateModelGroupChange> object(scope, QQmlDelegateModelGroupChange::create(v4));
        object->setPrototype(changeProto);
        object->d()->change = change;

        if (hasProperty)
            *hasProperty = true;
        return object.asReturnedValue();
    }

    static QV4::ReturnedValue get(const QV4::Managed *m, QV4::String *name, bool *hasProperty)
    {
        Q_ASSERT(m->as<QQmlDelegateModelGroupChangeArray>());
        const QQmlDelegateModelGroupChangeArray *array = static_cast<const QQmlDelegateModelGroupChangeArray *>(m);

        if (name->equals(array->engine()->id_length())) {
            if (hasProperty)
                *hasProperty = true;
            return QV4::Encode(array->count());
        }

        return Object::get(m, name, hasProperty);
    }
};

void QV4::Heap::QQmlDelegateModelGroupChangeArray::init(const QVector<QQmlChangeSet::Change> &changes)
{
    Object::init();
    this->changes = new QVector<QQmlChangeSet::Change>(changes);
    QV4::Scope scope(internalClass->engine);
    QV4::ScopedObject o(scope, this);
    o->setArrayType(QV4::Heap::ArrayData::Custom);
}

DEFINE_OBJECT_VTABLE(QQmlDelegateModelGroupChangeArray);

QQmlDelegateModelEngineData::QQmlDelegateModelEngineData(QV4::ExecutionEngine *v4)
{
    QV4::Scope scope(v4);

    QV4::ScopedObject proto(scope, v4->newObject());
    proto->defineAccessorProperty(QStringLiteral("index"), QQmlDelegateModelGroupChange::method_get_index, 0);
    proto->defineAccessorProperty(QStringLiteral("count"), QQmlDelegateModelGroupChange::method_get_count, 0);
    proto->defineAccessorProperty(QStringLiteral("moveId"), QQmlDelegateModelGroupChange::method_get_moveId, 0);
    changeProto.set(v4, proto);
}

QQmlDelegateModelEngineData::~QQmlDelegateModelEngineData()
{
}

QV4::ReturnedValue QQmlDelegateModelEngineData::array(QV8Engine *engine, const QVector<QQmlChangeSet::Change> &changes)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, QQmlDelegateModelGroupChangeArray::create(v4, changes));
    return o.asReturnedValue();
}

QT_END_NAMESPACE
