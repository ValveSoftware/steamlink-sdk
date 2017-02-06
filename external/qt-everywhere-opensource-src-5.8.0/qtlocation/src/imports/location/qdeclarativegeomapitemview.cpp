/****************************************************************************
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativegeomapitemview_p.h"
#include "qdeclarativegeomap_p.h"
#include "qdeclarativegeomapitembase_p.h"
#include "mapitemviewdelegateincubator.h"

#include <QtCore/QAbstractItemModel>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlIncubator>
#include <QtQml/private/qqmlopenmetaobject_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapItemView
    \instantiates QDeclarativeGeoMapItemView
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.5
    \inherits QObject

    \brief The MapItemView is used to populate Map from a model.

    The MapItemView is used to populate Map with MapItems from a model.
    The MapItemView type only makes sense when contained in a Map,
    meaning that it has no standalone presentation.

    \section2 Example Usage

    This example demonstrates how to use the MapViewItem object to display
    a \l{Route}{route} on a \l{Map}{map}:

    \snippet declarative/maps.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/maps.qml MapRoute
*/

QDeclarativeGeoMapItemView::QDeclarativeGeoMapItemView(QQuickItem *parent)
    : QObject(parent), componentCompleted_(false), delegate_(0),
      itemModel_(0), map_(0), fitViewport_(false), m_metaObjectType(0),
      m_readyIncubators(0), m_repopulating(false)
{
}

QDeclarativeGeoMapItemView::~QDeclarativeGeoMapItemView()
{
    removeInstantiatedItems();
    if (m_metaObjectType)
        m_metaObjectType->release();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::componentComplete()
{
    componentCompleted_ = true;
}

void QDeclarativeGeoMapItemView::incubatorStatusChanged(MapItemViewDelegateIncubator *incubator,
                                                        QQmlIncubator::Status status,
                                                        bool batched)
{
    if (status == QQmlIncubator::Loading)
        return;

    QDeclarativeGeoMapItemViewItemData *itemData = incubator->m_itemData;
    if (!itemData) {
        // Should never get here
        qWarning() << "MapItemViewDelegateIncubator incubating invalid itemData";
        return;
    }

    switch (status) {
    case QQmlIncubator::Ready:
        {
            QDeclarativeGeoMapItemBase *item = qobject_cast<QDeclarativeGeoMapItemBase *>(incubator->object());
            if (!item)
                break;
            itemData->item = item;
            if (!itemData->item) {
                qWarning() << "QDeclarativeGeoMapItemView map item delegate is of unsupported type.";
                delete incubator->object();
            } else {
                if (!batched) {
                    map_->addMapItem(itemData->item);
                    fitViewport();
                } else {
                    ++m_readyIncubators; // QSemaphore not needed as multiple threads not involved

                    if (m_readyIncubators == m_itemDataBatched.size()) {

                        // Clearing stuff older than the reset
                        foreach (QDeclarativeGeoMapItemViewItemData *i, m_itemData)
                            removeItemData(i);
                        m_itemData.clear();

                        // Adding everthing created after reset was issued
                        foreach (QDeclarativeGeoMapItemViewItemData *i, m_itemDataBatched) {
                            map_->addMapItem(i->item);
                        }
                        m_itemData = m_itemDataBatched;
                        m_itemDataBatched.clear();

                        m_readyIncubators = 0;
                        m_repopulating = false;

                        fitViewport();
                    }
                }
            }
            delete itemData->incubator;
            itemData->incubator = 0;
            break;
        }
    case QQmlIncubator::Null:
        // Should never get here
        delete itemData->incubator;
        itemData->incubator = 0;
        break;
    case QQmlIncubator::Error:
        qWarning() << "QDeclarativeGeoMapItemView map item creation failed.";
        delete itemData->incubator;
        itemData->incubator = 0;
        break;
    default:
        ;
    }
}

/*!
    \qmlproperty model QtLocation::MapItemView::model

    This property holds the model that provides data used for creating the map items defined by the
    delegate. Only QAbstractItemModel based models are supported.
*/
QVariant QDeclarativeGeoMapItemView::model() const
{
    return QVariant::fromValue(itemModel_);
}

void QDeclarativeGeoMapItemView::setModel(const QVariant &model)
{
    QAbstractItemModel *itemModel = model.value<QAbstractItemModel *>();
    if (itemModel == itemModel_)
        return;

    if (itemModel_) {
        disconnect(itemModel_, SIGNAL(modelReset()), this, SLOT(itemModelReset()));
        disconnect(itemModel_, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                   this, SLOT(itemModelRowsRemoved(QModelIndex,int,int)));
        disconnect(itemModel_, SIGNAL(rowsInserted(QModelIndex,int,int)),
                   this, SLOT(itemModelRowsInserted(QModelIndex,int,int)));
        disconnect(itemModel_, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                   this, SLOT(itemModelRowsMoved(QModelIndex,int,int,QModelIndex,int)));
        disconnect(itemModel_, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                   this, SLOT(itemModelDataChanged(QModelIndex,QModelIndex,QVector<int>)));

        removeInstantiatedItems(); // this also terminates ongong repopulations.
        m_metaObjectType->release();
        m_metaObjectType = 0;

        itemModel_ = 0;
    }

    if (itemModel) {
        itemModel_ = itemModel;
        connect(itemModel_, SIGNAL(modelReset()), this, SLOT(itemModelReset()));
        connect(itemModel_, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                this, SLOT(itemModelRowsRemoved(QModelIndex,int,int)));
        connect(itemModel_, SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(itemModelRowsInserted(QModelIndex,int,int)));
        connect(itemModel_, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                this, SLOT(itemModelRowsMoved(QModelIndex,int,int,QModelIndex,int)));
        connect(itemModel_, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                this, SLOT(itemModelDataChanged(QModelIndex,QModelIndex,QVector<int>)));

        m_metaObjectType = new QQmlOpenMetaObjectType(&QObject::staticMetaObject, 0);
        foreach (const QByteArray &name, itemModel_->roleNames())
            m_metaObjectType->createProperty(name);

        instantiateAllItems();
    }

    emit modelChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelReset()
{
    repopulate();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelRowsInserted(const QModelIndex &index, int start, int end)
{
    Q_UNUSED(index)

    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;

    for (int i = start; i <= end; ++i) {
        const QModelIndex insertedIndex = itemModel_->index(i, 0, index);
        // If ran inside a qquickwidget which forces incubators to be synchronous, this call won't happen
        // with m_repopulating == true while incubators from a model reset are still incubating.
        // Note that having the model in a different thread is not supported in general.
        createItemForIndex(insertedIndex, m_repopulating);
    }

    fitViewport();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelRowsRemoved(const QModelIndex &index, int start, int end)
{
    Q_UNUSED(index)

    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;

    for (int i = end; i >= start; --i) {
        if (m_repopulating) {
            QDeclarativeGeoMapItemViewItemData *itemData = m_itemDataBatched.takeAt(i);
            if (!itemData)
                continue;
            if (itemData->incubator) {
                if (itemData->incubator->isReady()) {
                    --m_readyIncubators;
                    delete itemData->incubator->object();
                }
                itemData->incubator->clear();
            }
            delete itemData;
        } else {
            QDeclarativeGeoMapItemViewItemData *itemData = m_itemData.takeAt(i);
            removeItemData(itemData);
        }
    }

    fitViewport();
}

void QDeclarativeGeoMapItemView::itemModelRowsMoved(const QModelIndex &parent, int start, int end,
                                                    const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    Q_UNUSED(destination)
    Q_UNUSED(row)

    qWarning() << "QDeclarativeGeoMapItemView does not support models that move rows.";
}

void QDeclarativeGeoMapItemView::itemModelDataChanged(const QModelIndex &topLeft,
                                                      const QModelIndex &bottomRight,
                                                      const QVector<int> &roles)
{
    Q_UNUSED(roles)

    if (!m_itemData.count() || (m_repopulating && !m_itemDataBatched.count()) )
        return;

    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
        const QModelIndex index = itemModel_->index(i, 0);
        QDeclarativeGeoMapItemViewItemData *itemData;
        if (m_repopulating)
            itemData= m_itemDataBatched.at(i);
        else
            itemData= m_itemData.at(i);

        QHashIterator<int, QByteArray> iterator(itemModel_->roleNames());
        while (iterator.hasNext()) {
            iterator.next();

            QVariant modelData = itemModel_->data(index, iterator.key());
            if (!modelData.isValid())
                continue;

            itemData->context->setContextProperty(QString::fromLatin1(iterator.value().constData()),
                                                  modelData);
            itemData->modelDataMeta->setValue(iterator.value(), modelData);
        }
    }
}

/*!
    \qmlproperty Component QtLocation::MapItemView::delegate

    This property holds the delegate which defines how each item in the
    model should be displayed. The Component must contain exactly one
    MapItem -derived object as the root object.
*/
QQmlComponent *QDeclarativeGeoMapItemView::delegate() const
{
    return delegate_;
}

void QDeclarativeGeoMapItemView::setDelegate(QQmlComponent *delegate)
{
    if (delegate_ == delegate)
        return;

    delegate_ = delegate;

    repopulate();
    emit delegateChanged();
}

/*!
    \qmlproperty Component QtLocation::MapItemView::autoFitViewport

    This property controls whether to automatically pan and zoom the viewport
    to display all map items when items are added or removed.

    Defaults to false.
*/
bool QDeclarativeGeoMapItemView::autoFitViewport() const
{
    return fitViewport_;
}

void QDeclarativeGeoMapItemView::setAutoFitViewport(const bool &fitViewport)
{
    if (fitViewport == fitViewport_)
        return;
    fitViewport_ = fitViewport;
    emit autoFitViewportChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::fitViewport()
{
    if (!map_ || !fitViewport_ || m_repopulating)
        return;

    if (map_->mapItems().size() > 0)
        map_->fitViewportToMapItems();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::setMap(QDeclarativeGeoMap *map)
{
    if (!map || map_) // changing map on the fly not supported
        return;
    map_ = map;
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::removeInstantiatedItems()
{
    if (!map_)
        return;

    terminateOngoingRepopulation();
    foreach (QDeclarativeGeoMapItemViewItemData *itemData, m_itemData)
        removeItemData(itemData);
    m_itemData.clear();
}

/*!
    \internal

    Instantiates all items.
*/
void QDeclarativeGeoMapItemView::instantiateAllItems()
{
    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;
    Q_ASSERT(!m_itemDataBatched.size());
    m_repopulating = true;

    // QQuickWidget forces incubators to synchronous mode. Thus itemDataChanged gets called during the for loop below.
    m_itemDataBatched.resize(itemModel_->rowCount());
    for (int i = 0; i < itemModel_->rowCount(); ++i) {
        const QModelIndex index = itemModel_->index(i, 0);
        createItemForIndex(index, true);
    }

    fitViewport();
}

void QDeclarativeGeoMapItemView::removeItemData(QDeclarativeGeoMapItemViewItemData *itemData)
{
    if (!itemData)
        return;
    if (itemData->incubator) {
        if (itemData->incubator->isReady()) {
            if (itemData->incubator->object() == itemData->item) {
                map_->removeMapItem(itemData->item); // removeMapItem checks whether the item is in the map, so it's safe to call.
                itemData->item = 0;
            }
            delete itemData->incubator->object();
        }
        itemData->incubator->clear(); // stops ongoing incubation
    }
    if (itemData->item)
        map_->removeMapItem(itemData->item);
    delete itemData; // destroys the ->item too.
}

void QDeclarativeGeoMapItemView::terminateOngoingRepopulation()
{
    if (m_repopulating) {
        // Terminate the previous resetting task. Not all incubators finished, but
        // QQmlIncubatorController operates in the same thread, so it is safe
        // to check, here, whether incubators are ready or not, without having
        // to race with them.

        foreach (QDeclarativeGeoMapItemViewItemData *itemData, m_itemDataBatched)
            removeItemData(itemData);

        m_itemDataBatched.clear();
        m_readyIncubators = 0;
        m_repopulating = false;
    }
}

/*!
    \internal
    Removes and repopulates all items.
*/
void QDeclarativeGeoMapItemView::repopulate()
{
    if (!itemModel_ || !itemModel_->rowCount()) {
        removeInstantiatedItems();
    } else {
        terminateOngoingRepopulation();
        instantiateAllItems(); // removal of instantiated item done at incubation completion
    }
}

/*!
    \internal

    Note: this call is async. that is returns to the event loop before returning to the caller.
    May also trigger incubatorStatusChanged() before returning to the caller if the incubator is fast enough.
*/
void QDeclarativeGeoMapItemView::createItemForIndex(const QModelIndex &index, bool batched)
{
    // Expected to be already tested by caller.
    Q_ASSERT(delegate_);
    Q_ASSERT(itemModel_);

    QDeclarativeGeoMapItemViewItemData *itemData = new QDeclarativeGeoMapItemViewItemData;

    itemData->modelData = new QObject;
    itemData->modelDataMeta = new QQmlOpenMetaObject(itemData->modelData, m_metaObjectType, false);
    itemData->context = new QQmlContext(qmlContext(this));

    QHashIterator<int, QByteArray> iterator(itemModel_->roleNames());
    while (iterator.hasNext()) {
        iterator.next();

        QVariant modelData = itemModel_->data(index, iterator.key());
        if (!modelData.isValid())
            continue;

        itemData->context->setContextProperty(QString::fromLatin1(iterator.value().constData()),
                                              modelData);

        itemData->modelDataMeta->setValue(iterator.value(), modelData);
    }

    itemData->context->setContextProperty(QLatin1String("model"), itemData->modelData);
    itemData->context->setContextProperty(QLatin1String("index"), index.row());

    if (batched || m_repopulating) {
        if (index.row() < m_itemDataBatched.size())
            m_itemDataBatched.replace(index.row(), itemData);
        else
            m_itemDataBatched.insert(index.row(), itemData);
    } else
        m_itemData.insert(index.row(), itemData);
    itemData->incubator = new MapItemViewDelegateIncubator(this, itemData, batched || m_repopulating);

    delegate_->create(*itemData->incubator, itemData->context);
}

QDeclarativeGeoMapItemViewItemData::~QDeclarativeGeoMapItemViewItemData()
{
    delete incubator;
    delete item;
    delete context;
    delete modelData;
}

#include "moc_qdeclarativegeomapitemview_p.cpp"

QT_END_NAMESPACE
