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

#ifndef QQMLDATAMODEL_P_P_H
#define QQMLDATAMODEL_P_P_H

#include "qqmldelegatemodel_p.h"
#include <private/qv4qobjectwrapper_p.h>

#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlincubator.h>

#include <private/qqmladaptormodel_p.h>
#include <private/qqmlopenmetaobject_p.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

typedef QQmlListCompositor Compositor;

class QQmlDelegateModelAttachedMetaObject;

class QQmlDelegateModelItemMetaType : public QQmlRefCount
{
public:
    QQmlDelegateModelItemMetaType(QV8Engine *engine, QQmlDelegateModel *model, const QStringList &groupNames);
    ~QQmlDelegateModelItemMetaType();

    void initializeMetaObject();
    void initializePrototype();

    int parseGroups(const QStringList &groupNames) const;
    int parseGroups(const QV4::Value &groupNames) const;

    QPointer<QQmlDelegateModel> model;
    const int groupCount;
    QV8Engine * const v8Engine;
    QQmlDelegateModelAttachedMetaObject *metaObject;
    const QStringList groupNames;
    QV4::PersistentValue modelItemProto;
};

class QQmlAdaptorModel;
class QQDMIncubationTask;

class QQmlDelegateModelItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int index READ modelIndex NOTIFY modelIndexChanged)
    Q_PROPERTY(QObject *model READ modelObject CONSTANT)
public:
    QQmlDelegateModelItem(QQmlDelegateModelItemMetaType *metaType, int modelIndex);
    ~QQmlDelegateModelItem();

    void referenceObject() { ++objectRef; }
    bool releaseObject() { return --objectRef == 0 && !(groups & Compositor::PersistedFlag); }
    bool isObjectReferenced() const { return objectRef != 0 || (groups & Compositor::PersistedFlag); }

    bool isReferenced() const {
        return scriptRef
                || incubationTask
                || ((groups & Compositor::UnresolvedFlag) && (groups & Compositor::GroupMask));
    }

    void Dispose();

    QObject *modelObject() { return this; }

    void incubateObject(
            QQmlComponent *component,
            QQmlEngine *engine,
            QQmlContextData *context,
            QQmlContextData *forContext);
    void destroyObject();

    static QQmlDelegateModelItem *dataForObject(QObject *object);

    int groupIndex(Compositor::Group group);

    int modelIndex() const { return index; }
    void setModelIndex(int idx) { index = idx; Q_EMIT modelIndexChanged(); }

    virtual QV4::ReturnedValue get() { return QV4::QObjectWrapper::wrap(v4, this); }

    virtual void setValue(const QString &role, const QVariant &value) { Q_UNUSED(role); Q_UNUSED(value); }
    virtual bool resolveIndex(const QQmlAdaptorModel &, int) { return false; }

    static QV4::ReturnedValue get_model(QV4::CallContext *ctx);
    static QV4::ReturnedValue get_groups(QV4::CallContext *ctx);
    static QV4::ReturnedValue set_groups(QV4::CallContext *ctx);
    static QV4::ReturnedValue get_member(QQmlDelegateModelItem *thisItem, uint flag, const QV4::Value &);
    static QV4::ReturnedValue set_member(QQmlDelegateModelItem *thisItem, uint flag, const QV4::Value &arg);
    static QV4::ReturnedValue get_index(QQmlDelegateModelItem *thisItem, uint flag, const QV4::Value &arg);

    QV4::ExecutionEngine *v4;
    QQmlDelegateModelItemMetaType * const metaType;
    QQmlContextData *contextData;
    QPointer<QObject> object;
    QPointer<QQmlDelegateModelAttached> attached;
    QQDMIncubationTask *incubationTask;
    int objectRef;
    int scriptRef;
    int groups;
    int index;

Q_SIGNALS:
    void modelIndexChanged();

protected:
    void objectDestroyed(QObject *);
};

namespace QV4 {
namespace Heap {
struct QQmlDelegateModelItemObject : Object {
    inline void init(QQmlDelegateModelItem *item);
    void destroy();
    QQmlDelegateModelItem *item;
};

}
}

struct QQmlDelegateModelItemObject : QV4::Object
{
    V4_OBJECT2(QQmlDelegateModelItemObject, QV4::Object)
    V4_NEEDS_DESTROY
};

void QV4::Heap::QQmlDelegateModelItemObject::init(QQmlDelegateModelItem *item)
{
    Object::init();
    this->item = item;
}



class QQmlDelegateModelPrivate;
class QQDMIncubationTask : public QQmlIncubator
{
public:
    QQDMIncubationTask(QQmlDelegateModelPrivate *l, IncubationMode mode)
        : QQmlIncubator(mode)
        , incubating(0)
        , vdm(l) {}

    virtual void statusChanged(Status);
    virtual void setInitialState(QObject *);

    QQmlDelegateModelItem *incubating;
    QQmlDelegateModelPrivate *vdm;
    int index[QQmlListCompositor::MaximumGroupCount];
};


class QQmlDelegateModelGroupEmitter
{
public:
    virtual ~QQmlDelegateModelGroupEmitter() {}
    virtual void emitModelUpdated(const QQmlChangeSet &changeSet, bool reset) = 0;
    virtual void createdPackage(int, QQuickPackage *) {}
    virtual void initPackage(int, QQuickPackage *) {}
    virtual void destroyingPackage(QQuickPackage *) {}

    QIntrusiveListNode emitterNode;
};

typedef QIntrusiveList<QQmlDelegateModelGroupEmitter, &QQmlDelegateModelGroupEmitter::emitterNode> QQmlDelegateModelGroupEmitterList;

class QQmlDelegateModelGroupPrivate : public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(QQmlDelegateModelGroup)

    QQmlDelegateModelGroupPrivate() : group(Compositor::Cache), defaultInclude(false) {}

    static QQmlDelegateModelGroupPrivate *get(QQmlDelegateModelGroup *group) {
        return static_cast<QQmlDelegateModelGroupPrivate *>(QObjectPrivate::get(group)); }

    void setModel(QQmlDelegateModel *model, Compositor::Group group);
    bool isChangedConnected();
    void emitChanges(QV8Engine *engine);
    void emitModelUpdated(bool reset);

    void createdPackage(int index, QQuickPackage *package);
    void initPackage(int index, QQuickPackage *package);
    void destroyingPackage(QQuickPackage *package);

    bool parseIndex(const QV4::Value &value, int *index, Compositor::Group *group) const;
    bool parseGroupArgs(
            QQmlV4Function *args, Compositor::Group *group, int *index, int *count, int *groups) const;

    Compositor::Group group;
    QPointer<QQmlDelegateModel> model;
    QQmlDelegateModelGroupEmitterList emitters;
    QQmlChangeSet changeSet;
    QString name;
    bool defaultInclude;
};

class QQmlDelegateModelParts;

class QQmlDelegateModelPrivate : public QObjectPrivate, public QQmlDelegateModelGroupEmitter
{
    Q_DECLARE_PUBLIC(QQmlDelegateModel)
public:
    QQmlDelegateModelPrivate(QQmlContext *);
    ~QQmlDelegateModelPrivate();

    static QQmlDelegateModelPrivate *get(QQmlDelegateModel *m) {
        return static_cast<QQmlDelegateModelPrivate *>(QObjectPrivate::get(m));
    }

    void init();
    void connectModel(QQmlAdaptorModel *model);

    void requestMoreIfNecessary();
    QObject *object(Compositor::Group group, int index, bool asynchronous);
    QQmlDelegateModel::ReleaseFlags release(QObject *object);
    QString stringValue(Compositor::Group group, int index, const QString &name);
    void emitCreatedPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package);
    void emitInitPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package);
    void emitCreatedItem(QQDMIncubationTask *incubationTask, QObject *item) {
        Q_EMIT q_func()->createdItem(incubationTask->index[m_compositorGroup], item); }
    void emitInitItem(QQDMIncubationTask *incubationTask, QObject *item) {
        Q_EMIT q_func()->initItem(incubationTask->index[m_compositorGroup], item); }
    void emitDestroyingPackage(QQuickPackage *package);
    void emitDestroyingItem(QObject *item) { Q_EMIT q_func()->destroyingItem(item); }
    void removeCacheItem(QQmlDelegateModelItem *cacheItem);

    void updateFilterGroup();

    void addGroups(Compositor::iterator from, int count, Compositor::Group group, int groupFlags);
    void removeGroups(Compositor::iterator from, int count, Compositor::Group group, int groupFlags);
    void setGroups(Compositor::iterator from, int count, Compositor::Group group, int groupFlags);

    void itemsInserted(
            const QVector<Compositor::Insert> &inserts,
            QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> *translatedInserts,
            QHash<int, QList<QQmlDelegateModelItem *> > *movedItems = 0);
    void itemsInserted(const QVector<Compositor::Insert> &inserts);
    void itemsRemoved(
            const QVector<Compositor::Remove> &removes,
            QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> *translatedRemoves,
            QHash<int, QList<QQmlDelegateModelItem *> > *movedItems = 0);
    void itemsRemoved(const QVector<Compositor::Remove> &removes);
    void itemsMoved(
            const QVector<Compositor::Remove> &removes, const QVector<Compositor::Insert> &inserts);
    void itemsChanged(const QVector<Compositor::Change> &changes);
    void emitChanges();
    void emitModelUpdated(const QQmlChangeSet &changeSet, bool reset);

    bool insert(Compositor::insert_iterator &before, const QV4::Value &object, int groups);

    static void group_append(QQmlListProperty<QQmlDelegateModelGroup> *property, QQmlDelegateModelGroup *group);
    static int group_count(QQmlListProperty<QQmlDelegateModelGroup> *property);
    static QQmlDelegateModelGroup *group_at(QQmlListProperty<QQmlDelegateModelGroup> *property, int index);

    void releaseIncubator(QQDMIncubationTask *incubationTask);
    void incubatorStatusChanged(QQDMIncubationTask *incubationTask, QQmlIncubator::Status status);
    void setInitialState(QQDMIncubationTask *incubationTask, QObject *o);

    QQmlAdaptorModel m_adaptorModel;
    QQmlListCompositor m_compositor;
    QQmlComponent *m_delegate;
    QQmlDelegateModelItemMetaType *m_cacheMetaType;
    QPointer<QQmlContext> m_context;
    QQmlDelegateModelParts *m_parts;
    QQmlDelegateModelGroupEmitterList m_pendingParts;

    QList<QQmlDelegateModelItem *> m_cache;
    QList<QQDMIncubationTask *> m_finishedIncubating;
    QList<QByteArray> m_watchedRoles;

    QString m_filterGroup;

    int m_count;
    int m_groupCount;

    QQmlListCompositor::Group m_compositorGroup;
    bool m_complete : 1;
    bool m_delegateValidated : 1;
    bool m_reset : 1;
    bool m_transaction : 1;
    bool m_incubatorCleanupScheduled : 1;
    bool m_waitingToFetchMore : 1;

    union {
        struct {
            QQmlDelegateModelGroup *m_cacheItems;
            QQmlDelegateModelGroup *m_items;
            QQmlDelegateModelGroup *m_persistedItems;
        };
        QQmlDelegateModelGroup *m_groups[Compositor::MaximumGroupCount];
    };
};

class QQmlPartsModel : public QQmlInstanceModel, public QQmlDelegateModelGroupEmitter
{
    Q_OBJECT
    Q_PROPERTY(QString filterOnGroup READ filterGroup WRITE setFilterGroup NOTIFY filterGroupChanged RESET resetFilterGroup)
public:
    QQmlPartsModel(QQmlDelegateModel *model, const QString &part, QObject *parent = 0);
    ~QQmlPartsModel();

    QString filterGroup() const;
    void setFilterGroup(const QString &group);
    void resetFilterGroup();
    void updateFilterGroup();
    void updateFilterGroup(Compositor::Group group, const QQmlChangeSet &changeSet);

    int count() const;
    bool isValid() const;
    QObject *object(int index, bool asynchronous=false);
    ReleaseFlags release(QObject *item);
    QString stringValue(int index, const QString &role);
    QList<QByteArray> watchedRoles() const { return m_watchedRoles; }
    void setWatchedRoles(const QList<QByteArray> &roles);

    int indexOf(QObject *item, QObject *objectContext) const;

    void emitModelUpdated(const QQmlChangeSet &changeSet, bool reset);

    void createdPackage(int index, QQuickPackage *package);
    void initPackage(int index, QQuickPackage *package);
    void destroyingPackage(QQuickPackage *package);

Q_SIGNALS:
    void filterGroupChanged();

private:
    QQmlDelegateModel *m_model;
    QHash<QObject *, QQuickPackage *> m_packaged;
    QString m_part;
    QString m_filterGroup;
    QList<QByteArray> m_watchedRoles;
    Compositor::Group m_compositorGroup;
    bool m_inheritGroup;
};

class QMetaPropertyBuilder;

class QQmlDelegateModelPartsMetaObject : public QQmlOpenMetaObject
{
public:
    QQmlDelegateModelPartsMetaObject(QObject *parent)
    : QQmlOpenMetaObject(parent) {}

    virtual void propertyCreated(int, QMetaPropertyBuilder &);
    virtual QVariant initialValue(int);
};

class QQmlDelegateModelParts : public QObject
{
Q_OBJECT
public:
    QQmlDelegateModelParts(QQmlDelegateModel *parent);

    QQmlDelegateModel *model;
    QList<QQmlPartsModel *> models;
};

class QQmlDelegateModelAttachedMetaObject : public QAbstractDynamicMetaObject, public QQmlRefCount
{
public:
    QQmlDelegateModelAttachedMetaObject(
            QQmlDelegateModelItemMetaType *metaType, QMetaObject *metaObject);
    ~QQmlDelegateModelAttachedMetaObject();

    void objectDestroyed(QObject *);
    int metaCall(QObject *, QMetaObject::Call, int _id, void **);

private:
    QQmlDelegateModelItemMetaType * const metaType;
    QMetaObject * const metaObject;
    const int memberPropertyOffset;
    const int indexPropertyOffset;
};

QT_END_NAMESPACE

#endif
