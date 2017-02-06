/****************************************************************************
**
** Copyright (C) 2013 Research In Motion.
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "quick3dnodeinstantiator_p.h"

#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlInfo>
#include <QtQml/QQmlError>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p.h>
#include <private/qnode_p.h>
#include <private/qqmlchangeset_p.h>
#include <private/qqmlobjectmodel_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

class Quick3DNodeInstantiatorPrivate : public QNodePrivate
{
    Q_DECLARE_PUBLIC(Quick3DNodeInstantiator)

public:
    Quick3DNodeInstantiatorPrivate();
    ~Quick3DNodeInstantiatorPrivate();

    void clear();
    void regenerate();
    void makeModel();
    void _q_createdItem(int, QObject *);
    void _q_modelUpdated(const QQmlChangeSet &, bool);

    bool m_componentComplete:1;
    bool m_effectiveReset:1;
    bool m_active:1;
    bool m_async:1;
    bool m_ownModel:1;
    QVariant m_model;
    QQmlInstanceModel *m_instanceModel;
    QQmlComponent *m_delegate;
    QVector<QPointer<QObject> > m_objects;
};

/*!
    \internal
*/
Quick3DNodeInstantiatorPrivate::Quick3DNodeInstantiatorPrivate()
    : QNodePrivate()
    , m_componentComplete(true)
    , m_effectiveReset(false)
    , m_active(true)
    , m_async(false)
    , m_ownModel(false)
    , m_model(QVariant(1))
    , m_instanceModel(0)
    , m_delegate(0)
{
}

Quick3DNodeInstantiatorPrivate::~Quick3DNodeInstantiatorPrivate()
{
    qDeleteAll(m_objects);
}

void Quick3DNodeInstantiatorPrivate::clear()
{
    Q_Q(Quick3DNodeInstantiator);
    if (!m_instanceModel)
        return;
    if (!m_objects.count())
        return;

    for (int i = 0; i < m_objects.count(); i++) {
        q->objectRemoved(i, m_objects[i]);
        m_instanceModel->release(m_objects[i]);
    }
    m_objects.clear();
    q->objectChanged();
}

void Quick3DNodeInstantiatorPrivate::regenerate()
{
    Q_Q(Quick3DNodeInstantiator);
    if (!m_componentComplete)
        return;

    int prevCount = q->count();

    clear();

    if (!m_active || !m_instanceModel || !m_instanceModel->count() || !m_instanceModel->isValid()) {
        if (prevCount)
            q->countChanged();
        return;
    }

    for (int i = 0; i < m_instanceModel->count(); i++) {
        QObject *object = m_instanceModel->object(i, m_async);
        // If the item was already created we won't get a createdItem
        if (object)
            _q_createdItem(i, object);
    }
    if (q->count() != prevCount)
        q->countChanged();
}

void Quick3DNodeInstantiatorPrivate::_q_createdItem(int idx, QObject *item)
{
    Q_Q(Quick3DNodeInstantiator);
    if (m_objects.contains(item)) //Case when it was created synchronously in regenerate
        return;
    static_cast<QNode *>(item)->setParent(q->parentNode());
    m_objects.insert(idx, item);
    if (m_objects.count() == 1)
        q->objectChanged();
    q->objectAdded(idx, item);
}

void Quick3DNodeInstantiatorPrivate::_q_modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_Q(Quick3DNodeInstantiator);

    if (!m_componentComplete || m_effectiveReset)
        return;

    if (reset) {
        regenerate();
        if (changeSet.difference() != 0)
            q->countChanged();
        return;
    }

    int difference = 0;
    QHash<int, QVector<QPointer<QObject> > > moved;
    const auto removes = changeSet.removes();
    for (const QQmlChangeSet::Change &remove : removes) {
        int index = qMin(remove.index, m_objects.count());
        int count = qMin(remove.index + remove.count, m_objects.count()) - index;
        if (remove.isMove()) {
            moved.insert(remove.moveId, m_objects.mid(index, count));
            m_objects.erase(
                    m_objects.begin() + index,
                    m_objects.begin() + index + count);
        } else {
            while (count--) {
                QObject *obj = m_objects.at(index);
                m_objects.remove(index);
                q->objectRemoved(index, obj);
                if (obj)
                    m_instanceModel->release(obj);
            }
        }

        difference -= remove.count;
    }

    const auto inserts = changeSet.inserts();
    for (const QQmlChangeSet::Change &insert : inserts) {
        int index = qMin(insert.index, m_objects.count());
        if (insert.isMove()) {
            QVector<QPointer<QObject> > movedObjects = moved.value(insert.moveId);
            m_objects = m_objects.mid(0, index) + movedObjects + m_objects.mid(index);
        } else for (int i = 0; i < insert.count; ++i) {
            int modelIndex = index + i;
            QObject *obj = m_instanceModel->object(modelIndex, m_async);
            if (obj)
                _q_createdItem(modelIndex, obj);
        }
        difference += insert.count;
    }

    if (difference != 0)
        q->countChanged();
}

void Quick3DNodeInstantiatorPrivate::makeModel()
{
    Q_Q(Quick3DNodeInstantiator);
    QQmlDelegateModel* delegateModel = new QQmlDelegateModel(qmlContext(q));
    m_instanceModel = delegateModel;
    m_ownModel = true;
    delegateModel->setDelegate(m_delegate);
    delegateModel->classBegin(); //Pretend it was made in QML
    if (m_componentComplete)
        delegateModel->componentComplete();
}

/*!
    \qmltype NodeInstantiator
    \inqmlmodule Qt3D.Core
    \brief Dynamically creates nodes.
    \since 5.5

    A NodeInstantiator can be used to control the dynamic creation of nodes,
    or to dynamically create multiple objects from a template.

    The NodeInstantiator element will manage the objects it creates. Those
    objects are parented to the Instantiator and can also be deleted by the
    NodeInstantiator if the NodeInstantiator's properties change. Nodes can
    also be destroyed dynamically through other means, and the NodeInstantiator
    will not recreate them unless the properties of the NodeInstantiator
    change.

*/
Quick3DNodeInstantiator::Quick3DNodeInstantiator(QNode *parent)
    : QNode(*new Quick3DNodeInstantiatorPrivate, parent)
{
}

/*!
    \qmlsignal Qt3D.Core::NodeInstantiator::objectAdded(int index, QtObject object)

    This signal is emitted when a node is added to the NodeInstantiator. The \a index
    parameter holds the index which the node has been given, and the \a object
    parameter holds the \l Node that has been added.

    The corresponding handler is \c onNodeAdded.
*/

/*!
    \qmlsignal Qt3D.Core::NodeInstantiator::objectRemoved(int index, QtObject object)

    This signal is emitted when an object is removed from the Instantiator. The \a index
    parameter holds the index which the object had been given, and the \a object
    parameter holds the \l [QML] {QtQml::}{QtObject} that has been removed.

    Do not keep a reference to \a object if it was created by this Instantiator, as
    in these cases it will be deleted shortly after the signal is handled.

    The corresponding handler is \c onObjectRemoved.
*/
/*!
    \qmlproperty bool Qt3D.Core::NodeInstantiator::active

    When active is \c true, and the delegate component is ready, the Instantiator will
    create objects according to the model. When active is \c false, no objects
    will be created and any previously created objects will be destroyed.

    Default is \c true.
*/
bool Quick3DNodeInstantiator::isActive() const
{
    Q_D(const Quick3DNodeInstantiator);
    return d->m_active;
}

void Quick3DNodeInstantiator::setActive(bool newVal)
{
    Q_D(Quick3DNodeInstantiator);
    if (newVal == d->m_active)
        return;
    d->m_active = newVal;
    emit activeChanged();
    d->regenerate();
}

/*!
    \qmlproperty bool Qt3D.Core::NodeInstantiator::asynchronous

    When asynchronous is true the Instantiator will attempt to create objects
    asynchronously. This means that objects may not be available immediately,
    even if active is set to true.

    You can use the objectAdded signal to respond to items being created.

    Default is \c false.
*/
bool Quick3DNodeInstantiator::isAsync() const
{
    Q_D(const Quick3DNodeInstantiator);
    return d->m_async;
}

void Quick3DNodeInstantiator::setAsync(bool newVal)
{
    Q_D(Quick3DNodeInstantiator);
    if (newVal == d->m_async)
        return;
    d->m_async = newVal;
    emit asynchronousChanged();
}


/*!
    \qmlproperty int Qt3D.Core::NodeInstantiator::count
    \readonly

    The number of objects the Instantiator is currently managing.
*/

int Quick3DNodeInstantiator::count() const
{
    Q_D(const Quick3DNodeInstantiator);
    return d->m_objects.count();
}

/*!
    \qmlproperty QtQml::Component Qt3D.Core::NodeInstantiator::delegate
    \default

    The component used to create all objects.

    Note that an extra variable, index, will be available inside instances of the
    delegate. This variable refers to the index of the instance inside the Instantiator,
    and can be used to obtain the object through the itemAt method of the Instantiator.

    If this property is changed, all instances using the old delegate will be destroyed
    and new instances will be created using the new delegate.
*/
QQmlComponent *Quick3DNodeInstantiator::delegate()
{
    Q_D(Quick3DNodeInstantiator);
    return d->m_delegate;
}

void Quick3DNodeInstantiator::setDelegate(QQmlComponent *c)
{
    Q_D(Quick3DNodeInstantiator);
    if (c == d->m_delegate)
        return;

    d->m_delegate = c;
    emit delegateChanged();

    if (!d->m_ownModel)
        return;

    if (QQmlDelegateModel *dModel = qobject_cast<QQmlDelegateModel*>(d->m_instanceModel))
        dModel->setDelegate(c);
    if (d->m_componentComplete)
        d->regenerate();
}

/*!
    \qmlproperty variant Qt3D.Core::NodeInstantiator::model

    This property can be set to any of the supported \l {qml-data-models}{data models}:

    \list
    \li A number that indicates the number of delegates to be created by the repeater
    \li A model (for example, a ListModel item or a QAbstractItemModel subclass)
    \li A string list
    \li An object list
    \endlist

    The type of model affects the properties that are exposed to the \l delegate.

    Default value is 1, which creates a single delegate instance.

    \sa {qml-data-models}{Data Models}
*/

QVariant Quick3DNodeInstantiator::model() const
{
    Q_D(const Quick3DNodeInstantiator);
    return d->m_model;
}

void Quick3DNodeInstantiator::setModel(const QVariant &v)
{
    Q_D(Quick3DNodeInstantiator);
    if (d->m_model == v)
        return;

    d->m_model = v;
    //Don't actually set model until componentComplete in case it wants to create its delegates immediately
    if (!d->m_componentComplete)
        return;

    QQmlInstanceModel *prevModel = d->m_instanceModel;
    QObject *object = qvariant_cast<QObject*>(v);
    QQmlInstanceModel *vim = 0;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        if (d->m_ownModel) {
            delete d->m_instanceModel;
            prevModel = 0;
            d->m_ownModel = false;
        }
        d->m_instanceModel = vim;
    } else if (v != QVariant(0)){
        if (!d->m_ownModel)
            d->makeModel();

        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel *>(d->m_instanceModel)) {
            d->m_effectiveReset = true;
            dataModel->setModel(v);
            d->m_effectiveReset = false;
        }
    }

    if (d->m_instanceModel != prevModel) {
        if (prevModel) {
            disconnect(prevModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                    this, SLOT(_q_modelUpdated(QQmlChangeSet,bool)));
            disconnect(prevModel, SIGNAL(createdItem(int,QObject*)), this, SLOT(_q_createdItem(int,QObject*)));
            //disconnect(prevModel, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
        }

        connect(d->m_instanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, SLOT(_q_modelUpdated(QQmlChangeSet,bool)));
        connect(d->m_instanceModel, SIGNAL(createdItem(int,QObject*)), this, SLOT(_q_createdItem(int,QObject*)));
        //connect(d->m_instanceModel, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
    }

    d->regenerate();
    emit modelChanged();
}

/*!
    \qmlproperty QtQml::QtObject Qt3D.Core::NodeInstantiator::object
    \readonly

    This is a reference to the first created object, intended as a convenience
    for the case where only one object has been created.
*/
QObject *Quick3DNodeInstantiator::object() const
{
    Q_D(const Quick3DNodeInstantiator);
    if (d->m_objects.count())
        return d->m_objects[0];
    return 0;
}

/*!
    \qmlmethod QtQml::QtObject Qt3D.Core::NodeInstantiator::objectAt(int index)

    Returns a reference to the object with the given \a index.
*/
QObject *Quick3DNodeInstantiator::objectAt(int index) const
{
    Q_D(const Quick3DNodeInstantiator);
    if (index >= 0 && index < d->m_objects.count())
        return d->m_objects[index];
    return 0;
}

/*!
 \internal
*/
void Quick3DNodeInstantiator::classBegin()
{
    Q_D(Quick3DNodeInstantiator);
    d->m_componentComplete = false;
}

/*!
 \internal
*/
void Quick3DNodeInstantiator::componentComplete()
{
    Q_D(Quick3DNodeInstantiator);
    d->m_componentComplete = true;
    if (d->m_ownModel) {
        static_cast<QQmlDelegateModel *>(d->m_instanceModel)->componentComplete();
        d->regenerate();
    } else {
        QVariant realModel = d->m_model;
        d->m_model = QVariant(0);
        setModel(realModel); //If realModel == d->m_model this won't do anything, but that's fine since the model's 0
        //setModel calls regenerate
    }
}

// TODO: Avoid cloning here
//void Quick3DNodeInstantiator::copy(const QNode *ref)
//{
//    QNode::copy(ref);
//    const Quick3DNodeInstantiator *instantiator = static_cast<const Quick3DNodeInstantiator*>(ref);
//    // We only need to clone the children as the instantiator itself has no
//    // corresponding backend node type.
//    for (int i = 0; i < instantiator->d_func()->m_objects.size(); ++i) {
//        QNode *n = qobject_cast<QNode *>(instantiator->d_func()->m_objects.at(i));
//        if (!n)
//            continue;
//        QNode *clonedNode = QNode::clone(n);
//        clonedNode->setParent(this);
//        d_func()->m_objects.append(clonedNode);
//    }
//}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE

#include "moc_quick3dnodeinstantiator_p.cpp"
