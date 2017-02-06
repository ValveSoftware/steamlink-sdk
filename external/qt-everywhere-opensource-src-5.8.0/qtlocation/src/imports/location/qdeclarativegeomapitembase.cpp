/****************************************************************************
**
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

#include "qdeclarativegeomapitembase_p.h"
#include "qgeocameradata_p.h"
#include <QtQml/QQmlInfo>
#include <QtQuick/QSGOpacityNode>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

QGeoMapViewportChangeEvent::QGeoMapViewportChangeEvent()
    : zoomLevelChanged(false),
      centerChanged(false),
      mapSizeChanged(false),
      tiltChanged(false),
      bearingChanged(false),
      rollChanged(false)
{
}

QGeoMapViewportChangeEvent::QGeoMapViewportChangeEvent(const QGeoMapViewportChangeEvent &other)
{
    this->operator=(other);
}

QGeoMapViewportChangeEvent &QGeoMapViewportChangeEvent::operator=(const QGeoMapViewportChangeEvent &other)
{
    if (this == &other)
        return (*this);

    cameraData = other.cameraData;
    mapSize = other.mapSize;
    zoomLevelChanged = other.zoomLevelChanged;
    centerChanged = other.centerChanged;
    mapSizeChanged = other.mapSizeChanged;
    tiltChanged = other.tiltChanged;
    bearingChanged = other.bearingChanged;
    rollChanged = other.rollChanged;

    return (*this);
}

QDeclarativeGeoMapItemBase::QDeclarativeGeoMapItemBase(QQuickItem *parent)
:   QQuickItem(parent), map_(0), quickMap_(0)
{
    setFiltersChildMouseEvents(true);
    connect(this, SIGNAL(childrenChanged()),
            this, SLOT(afterChildrenChanged()));
}

QDeclarativeGeoMapItemBase::~QDeclarativeGeoMapItemBase()
{
    disconnect(this, SLOT(afterChildrenChanged()));
    if (quickMap_)
        quickMap_->removeMapItem(this);
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemBase::afterChildrenChanged()
{
    QList<QQuickItem *> kids = childItems();
    if (kids.size() > 0) {
        bool printedWarning = false;
        foreach (QQuickItem *i, kids) {
            if (i->flags() & QQuickItem::ItemHasContents
                    && !qobject_cast<QQuickMouseArea *>(i)) {
                if (!printedWarning) {
                    qmlInfo(this) << "Geographic map items do not support child items";
                    printedWarning = true;
                }

                qmlInfo(i) << "deleting this child";
                i->deleteLater();
            }
        }
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemBase::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    if (quickMap == quickMap_)
        return;
    if (quickMap && quickMap_)
        return; // don't allow association to more than one map
    if (quickMap_)
        quickMap_->disconnect(this);
    if (map_)
        map_->disconnect(this);

    quickMap_ = quickMap;
    map_ = map;

    if (map_ && quickMap_) {
        connect(map_, SIGNAL(cameraDataChanged(QGeoCameraData)),
                this, SLOT(baseCameraDataChanged(QGeoCameraData)));
        connect(quickMap, SIGNAL(heightChanged()), this, SLOT(polishAndUpdate()));
        connect(quickMap, SIGNAL(widthChanged()), this, SLOT(polishAndUpdate()));
        lastSize_ = QSizeF(quickMap_->width(), quickMap_->height());
        lastCameraData_ = map_->cameraData();
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemBase::baseCameraDataChanged(const QGeoCameraData &cameraData)
{
    QGeoMapViewportChangeEvent evt;
    evt.cameraData = cameraData;
    evt.mapSize = QSizeF(quickMap_->width(), quickMap_->height());

    if (evt.mapSize != lastSize_)
        evt.mapSizeChanged = true;

    if (cameraData.bearing() != lastCameraData_.bearing())
        evt.bearingChanged = true;
    if (cameraData.center() != lastCameraData_.center())
        evt.centerChanged = true;
    if (cameraData.roll() != lastCameraData_.roll())
        evt.rollChanged = true;
    if (cameraData.tilt() != lastCameraData_.tilt())
        evt.tiltChanged = true;
    if (cameraData.zoomLevel() != lastCameraData_.zoomLevel())
        evt.zoomLevelChanged = true;

    lastSize_ = evt.mapSize;
    lastCameraData_ = cameraData;

    afterViewportChanged(evt);
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemBase::setPositionOnMap(const QGeoCoordinate &coordinate, const QPointF &offset)
{
    if (!map_ || !quickMap_)
        return;

    QPointF topLeft = map_->coordinateToItemPosition(coordinate, false).toPointF() - offset;

    setPosition(topLeft);
}

/*!
    \internal
*/
float QDeclarativeGeoMapItemBase::zoomLevelOpacity() const
{
    if (quickMap_->zoomLevel() > 3.0)
        return 1.0;
    else if (quickMap_->zoomLevel() > 2.0)
        return quickMap_->zoomLevel() - 2.0;
    else
        return 0.0;
}

bool QDeclarativeGeoMapItemBase::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_UNUSED(item)
    if (event->type() == QEvent::MouseButtonPress && !contains(static_cast<QMouseEvent*>(event)->pos())) {
        // This is an evil hack: in case of items that are not rectangles, we never accept the event.
        // Instead the events are now delivered to QDeclarativeGeoMapItemBase which doesn't to anything with them.
        // The map below it still works since it filters events and steals the events at some point.
        event->setAccepted(false);
        return true;
    }
    return false;
}

/*!
    \internal
*/
QSGNode *QDeclarativeGeoMapItemBase::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *pd)
{
    if (!map_ || !quickMap_) {
        delete oldNode;
        return 0;
    }

    QSGOpacityNode *opn = static_cast<QSGOpacityNode *>(oldNode);
    if (!opn)
        opn = new QSGOpacityNode();

    opn->setOpacity(zoomLevelOpacity());

    QSGNode *oldN = opn->childCount() ? opn->firstChild() : 0;
    opn->removeAllChildNodes();
    if (opn->opacity() > 0.0) {
        QSGNode *n = this->updateMapItemPaintNode(oldN, pd);
        if (n)
            opn->appendChildNode(n);
    } else {
        delete oldN;
    }

    return opn;
}

/*!
    \internal
*/
QSGNode *QDeclarativeGeoMapItemBase::updateMapItemPaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    return 0;
}

bool QDeclarativeGeoMapItemBase::isPolishScheduled() const
{
    return QQuickItemPrivate::get(this)->polishScheduled;
}

void QDeclarativeGeoMapItemBase::polishAndUpdate()
{
    polish();
    update();
}


#include "moc_qdeclarativegeomapitembase_p.cpp"

QT_END_NAMESPACE
