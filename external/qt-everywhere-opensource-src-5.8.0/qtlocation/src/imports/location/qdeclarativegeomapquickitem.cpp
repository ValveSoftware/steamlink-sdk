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

#include "qdeclarativegeomapquickitem_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtQml/qqmlinfo.h>
#include <QtQuick/QSGOpacityNode>
#include "qdoublevector2d_p.h"
#include <QtQuick/private/qquickmousearea_p.h>

#include <QDebug>
#include <cmath>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapQuickItem
    \instantiates QDeclarativeGeoMapQuickItem
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.5

    \brief The MapQuickItem type displays an arbitrary Qt Quick object
           on a Map.

    The MapQuickItem type is used to place an arbitrary Qt Quick object
    on a Map at a specified location and size. Compared to floating an item
    above the Map, a MapQuickItem will follow the panning (and optionally, the
    zooming) of the Map as if it is on the Map surface.

    The \l{sourceItem} property contains the Qt Quick item to be drawn, which
    can be any kind of visible type.

    \section2 Positioning and Sizing

    The positioning of the MapQuickItem on the Map is controlled by two
    properties: \l coordinate and \l anchorPoint. If only \l coordinate is set,
    it specifies a longitude/latitude coordinate for the item to be placed at.
    The set coordinate will line up with the top-left corner of the contained
    item when shown on the screen.

    The \l anchorPoint property provides a way to line up the coordinate with
    other parts of the item than just the top-left corner, by setting a number
    of pixels the item will be offset by. A simple way to think about it is
    to note that the point given by \l anchorPoint on the item itself is the
    point that will line up with the given \l coordinate when displayed.

    In addition to being anchored to the map, the MapQuickItem can optionally
    follow the scale of the map, and change size when the Map is zoomed in or
    zoomed out. This behaviour is controlled by the \l zoomLevel property. The
    default behaviour if \l zoomLevel is not set is for the item to be drawn
    "on the screen" rather than "on the map", so that its size remains the same
    regardless of the zoom level of the Map.

    \section2 Performance

    Performance of a MapQuickItem is normally in the same ballpark as the
    contained Qt Quick item alone. Overheads added amount to a translation
    and (possibly) scaling of the original item, as well as a transformation
    from longitude and latitude to screen position.

    \section2 Limitations

    \note Due to an implementation detail, items placed inside a
    MapQuickItem will have a \c{parent} item which is not the MapQuickItem.
    Refer to the MapQuickItem by its \c{id}, and avoid the use of \c{anchor}
    in the \c{sourceItem}.

    \section2 Example Usage

    The following snippet shows a MapQuickItem containing an Image object,
    to display a Marker on the Map. This strategy is used to show the map
    markers in the MapViewer example.

    \snippet mapviewer/map/Marker.qml mqi-top
    \snippet mapviewer/map/Marker.qml mqi-anchor
    \snippet mapviewer/map/Marker.qml mqi-closeimage
    \snippet mapviewer/map/Marker.qml mqi-close

    \image api-mapquickitem.png
*/

QDeclarativeGeoMapQuickItem::QDeclarativeGeoMapQuickItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), zoomLevel_(0.0),
    mapAndSourceItemSet_(false), updatingGeometry_(false)
{
    setFlag(ItemHasContents, true);
    opacityContainer_ = new QQuickItem(this);
    opacityContainer_->setParentItem(this);
    opacityContainer_->setFlag(ItemHasContents, true);
}

QDeclarativeGeoMapQuickItem::~QDeclarativeGeoMapQuickItem() {}

/*!
    \qmlproperty coordinate MapQuickItem::coordinate

    This property holds the anchor coordinate of the MapQuickItem. The point
    on the sourceItem that is specified by anchorPoint is kept aligned with
    this coordinate when drawn on the map.

    In the image below, there are 3 MapQuickItems that are identical except
    for the value of their anchorPoint properties. The values of anchorPoint
    for each are written on top of the item.

    \image api-mapquickitem-anchor.png
*/
void QDeclarativeGeoMapQuickItem::setCoordinate(const QGeoCoordinate &coordinate)
{
    if (coordinate_ == coordinate)
        return;

    coordinate_ = coordinate;

    polishAndUpdate();
    emit coordinateChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMapQuickItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map && quickMap) {
        connect(map, SIGNAL(cameraDataChanged(QGeoCameraData)),
                this, SLOT(polishAndUpdate()));
        polishAndUpdate();
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMapQuickItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (!mapAndSourceItemSet_ || updatingGeometry_ ||
        newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QGeoCoordinate newCoordinate = map()->itemPositionToCoordinate(QDoubleVector2D(x(), y()) + (scaleFactor() * QDoubleVector2D(anchorPoint_)), false);
    if (newCoordinate.isValid())
        setCoordinate(newCoordinate);

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

/*!
    \internal
*/
QGeoCoordinate QDeclarativeGeoMapQuickItem::coordinate()
{
    return coordinate_;
}

/*!
    \qmlproperty object MapQuickItem::sourceItem

    This property holds the source item that will be drawn on the map.
*/
void QDeclarativeGeoMapQuickItem::setSourceItem(QQuickItem *sourceItem)
{
    if (sourceItem_.data() == sourceItem)
        return;
    sourceItem_ = sourceItem;

    polishAndUpdate();
    emit sourceItemChanged();
}

QQuickItem *QDeclarativeGeoMapQuickItem::sourceItem()
{
    return sourceItem_.data();
}

/*!
    \internal
*/
void QDeclarativeGeoMapQuickItem::afterChildrenChanged()
{
    QList<QQuickItem *> kids = childItems();
    if (kids.size() > 0) {
        bool printedWarning = false;
        foreach (QQuickItem *i, kids) {
            if (i->flags() & QQuickItem::ItemHasContents
                    && !qobject_cast<QQuickMouseArea *>(i)
                    && sourceItem_.data() != i
                    && opacityContainer_ != i) {
                if (!printedWarning) {
                    qmlInfo(this) << "Use the sourceItem property for the contained item, direct children are not supported";
                    printedWarning = true;
                }

                qmlInfo(i) << "deleting this child";
                i->deleteLater();
            }
        }
    }
}

/*!
    \qmlproperty QPointF MapQuickItem::anchorPoint

    This property determines which point on the sourceItem that will be lined
    up with the coordinate on the map.
*/
void QDeclarativeGeoMapQuickItem::setAnchorPoint(const QPointF &anchorPoint)
{
    if (anchorPoint == anchorPoint_)
        return;
    anchorPoint_ = anchorPoint;
    polishAndUpdate();
    emit anchorPointChanged();
}

QPointF QDeclarativeGeoMapQuickItem::anchorPoint() const
{
    return anchorPoint_;
}

/*!
    \qmlproperty real MapQuickItem::zoomLevel

    This property controls the scaling behaviour of the contents of the
    MapQuickItem. In particular, by setting this property it is possible
    to choose between objects that are drawn on the screen (and sized in
    screen pixels), and those drawn on the map surface (which change size
    with the zoom level of the map).

    The default value for this property is 0.0, which corresponds to drawing
    the object on the screen surface. If set to another value, the object will
    be drawn on the map surface instead. The value (if not zero) specifies the
    zoomLevel at which the object will be visible at a scale of 1:1 (ie, where
    object pixels and screen pixels are the same). At zoom levels lower than
    this, the object will appear smaller, and at higher zoom levels, appear
    larger. This is in contrast to when this property is set to zero, where
    the object remains the same size on the screen at all zoom levels.
*/
void QDeclarativeGeoMapQuickItem::setZoomLevel(qreal zoomLevel)
{
    if (zoomLevel == zoomLevel_)
        return;
    zoomLevel_ = zoomLevel;
    polishAndUpdate();
    emit zoomLevelChanged();
}

qreal QDeclarativeGeoMapQuickItem::zoomLevel() const
{
    return zoomLevel_;
}

/*!
    \internal
*/
void QDeclarativeGeoMapQuickItem::updatePolish()
{
    if (!quickMap() && sourceItem_) {
        mapAndSourceItemSet_ = false;
        sourceItem_.data()->setParentItem(0);
        return;
    }

    if (!quickMap() || !map() || !sourceItem_) {
        mapAndSourceItemSet_ = false;
        return;
    }

    if (!mapAndSourceItemSet_ && quickMap() && map() && sourceItem_) {
        mapAndSourceItemSet_ = true;
        sourceItem_.data()->setParentItem(opacityContainer_);
        sourceItem_.data()->setTransformOrigin(QQuickItem::TopLeft);
        connect(sourceItem_.data(), SIGNAL(xChanged()),
                this, SLOT(polishAndUpdate()));
        connect(sourceItem_.data(), SIGNAL(yChanged()),
                this, SLOT(polishAndUpdate()));
        connect(sourceItem_.data(), SIGNAL(widthChanged()),
                this, SLOT(polishAndUpdate()));
        connect(sourceItem_.data(), SIGNAL(heightChanged()),
                this, SLOT(polishAndUpdate()));
    }

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    opacityContainer_->setOpacity(zoomLevelOpacity());

    sourceItem_.data()->setScale(scaleFactor());
    sourceItem_.data()->setPosition(QPointF(0,0));
    setWidth(sourceItem_.data()->width());
    setHeight(sourceItem_.data()->height());
    setPositionOnMap(coordinate(), scaleFactor() * anchorPoint_);
}

/*!
    \internal
*/
void QDeclarativeGeoMapQuickItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
{
    Q_UNUSED(event);
}

/*!
    \internal
*/
qreal QDeclarativeGeoMapQuickItem::scaleFactor()
{
    qreal scale = 1.0;
    // use 1+x to avoid fuzzy compare against zero
    if (!qFuzzyCompare(1.0 + zoomLevel_, 1.0))
        scale = std::pow(0.5, zoomLevel_ - map()->cameraData().zoomLevel());
    return scale;
}

#include "moc_qdeclarativegeomapquickitem_p.cpp"

QT_END_NAMESPACE
