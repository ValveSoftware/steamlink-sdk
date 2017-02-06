/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickrepeater_p.h"
#include "qquickrepeater_p_p.h"

#include <private/qqmlglobal_p.h>
#include <private/qqmllistaccessor_p.h>
#include <private/qqmlchangeset_p.h>
#include <private/qqmldelegatemodel_p.h>

#include <QtQml/QQmlInfo>

QT_BEGIN_NAMESPACE

QQuickRepeaterPrivate::QQuickRepeaterPrivate()
    : model(0)
    , ownModel(false)
    , dataSourceIsObject(false)
    , delegateValidated(false)
    , itemCount(0)
{
    setTransparentForPositioner(true);
}

QQuickRepeaterPrivate::~QQuickRepeaterPrivate()
{
    if (ownModel)
        delete model;
}

/*!
    \qmltype Repeater
    \instantiates QQuickRepeater
    \inqmlmodule QtQuick
    \ingroup qtquick-models
    \ingroup qtquick-positioning
    \inherits Item
    \brief Instantiates a number of Item-based components using a provided model

    The Repeater type is used to create a large number of
    similar items. Like other view types, a Repeater has a \l model and a \l delegate:
    for each entry in the model, the delegate is instantiated
    in a context seeded with data from the model. A Repeater item is usually
    enclosed in a positioner type such as \l Row or \l Column to visually
    position the multiple delegate items created by the Repeater.

    The following Repeater creates three instances of a \l Rectangle item within
    a \l Row:

    \snippet qml/repeaters/repeater.qml import
    \codeline
    \snippet qml/repeaters/repeater.qml simple

    \image repeater-simple.png

    A Repeater's \l model can be any of the supported \l {qml-data-models}{data models}.
    Additionally, like delegates for other views, a Repeater delegate can access
    its index within the repeater, as well as the model data relevant to the
    delegate. See the \l delegate property documentation for details.

    Items instantiated by the Repeater are inserted, in order, as
    children of the Repeater's parent.  The insertion starts immediately after
    the repeater's position in its parent stacking list.  This allows
    a Repeater to be used inside a layout. For example, the following Repeater's
    items are stacked between a red rectangle and a blue rectangle:

    \snippet qml/repeaters/repeater.qml layout

    \image repeater.png


    \note A Repeater item owns all items it instantiates. Removing or dynamically destroying
    an item created by a Repeater results in unpredictable behavior.


    \section2 Considerations when using Repeater

    The Repeater type creates all of its delegate items when the repeater is first
    created. This can be inefficient if there are a large number of delegate items and
    not all of the items are required to be visible at the same time. If this is the case,
    consider using other view types like ListView (which only creates delegate items
    when they are scrolled into view) or use the \l {Dynamic Object Creation} methods to
    create items as they are required.

    Also, note that Repeater is \l {Item}-based, and can only repeat \l {Item}-derived objects.
    For example, it cannot be used to repeat QtObjects:
    \code
    //bad code
    Item {
        Can't repeat QtObject as it doesn't derive from Item.
        Repeater {
            model: 10
            QtObject {}
        }
    }
    \endcode
 */

/*!
    \qmlsignal QtQuick::Repeater::itemAdded(int index, Item item)

    This signal is emitted when an item is added to the repeater. The \a index
    parameter holds the index at which the item has been inserted within the
    repeater, and the \a item parameter holds the \l Item that has been added.

    The corresponding handler is \c onItemAdded.
*/

/*!
    \qmlsignal QtQuick::Repeater::itemRemoved(int index, Item item)

    This signal is emitted when an item is removed from the repeater. The \a index
    parameter holds the index at which the item was removed from the repeater,
    and the \a item parameter holds the \l Item that was removed.

    Do not keep a reference to \a item if it was created by this repeater, as
    in these cases it will be deleted shortly after the signal is handled.

    The corresponding handler is \c onItemRemoved.
*/
QQuickRepeater::QQuickRepeater(QQuickItem *parent)
  : QQuickItem(*(new QQuickRepeaterPrivate), parent)
{
}

QQuickRepeater::~QQuickRepeater()
{
}

/*!
    \qmlproperty any QtQuick::Repeater::model

    The model providing data for the repeater.

    This property can be set to any of the supported \l {qml-data-models}{data models}:

    \list
    \li A number that indicates the number of delegates to be created by the repeater
    \li A model (e.g. a ListModel item, or a QAbstractItemModel subclass)
    \li A string list
    \li An object list
    \endlist

    The type of model affects the properties that are exposed to the \l delegate.

    \sa {qml-data-models}{Data Models}
*/
QVariant QQuickRepeater::model() const
{
    Q_D(const QQuickRepeater);

    if (d->dataSourceIsObject) {
        QObject *o = d->dataSourceAsObject;
        return QVariant::fromValue(o);
    }

    return d->dataSource;
}

void QQuickRepeater::setModel(const QVariant &m)
{
    Q_D(QQuickRepeater);
    QVariant model = m;
    if (model.userType() == qMetaTypeId<QJSValue>())
        model = model.value<QJSValue>().toVariant();

    if (d->dataSource == model)
        return;

    clear();
    if (d->model) {
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, QQuickRepeater, SLOT(modelUpdated(QQmlChangeSet,bool)));
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(createdItem(int,QObject*)),
                this, QQuickRepeater, SLOT(createdItem(int,QObject*)));
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(initItem(int,QObject*)),
                this, QQuickRepeater, SLOT(initItem(int,QObject*)));
    }
    d->dataSource = model;
    QObject *object = qvariant_cast<QObject*>(model);
    d->dataSourceAsObject = object;
    d->dataSourceIsObject = object != 0;
    QQmlInstanceModel *vim = 0;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        if (d->ownModel) {
            delete d->model;
            d->ownModel = false;
        }
        d->model = vim;
    } else {
        if (!d->ownModel) {
            d->model = new QQmlDelegateModel(qmlContext(this));
            d->ownModel = true;
            if (isComponentComplete())
                static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
        }
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            dataModel->setModel(model);
    }
    if (d->model) {
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, QQuickRepeater, SLOT(modelUpdated(QQmlChangeSet,bool)));
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(createdItem(int,QObject*)),
                this, QQuickRepeater, SLOT(createdItem(int,QObject*)));
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(initItem(int,QObject*)),
                this, QQuickRepeater, SLOT(initItem(int,QObject*)));
        regenerate();
    }
    emit modelChanged();
    emit countChanged();
}

/*!
    \qmlproperty Component QtQuick::Repeater::delegate
    \default

    The delegate provides a template defining each item instantiated by the repeater.

    Delegates are exposed to a read-only \c index property that indicates the index
    of the delegate within the repeater. For example, the following \l Text delegate
    displays the index of each repeated item:

    \table
    \row
    \li \snippet qml/repeaters/repeater.qml index
    \li \image repeater-index.png
    \endtable

    If the \l model is a \l{QStringList-based model}{string list} or
    \l{QObjectList-based model}{object list}, the delegate is also exposed to
    a read-only \c modelData property that holds the string or object data. For
    example:

    \table
    \row
    \li \snippet qml/repeaters/repeater.qml modeldata
    \li \image repeater-modeldata.png
    \endtable

    If the \l model is a model object (such as a \l ListModel) the delegate
    can access all model roles as named properties, in the same way that delegates
    do for view classes like ListView.

    \sa {QML Data Models}
 */
QQmlComponent *QQuickRepeater::delegate() const
{
    Q_D(const QQuickRepeater);
    if (d->model) {
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            return dataModel->delegate();
    }

    return 0;
}

void QQuickRepeater::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickRepeater);
    if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
       if (delegate == dataModel->delegate())
           return;

    if (!d->ownModel) {
        d->model = new QQmlDelegateModel(qmlContext(this));
        d->ownModel = true;
    }

    if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model)) {
        dataModel->setDelegate(delegate);
        regenerate();
        emit delegateChanged();
        d->delegateValidated = false;
    }
}

/*!
    \qmlproperty int QtQuick::Repeater::count

    This property holds the number of items in the repeater.
*/
int QQuickRepeater::count() const
{
    Q_D(const QQuickRepeater);
    if (d->model)
        return d->model->count();
    return 0;
}

/*!
    \qmlmethod Item QtQuick::Repeater::itemAt(index)

    Returns the \l Item that has been created at the given \a index, or \c null
    if no item exists at \a index.
*/
QQuickItem *QQuickRepeater::itemAt(int index) const
{
    Q_D(const QQuickRepeater);
    if (index >= 0 && index < d->deletables.count())
        return d->deletables[index];
    return 0;
}

void QQuickRepeater::componentComplete()
{
    Q_D(QQuickRepeater);
    if (d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    QQuickItem::componentComplete();
    regenerate();
    if (d->model && d->model->count())
        emit countChanged();
}

void QQuickRepeater::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemParentHasChanged) {
        regenerate();
    }
}

void QQuickRepeater::clear()
{
    Q_D(QQuickRepeater);
    bool complete = isComponentComplete();

    if (d->model) {
        // We remove in reverse order deliberately; so that signals are emitted
        // with sensible indices.
        for (int i = d->deletables.count() - 1; i >= 0; --i) {
            if (QQuickItem *item = d->deletables.at(i)) {
                if (complete)
                    emit itemRemoved(i, item);
                d->model->release(item);
                item->setParentItem(0);
            }
        }
    }
    d->deletables.clear();
    d->itemCount = 0;
}

void QQuickRepeater::regenerate()
{
    Q_D(QQuickRepeater);
    if (!isComponentComplete())
        return;

    clear();

    if (!d->model || !d->model->count() || !d->model->isValid() || !parentItem() || !isComponentComplete())
        return;

    d->itemCount = count();
    d->deletables.resize(d->itemCount);
    d->requestItems();
}

void QQuickRepeaterPrivate::requestItems()
{
    for (int i = 0; i < itemCount; i++) {
        QObject *object = model->object(i, false);
        if (object)
            model->release(object);
    }
}

void QQuickRepeater::createdItem(int index, QObject *)
{
    Q_D(QQuickRepeater);
    QObject *object = d->model->object(index, false);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    emit itemAdded(index, item);
}

void QQuickRepeater::initItem(int index, QObject *object)
{
    Q_D(QQuickRepeater);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);

    if (!d->deletables.at(index)) {
        if (!item) {
            if (object) {
                d->model->release(object);
                if (!d->delegateValidated) {
                    d->delegateValidated = true;
                    QObject* delegate = this->delegate();
                    qmlInfo(delegate ? delegate : this) << QQuickRepeater::tr("Delegate must be of Item type");
                }
            }
            return;
        }
        d->deletables[index] = item;
        item->setParentItem(parentItem());
        if (index > 0 && d->deletables.at(index-1)) {
            item->stackAfter(d->deletables.at(index-1));
        } else {
            QQuickItem *after = this;
            for (int si = index+1; si < d->itemCount; ++si) {
                if (d->deletables.at(si)) {
                    after = d->deletables.at(si);
                    break;
                }
            }
            item->stackBefore(after);
        }
    }
}

void QQuickRepeater::modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_D(QQuickRepeater);

    if (!isComponentComplete())
        return;

    if (reset) {
        regenerate();
        if (changeSet.difference() != 0)
            emit countChanged();
        return;
    }

    int difference = 0;
    QHash<int, QVector<QPointer<QQuickItem> > > moved;
    for (const QQmlChangeSet::Change &remove : changeSet.removes()) {
        int index = qMin(remove.index, d->deletables.count());
        int count = qMin(remove.index + remove.count, d->deletables.count()) - index;
        if (remove.isMove()) {
            moved.insert(remove.moveId, d->deletables.mid(index, count));
            d->deletables.erase(
                    d->deletables.begin() + index,
                    d->deletables.begin() + index + count);
        } else while (count--) {
            QQuickItem *item = d->deletables.at(index);
            d->deletables.remove(index);
            emit itemRemoved(index, item);
            if (item) {
                d->model->release(item);
                item->setParentItem(0);
            }
            --d->itemCount;
        }

        difference -= remove.count;
    }

    for (const QQmlChangeSet::Change &insert : changeSet.inserts()) {
        int index = qMin(insert.index, d->deletables.count());
        if (insert.isMove()) {
            QVector<QPointer<QQuickItem> > items = moved.value(insert.moveId);
            d->deletables = d->deletables.mid(0, index) + items + d->deletables.mid(index);
            QQuickItem *stackBefore = index + items.count() < d->deletables.count()
                    ? d->deletables.at(index + items.count())
                    : this;
            for (int i = index; i < index + items.count(); ++i)
                d->deletables.at(i)->stackBefore(stackBefore);
        } else for (int i = 0; i < insert.count; ++i) {
            int modelIndex = index + i;
            ++d->itemCount;
            d->deletables.insert(modelIndex, 0);
            QObject *object = d->model->object(modelIndex, false);
            if (object)
                d->model->release(object);
        }
        difference += insert.count;
    }

    if (difference != 0)
        emit countChanged();
}

QT_END_NAMESPACE
