/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
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

#include "qqmlinstantiator_p.h"
#include "qqmlinstantiator_p_p.h"
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlInfo>
#include <QtQml/QQmlError>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p.h>

QT_BEGIN_NAMESPACE

QQmlInstantiatorPrivate::QQmlInstantiatorPrivate()
    : componentComplete(true)
    , effectiveReset(false)
    , active(true)
    , async(false)
    , ownModel(false)
    , requestedIndex(-1)
    , model(QVariant(1))
    , instanceModel(0)
    , delegate(0)
{
}

QQmlInstantiatorPrivate::~QQmlInstantiatorPrivate()
{
    qDeleteAll(objects);
}

void QQmlInstantiatorPrivate::clear()
{
    Q_Q(QQmlInstantiator);
    if (!instanceModel)
        return;
    if (!objects.count())
        return;

    for (int i=0; i < objects.count(); i++) {
        q->objectRemoved(i, objects[i]);
        instanceModel->release(objects[i]);
    }
    objects.clear();
    q->objectChanged();
}

QObject *QQmlInstantiatorPrivate::modelObject(int index, bool async)
{
    requestedIndex = index;
    QObject *o = instanceModel->object(index, async);
    requestedIndex = -1;
    return o;
}


void QQmlInstantiatorPrivate::regenerate()
{
    Q_Q(QQmlInstantiator);
    if (!componentComplete)
        return;

    int prevCount = q->count();

    clear();

    if (!active || !instanceModel || !instanceModel->count() || !instanceModel->isValid()) {
        if (prevCount)
            q->countChanged();
        return;
    }

    for (int i = 0; i < instanceModel->count(); i++) {
        QObject *object = modelObject(i, async);
        // If the item was already created we won't get a createdItem
        if (object)
            _q_createdItem(i, object);
    }
    if (q->count() != prevCount)
        q->countChanged();
}

void QQmlInstantiatorPrivate::_q_createdItem(int idx, QObject* item)
{
    Q_Q(QQmlInstantiator);
    if (objects.contains(item)) //Case when it was created synchronously in regenerate
        return;
    if (requestedIndex != idx) // Asynchronous creation, reference the object
        (void)instanceModel->object(idx, false);
    item->setParent(q);
    if (objects.size() < idx + 1) {
        int modelCount = instanceModel->count();
        if (objects.capacity() < modelCount)
            objects.reserve(modelCount);
        objects.resize(idx + 1);
    }
    if (QObject *o = objects.at(idx))
        instanceModel->release(o);
    objects.replace(idx, item);
    if (objects.count() == 1)
        q->objectChanged();
    q->objectAdded(idx, item);
}

void QQmlInstantiatorPrivate::_q_modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_Q(QQmlInstantiator);

    if (!componentComplete || effectiveReset)
        return;

    if (reset) {
        regenerate();
        if (changeSet.difference() != 0)
            q->countChanged();
        return;
    }

    int difference = 0;
    QHash<int, QVector<QPointer<QObject> > > moved;
    foreach (const QQmlChangeSet::Change &remove, changeSet.removes()) {
        int index = qMin(remove.index, objects.count());
        int count = qMin(remove.index + remove.count, objects.count()) - index;
        if (remove.isMove()) {
            moved.insert(remove.moveId, objects.mid(index, count));
            objects.erase(
                    objects.begin() + index,
                    objects.begin() + index + count);
        } else while (count--) {
            QObject *obj = objects.at(index);
            objects.remove(index);
            q->objectRemoved(index, obj);
            if (obj)
                instanceModel->release(obj);
        }

        difference -= remove.count;
    }

    foreach (const QQmlChangeSet::Change &insert, changeSet.inserts()) {
        int index = qMin(insert.index, objects.count());
        if (insert.isMove()) {
            QVector<QPointer<QObject> > movedObjects = moved.value(insert.moveId);
            objects = objects.mid(0, index) + movedObjects + objects.mid(index);
        } else {
            if (insert.index <= objects.size())
                objects.insert(insert.index, insert.count, 0);
            for (int i = 0; i < insert.count; ++i) {
                int modelIndex = index + i;
                QObject* obj = modelObject(modelIndex, async);
                if (obj)
                    _q_createdItem(modelIndex, obj);
            }
        }
        difference += insert.count;
    }

    if (difference != 0)
        q->countChanged();
}

void QQmlInstantiatorPrivate::makeModel()
{
    Q_Q(QQmlInstantiator);
    QQmlDelegateModel* delegateModel = new QQmlDelegateModel(qmlContext(q), q);
    instanceModel = delegateModel;
    ownModel = true;
    delegateModel->setDelegate(delegate);
    delegateModel->classBegin(); //Pretend it was made in QML
    if (componentComplete)
        delegateModel->componentComplete();
}


/*!
    \qmltype Instantiator
    \instantiates QQmlInstantiator
    \inqmlmodule QtQml
    \brief Dynamically creates objects

    A Instantiator can be used to control the dynamic creation of objects, or to dynamically
    create multiple objects from a template.

    The Instantiator element will manage the objects it creates. Those objects are parented to the
    Instantiator and can also be deleted by the Instantiator if the Instantiator's properties change. Objects
    can also be destroyed dynamically through other means, and the Instantiator will not recreate
    them unless the properties of the Instantiator change.

*/
QQmlInstantiator::QQmlInstantiator(QObject *parent)
    : QObject(*(new QQmlInstantiatorPrivate), parent)
{
}

QQmlInstantiator::~QQmlInstantiator()
{
}

/*!
    \qmlsignal QtQml::Instantiator::objectAdded(int index, QtObject object)

    This signal is emitted when an object is added to the Instantiator. The \a index
    parameter holds the index which the object has been given, and the \a object
    parameter holds the \l QtObject that has been added.

    The corresponding handler is \c onObjectAdded.
*/

/*!
    \qmlsignal QtQml::Instantiator::objectRemoved(int index, QtObject object)

    This signal is emitted when an object is removed from the Instantiator. The \a index
    parameter holds the index which the object had been given, and the \a object
    parameter holds the \l QtObject that has been removed.

    Do not keep a reference to \a object if it was created by this Instantiator, as
    in these cases it will be deleted shortly after the signal is handled.

    The corresponding handler is \c onObjectRemoved.
*/
/*!
    \qmlproperty bool QtQml::Instantiator::active

    When active is true, and the delegate component is ready, the Instantiator will
    create objects according to the model. When active is false, no objects
    will be created and any previously created objects will be destroyed.

    Default is true.
*/
bool QQmlInstantiator::isActive() const
{
    Q_D(const QQmlInstantiator);
    return d->active;
}

void QQmlInstantiator::setActive(bool newVal)
{
    Q_D(QQmlInstantiator);
    if (newVal == d->active)
        return;
    d->active = newVal;
    emit activeChanged();
    d->regenerate();
}

/*!
    \qmlproperty bool QtQml::Instantiator::asynchronous

    When asynchronous is true the Instantiator will attempt to create objects
    asynchronously. This means that objects may not be available immediately,
    even if active is set to true.

    You can use the objectAdded signal to respond to items being created.

    Default is false.
*/
bool QQmlInstantiator::isAsync() const
{
    Q_D(const QQmlInstantiator);
    return d->async;
}

void QQmlInstantiator::setAsync(bool newVal)
{
    Q_D(QQmlInstantiator);
    if (newVal == d->async)
        return;
    d->async = newVal;
    emit asynchronousChanged();
}


/*!
    \qmlproperty int QtQml::Instantiator::count

    The number of objects the Instantiator is currently managing.
*/

int QQmlInstantiator::count() const
{
    Q_D(const QQmlInstantiator);
    return d->objects.count();
}

/*!
    \qmlproperty QtQml::Component QtQml::Instantiator::delegate
    \default

    The component used to create all objects.

    Note that an extra variable, index, will be available inside instances of the
    delegate. This variable refers to the index of the instance inside the Instantiator,
    and can be used to obtain the object through the objectAt method of the Instantiator.

    If this property is changed, all instances using the old delegate will be destroyed
    and new instances will be created using the new delegate.
*/
QQmlComponent* QQmlInstantiator::delegate()
{
    Q_D(QQmlInstantiator);
    return d->delegate;
}

void QQmlInstantiator::setDelegate(QQmlComponent* c)
{
    Q_D(QQmlInstantiator);
    if (c == d->delegate)
        return;

    d->delegate = c;
    emit delegateChanged();

    if (!d->ownModel)
        return;

    if (QQmlDelegateModel *dModel = qobject_cast<QQmlDelegateModel*>(d->instanceModel))
        dModel->setDelegate(c);
    if (d->componentComplete)
        d->regenerate();
}

/*!
    \qmlproperty variant QtQml::Instantiator::model

    This property can be set to any of the supported \l {qml-data-models}{data models}:

    \list
    \li A number that indicates the number of delegates to be created by the repeater
    \li A model (e.g. a ListModel item, or a QAbstractItemModel subclass)
    \li A string list
    \li An object list
    \endlist

    The type of model affects the properties that are exposed to the \l delegate.

    Default value is 1, which creates a single delegate instance.

    \sa {qml-data-models}{Data Models}
*/

QVariant QQmlInstantiator::model() const
{
    Q_D(const QQmlInstantiator);
    return d->model;
}

void QQmlInstantiator::setModel(const QVariant &v)
{
    Q_D(QQmlInstantiator);
    if (d->model == v)
        return;

    d->model = v;
    //Don't actually set model until componentComplete in case it wants to create its delegates immediately
    if (!d->componentComplete)
        return;

    QQmlInstanceModel *prevModel = d->instanceModel;
    QObject *object = qvariant_cast<QObject*>(v);
    QQmlInstanceModel *vim = 0;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        if (d->ownModel) {
            delete d->instanceModel;
            prevModel = 0;
            d->ownModel = false;
        }
        d->instanceModel = vim;
    } else if (v != QVariant(0)){
        if (!d->ownModel)
            d->makeModel();

        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel *>(d->instanceModel)) {
            d->effectiveReset = true;
            dataModel->setModel(v);
            d->effectiveReset = false;
        }
    }

    if (d->instanceModel != prevModel) {
        if (prevModel) {
            disconnect(prevModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                    this, SLOT(_q_modelUpdated(QQmlChangeSet,bool)));
            disconnect(prevModel, SIGNAL(createdItem(int,QObject*)), this, SLOT(_q_createdItem(int,QObject*)));
            //disconnect(prevModel, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
        }

        connect(d->instanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, SLOT(_q_modelUpdated(QQmlChangeSet,bool)));
        connect(d->instanceModel, SIGNAL(createdItem(int,QObject*)), this, SLOT(_q_createdItem(int,QObject*)));
        //connect(d->instanceModel, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
    }

    d->regenerate();
    emit modelChanged();
}

/*!
    \qmlproperty QtObject QtQml::Instantiator::object

    This is a reference to the first created object, intended as a convenience
    for the case where only one object has been created.
*/
QObject *QQmlInstantiator::object() const
{
    Q_D(const QQmlInstantiator);
    if (d->objects.count())
        return d->objects[0];
    return 0;
}

/*!
    \qmlmethod QtObject QtQml::Instantiator::objectAt(int index)

    Returns a reference to the object with the given \a index.
*/
QObject *QQmlInstantiator::objectAt(int index) const
{
    Q_D(const QQmlInstantiator);
    if (index >= 0 && index < d->objects.count())
        return d->objects[index];
    return 0;
}

/*!
 \internal
*/
void QQmlInstantiator::classBegin()
{
    Q_D(QQmlInstantiator);
    d->componentComplete = false;
}

/*!
 \internal
*/
void QQmlInstantiator::componentComplete()
{
    Q_D(QQmlInstantiator);
    d->componentComplete = true;
    if (d->ownModel) {
        static_cast<QQmlDelegateModel*>(d->instanceModel)->componentComplete();
        d->regenerate();
    } else {
        QVariant realModel = d->model;
        d->model = QVariant(0);
        setModel(realModel); //If realModel == d->model this won't do anything, but that's fine since the model's 0
        //setModel calls regenerate
    }
}

QT_END_NAMESPACE

#include "moc_qqmlinstantiator_p.cpp"
