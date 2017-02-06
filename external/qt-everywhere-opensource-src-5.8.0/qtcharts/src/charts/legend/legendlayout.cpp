/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/legendlayout_p.h>
#include <private/chartpresenter_p.h>
#include <private/qlegend_p.h>
#include <private/abstractchartlayout_p.h>

#include <private/qlegendmarker_p.h>
#include <private/legendmarkeritem_p.h>
#include <QtCharts/QLegendMarker>

QT_CHARTS_BEGIN_NAMESPACE

LegendLayout::LegendLayout(QLegend *legend)
    : m_legend(legend),
      m_offsetX(0),
      m_offsetY(0)
{

}

LegendLayout::~LegendLayout()
{

}

void LegendLayout::setOffset(qreal x, qreal y)
{
    bool scrollHorizontal = true;
    switch (m_legend->alignment()) {
    case Qt::AlignTop:
    case Qt::AlignBottom:
        scrollHorizontal = true;
        break;
    case Qt::AlignLeft:
    case Qt::AlignRight:
        scrollHorizontal = false;
        break;
    }

    // If detached, the scrolling direction is vertical instead of horizontal and vice versa.
    if (!m_legend->isAttachedToChart())
        scrollHorizontal = !scrollHorizontal;

    QRectF boundingRect = geometry();
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    boundingRect.adjust(left, top, -right, -bottom);

    // Limit offset between m_minOffset and m_maxOffset
    if (scrollHorizontal) {
        if (m_width <= boundingRect.width())
            return;

        if (x != m_offsetX) {
            m_offsetX = qBound(m_minOffsetX, x, m_maxOffsetX);
            m_legend->d_ptr->items()->setPos(-m_offsetX, boundingRect.top());
        }
    } else {
        if (m_height <= boundingRect.height())
            return;

        if (y != m_offsetY) {
            m_offsetY = qBound(m_minOffsetY, y, m_maxOffsetY);
            m_legend->d_ptr->items()->setPos(boundingRect.left(), -m_offsetY);
        }
    }
}

QPointF LegendLayout::offset() const
{
    return QPointF(m_offsetX, m_offsetY);
}

void LegendLayout::invalidate()
{
    QGraphicsLayout::invalidate();
    if (m_legend->isAttachedToChart())
        m_legend->d_ptr->m_presenter->layout()->invalidate();
}

void LegendLayout::setGeometry(const QRectF &rect)
{
    m_legend->d_ptr->items()->setVisible(m_legend->isVisible());

    QGraphicsLayout::setGeometry(rect);

    if (m_legend->isAttachedToChart())
        setAttachedGeometry(rect);
    else
        setDettachedGeometry(rect);
}

void LegendLayout::setAttachedGeometry(const QRectF &rect)
{
    if (!rect.isValid())
        return;

    qreal oldOffsetX = m_offsetX;
    qreal oldOffsetY = m_offsetY;
    m_offsetX = 0;
    m_offsetY = 0;

    QSizeF size(0, 0);

    if (m_legend->d_ptr->markers().isEmpty()) {
        return;
    }

    m_width = 0;
    m_height = 0;

    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    QRectF geometry = rect.adjusted(left, top, -right, -bottom);

    switch(m_legend->alignment()) {
    case Qt::AlignTop:
    case Qt::AlignBottom: {
            // Calculate the space required for items and add them to a sorted list.
            qreal markerItemsWidth = 0;
            qreal itemMargins = 0;
            QList<LegendWidthStruct *> legendWidthList;
            foreach (QLegendMarker *marker, m_legend->d_ptr->markers()) {
                LegendMarkerItem *item = marker->d_ptr->item();
                if (item->isVisible()) {
                    QSizeF dummySize;
                    qreal itemWidth = item->sizeHint(Qt::PreferredSize, dummySize).width();
                    LegendWidthStruct *structItem = new LegendWidthStruct;
                    structItem->item = item;
                    structItem->width = itemWidth;
                    legendWidthList.append(structItem);
                    markerItemsWidth += itemWidth;
                    itemMargins += marker->d_ptr->item()->m_margin;
                }
            }
            std::sort(legendWidthList.begin(), legendWidthList.end(), widthLongerThan);

            // If the items would occupy more space than is available, start truncating them
            // from the longest one.
            qreal availableGeometry = geometry.width() - right - left * 2 - itemMargins;
            if (markerItemsWidth >= availableGeometry && legendWidthList.count() > 0) {
                bool truncated(false);
                int count = legendWidthList.count();
                for (int i = 1; i < count; i++) {
                    int truncateIndex = i - 1;

                    while (legendWidthList.at(truncateIndex)->width >= legendWidthList.at(i)->width
                           && !truncated) {
                        legendWidthList.at(truncateIndex)->width--;
                        markerItemsWidth--;
                        if (i > 1) {
                            // Truncate the items that are before the truncated one in the list.
                            for (int j = truncateIndex - 1; j >= 0; j--) {
                                if (legendWidthList.at(truncateIndex)->width
                                        < legendWidthList.at(j)->width) {
                                    legendWidthList.at(j)->width--;
                                    markerItemsWidth--;
                                }
                            }
                        }
                        if (markerItemsWidth < availableGeometry)
                            truncated = true;
                    }
                    // Truncate the last item if needed.
                    if (i == count - 1) {
                        if (legendWidthList.at(count - 1)->width
                                > legendWidthList.at(truncateIndex)->width) {
                            legendWidthList.at(count - 1)->width--;
                            markerItemsWidth--;
                        }
                    }

                    if (truncated)
                        break;
                }
                // Items are of same width and all of them need to be truncated
                // or there is just one item that is truncated.
                while (markerItemsWidth >= availableGeometry) {
                    for (int i = 0; i < count; i++) {
                        legendWidthList.at(i)->width--;
                        markerItemsWidth--;
                    }
                }
            }

            QPointF point(0,0);

            int markerCount = m_legend->d_ptr->markers().count();
            for (int i = 0; i < markerCount; i++) {
                QLegendMarker *marker;
                if (m_legend->d_ptr->m_reverseMarkers)
                    marker = m_legend->d_ptr->markers().at(markerCount - 1 - i);
                else
                    marker = m_legend->d_ptr->markers().at(i);
                LegendMarkerItem *item = marker->d_ptr->item();
                if (item->isVisible()) {
                    QRectF itemRect = geometry;
                    qreal availableWidth = 0;
                    for (int i = 0; i < legendWidthList.size(); ++i) {
                        if (legendWidthList.at(i)->item == item) {
                            availableWidth = legendWidthList.at(i)->width;
                            break;
                        }
                    }
                    itemRect.setWidth(availableWidth);
                    item->setGeometry(itemRect);
                    item->setPos(point.x(),geometry.height()/2 - item->boundingRect().height()/2);
                    const QRectF &rect = item->boundingRect();
                    size = size.expandedTo(rect.size());
                    qreal w = rect.width();
                    m_width = m_width + w - item->m_margin;
                    point.setX(point.x() + w);
                }
            }
            // Delete structs from the container
            qDeleteAll(legendWidthList);

            // Round to full pixel via QPoint to avoid one pixel clipping on the edge in some cases
            if (m_width < geometry.width()) {
                m_legend->d_ptr->items()->setPos(QPoint(geometry.width() / 2 - m_width / 2,
                                                        geometry.top()));
            } else {
                m_legend->d_ptr->items()->setPos(geometry.topLeft().toPoint());
            }
            m_height = size.height();
        }
        break;
    case Qt::AlignLeft:
    case Qt::AlignRight: {
            QPointF point(0,0);
            int markerCount = m_legend->d_ptr->markers().count();
            for (int i = 0; i < markerCount; i++) {
                QLegendMarker *marker;
                if (m_legend->d_ptr->m_reverseMarkers)
                    marker = m_legend->d_ptr->markers().at(markerCount - 1 - i);
                else
                    marker = m_legend->d_ptr->markers().at(i);
                LegendMarkerItem *item = marker->d_ptr->item();
                if (item->isVisible()) {
                    item->setGeometry(geometry);
                    item->setPos(point);
                    const QRectF &rect = item->boundingRect();
                    qreal h = rect.height();
                    size = size.expandedTo(rect.size());
                    m_height+=h;
                    point.setY(point.y() + h);
                }
            }

            // Round to full pixel via QPoint to avoid one pixel clipping on the edge in some cases
            if (m_height < geometry.height()) {
                m_legend->d_ptr->items()->setPos(QPoint(geometry.left(),
                                                        geometry.height() / 2 - m_height / 2));
            } else {
                m_legend->d_ptr->items()->setPos(geometry.topLeft().toPoint());
            }
            m_width = size.width();
            break;
            }
        }

    m_minOffsetX = -left;
    m_minOffsetY = - top;
    m_maxOffsetX = m_width - geometry.width() - right;
    m_maxOffsetY = m_height - geometry.height() - bottom;

    setOffset(oldOffsetX, oldOffsetY);
}

void LegendLayout::setDettachedGeometry(const QRectF &rect)
{
    if (!rect.isValid())
        return;

    // Detached layout is different.
    // In detached mode legend may have multiple rows and columns, so layout calculations
    // differ a log from attached mode.
    // Also the scrolling logic is bit different.

    qreal oldOffsetX = m_offsetX;
    qreal oldOffsetY = m_offsetY;
    m_offsetX = 0;
    m_offsetY = 0;

    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRectF geometry = rect.adjusted(left, top, -right, -bottom);

    QSizeF size(0, 0);

    QList<QLegendMarker *> markers = m_legend->d_ptr->markers();

    if (markers.isEmpty())
        return;

    switch (m_legend->alignment()) {
    case Qt::AlignTop: {
        QPointF point(0, 0);
        m_width = 0;
        m_height = 0;
        for (int i = 0; i < markers.count(); i++) {
            LegendMarkerItem *item = markers.at(i)->d_ptr->item();
            if (item->isVisible()) {
                item->setGeometry(geometry);
                item->setPos(point.x(),point.y());
                const QRectF &boundingRect = item->boundingRect();
                qreal w = boundingRect.width();
                qreal h = boundingRect.height();
                m_width = qMax(m_width,w);
                m_height = qMax(m_height,h);
                point.setX(point.x() + w);
                if (point.x() + w > geometry.left() + geometry.width() - right) {
                    // Next item would go off rect.
                    point.setX(0);
                    point.setY(point.y() + h);
                    if (i+1 < markers.count()) {
                        m_height += h;
                    }
                }
            }
        }
        m_legend->d_ptr->items()->setPos(geometry.topLeft());

        m_minOffsetX = -left;
        m_minOffsetY = -top;
        m_maxOffsetX = m_width - geometry.width() - right;
        m_maxOffsetY = m_height - geometry.height() - bottom;
    }
    break;
    case Qt::AlignBottom: {
        QPointF point(0, geometry.height());
        m_width = 0;
        m_height = 0;
        for (int i = 0; i < markers.count(); i++) {
            LegendMarkerItem *item = markers.at(i)->d_ptr->item();
            if (item->isVisible()) {
                item->setGeometry(geometry);
                const QRectF &boundingRect = item->boundingRect();
                qreal w = boundingRect.width();
                qreal h = boundingRect.height();
                m_width = qMax(m_width,w);
                m_height = qMax(m_height,h);
                item->setPos(point.x(),point.y() - h);
                point.setX(point.x() + w);
                if (point.x() + w > geometry.left() + geometry.width() - right) {
                    // Next item would go off rect.
                    point.setX(0);
                    point.setY(point.y() - h);
                    if (i+1 < markers.count()) {
                        m_height += h;
                    }
                }
            }
        }
        m_legend->d_ptr->items()->setPos(geometry.topLeft());

        m_minOffsetX = -left;
        m_minOffsetY = -m_height + geometry.height() - top;
        m_maxOffsetX = m_width - geometry.width() - right;
        m_maxOffsetY = -bottom;
    }
    break;
    case Qt::AlignLeft: {
        QPointF point(0, 0);
        m_width = 0;
        m_height = 0;
        qreal maxWidth = 0;
        for (int i = 0; i < markers.count(); i++) {
            LegendMarkerItem *item = markers.at(i)->d_ptr->item();
            if (item->isVisible()) {
                item->setGeometry(geometry);
                const QRectF &boundingRect = item->boundingRect();
                qreal w = boundingRect.width();
                qreal h = boundingRect.height();
                m_height = qMax(m_height,h);
                maxWidth = qMax(maxWidth,w);
                item->setPos(point.x(),point.y());
                point.setY(point.y() + h);
                if (point.y() + h > geometry.bottom() - bottom) {
                    // Next item would go off rect.
                    point.setX(point.x() + maxWidth);
                    point.setY(0);
                    if (i+1 < markers.count()) {
                        m_width += maxWidth;
                        maxWidth = 0;
                    }
                }
            }
        }
        m_width += maxWidth;
        m_legend->d_ptr->items()->setPos(geometry.topLeft());

        m_minOffsetX = -left;
        m_minOffsetY = -top;
        m_maxOffsetX = m_width - geometry.width() - right;
        m_maxOffsetY = m_height - geometry.height() - bottom;
    }
    break;
    case Qt::AlignRight: {
        QPointF point(geometry.width(), 0);
        m_width = 0;
        m_height = 0;
        qreal maxWidth = 0;
        for (int i = 0; i < markers.count(); i++) {
            LegendMarkerItem *item = markers.at(i)->d_ptr->item();
            if (item->isVisible()) {
                item->setGeometry(geometry);
                const QRectF &boundingRect = item->boundingRect();
                qreal w = boundingRect.width();
                qreal h = boundingRect.height();
                m_height = qMax(m_height,h);
                maxWidth = qMax(maxWidth,w);
                item->setPos(point.x() - w,point.y());
                point.setY(point.y() + h);
                if (point.y() + h > geometry.bottom()-bottom) {
                    // Next item would go off rect.
                    point.setX(point.x() - maxWidth);
                    point.setY(0);
                    if (i+1 < markers.count()) {
                        m_width += maxWidth;
                        maxWidth = 0;
                    }
                }
            }
        }
        m_width += maxWidth;
        m_legend->d_ptr->items()->setPos(geometry.topLeft());

        m_minOffsetX = - m_width + geometry.width() - left;
        m_minOffsetY = -top;
        m_maxOffsetX = - right;
        m_maxOffsetY = m_height - geometry.height() - bottom;
    }
    break;
    default:
        break;
    }

    setOffset(oldOffsetX, oldOffsetY);
}

QSizeF LegendLayout::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    QSizeF size(0, 0);
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    if(constraint.isValid()) {
        foreach(QLegendMarker *marker, m_legend->d_ptr->markers()) {
            LegendMarkerItem *item = marker->d_ptr->item();
            size = size.expandedTo(item->effectiveSizeHint(which));
        }
        size = size.boundedTo(constraint);
    }
    else if (constraint.width() >= 0) {
        qreal width = 0;
        qreal height = 0;
        foreach(QLegendMarker *marker, m_legend->d_ptr->markers()) {
            LegendMarkerItem *item = marker->d_ptr->item();
            width+=item->effectiveSizeHint(which).width();
            height=qMax(height,item->effectiveSizeHint(which).height());
        }

        size = QSizeF(qMin(constraint.width(),width), height);
    }
    else if (constraint.height() >= 0) {
        qreal width = 0;
        qreal height = 0;
        foreach(QLegendMarker *marker, m_legend->d_ptr->markers()) {
            LegendMarkerItem *item = marker->d_ptr->item();
            width=qMax(width,item->effectiveSizeHint(which).width());
            height+=height,item->effectiveSizeHint(which).height();
        }
        size = QSizeF(width,qMin(constraint.height(),height));
    }
    else {
        foreach(QLegendMarker *marker, m_legend->d_ptr->markers()) {
            LegendMarkerItem *item = marker->d_ptr->item();
            size = size.expandedTo(item->effectiveSizeHint(which));
        }
    }
    size += QSize(left + right, top + bottom);
    return size;
}

bool LegendLayout::widthLongerThan(const LegendWidthStruct *item1,
                                   const LegendWidthStruct *item2)
{
    return item1->width > item2->width;
}

QT_CHARTS_END_NAMESPACE
