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

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtWidgets/QGraphicsTextItem>
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtWidgets/QGraphicsRectItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtGui/QTextDocument>
#include <QtCore/QtMath>

#include <QtCharts/QLegend>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <private/qlegend_p.h>
#include <QtCharts/QLegendMarker>
#include <private/qlegendmarker_p.h>
#include <private/legendmarkeritem_p.h>
#include <private/chartpresenter_p.h>

QT_CHARTS_BEGIN_NAMESPACE

LegendMarkerItem::LegendMarkerItem(QLegendMarkerPrivate *marker, QGraphicsObject *parent) :
    QGraphicsObject(parent),
    m_marker(marker),
    m_defaultMarkerRect(0.0, 0.0, 10.0, 10.0),
    m_markerRect(0.0, 0.0, -1.0, -1.0),
    m_boundingRect(0,0,0,0),
    m_textItem(new QGraphicsTextItem(this)),
    m_markerItem(nullptr),
    m_margin(3),
    m_space(4),
    m_markerShape(QLegend::MarkerShapeDefault),
    m_hovering(false),
    m_itemType(TypeRect)
{
    updateMarkerShapeAndSize();
    m_textItem->document()->setDocumentMargin(ChartPresenter::textMargin());
    setAcceptHoverEvents(true);
}

LegendMarkerItem::~LegendMarkerItem()
{
    if (m_hovering) {
        emit m_marker->q_ptr->hovered(false);
    }
}

void LegendMarkerItem::setPen(const QPen &pen)
{
    m_pen = pen;
    setItemBrushAndPen();
}

QPen LegendMarkerItem::pen() const
{
    return m_pen;
}

void LegendMarkerItem::setBrush(const QBrush &brush)
{
    m_brush = brush;
    setItemBrushAndPen();
}

QBrush LegendMarkerItem::brush() const
{
    return m_brush;
}

void LegendMarkerItem::setSeriesPen(const QPen &pen)
{
    m_seriesPen = pen;
    setItemBrushAndPen();
}

void LegendMarkerItem::setSeriesBrush(const QBrush &brush)
{
    m_seriesBrush = brush;
    setItemBrushAndPen();
}

void LegendMarkerItem::setFont(const QFont &font)
{
    QFontMetrics fn(font);
    m_font = font;

    m_defaultMarkerRect = QRectF(0, 0, fn.height() / 2, fn.height() / 2);
    if (effectiveMarkerShape() != QLegend::MarkerShapeFromSeries)
        updateMarkerShapeAndSize();
    m_marker->invalidateLegend();
}

QFont LegendMarkerItem::font() const
{
    return m_font;
}

void LegendMarkerItem::setLabel(const QString label)
{
    m_label = label;
    updateGeometry();
}

QString LegendMarkerItem::label() const
{
    return m_label;
}

void LegendMarkerItem::setLabelBrush(const QBrush &brush)
{
    m_textItem->setDefaultTextColor(brush.color());
}

QBrush LegendMarkerItem::labelBrush() const
{
    return QBrush(m_textItem->defaultTextColor());
}

void LegendMarkerItem::setGeometry(const QRectF &rect)
{
    const qreal width = rect.width();
    const qreal markerWidth = effectiveMarkerWidth();
    const qreal x = m_margin + markerWidth + m_space + m_margin;
    QRectF truncatedRect;
    const QString html = ChartPresenter::truncatedText(m_font, m_label, qreal(0.0),
                                                       width - x, rect.height(), truncatedRect);
    m_textItem->setHtml(html);
#if QT_CONFIG(tooltip)
    if (m_marker->m_legend->showToolTips() && html != m_label)
        m_textItem->setToolTip(m_label);
    else
        m_textItem->setToolTip(QString());
#endif
    m_textItem->setFont(m_font);
    m_textItem->setTextWidth(truncatedRect.width());

    qreal y = qMax(m_markerRect.height() + 2 * m_margin, truncatedRect.height() + 2 * m_margin);

    const QRectF &textRect = m_textItem->boundingRect();

    m_textItem->setPos(x - m_margin, y / 2 - textRect.height() / 2);
    setItemRect();

    // The textMargin adjustments to position are done to make default case rects less blurry with anti-aliasing
    m_markerItem->setPos(m_margin - ChartPresenter::textMargin()
                         + (markerWidth - m_markerRect.width()) / 2.0,
                         y / 2.0  - m_markerRect.height() / 2.0 + ChartPresenter::textMargin());

    prepareGeometryChange();
    m_boundingRect = QRectF(0, 0, x + textRect.width() + m_margin, y);
}

QRectF LegendMarkerItem::boundingRect() const
{
    return m_boundingRect;
}

QRectF LegendMarkerItem::markerRect() const
{
    return m_markerRect;
}

void LegendMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    Q_UNUSED(painter)
}

QSizeF LegendMarkerItem::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    Q_UNUSED(constraint)

    QSizeF sh;
    const qreal markerWidth = effectiveMarkerWidth();

    switch (which) {
    case Qt::MinimumSize: {
        const QRectF labelRect = ChartPresenter::textBoundingRect(m_font, QStringLiteral("..."));
        sh = QSizeF(labelRect.width() + (2.0 * m_margin) + m_space + markerWidth,
                    qMax(m_markerRect.height(), labelRect.height()) + (2.0 * m_margin));
        break;
    }
    case Qt::PreferredSize: {
        const QRectF labelRect = ChartPresenter::textBoundingRect(m_font, m_label);
        sh = QSizeF(labelRect.width() + (2.0 * m_margin) + m_space + markerWidth,
                    qMax(m_markerRect.height(), labelRect.height()) + (2.0 * m_margin));
        break;
    }
    default:
        break;
    }

    return sh;
}

void LegendMarkerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovering = true;
    emit m_marker->q_ptr->hovered(true);
}

void LegendMarkerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovering = false;
    emit m_marker->q_ptr->hovered(false);
}

QString LegendMarkerItem::displayedLabel() const
{
    return m_textItem->toHtml();
}

void LegendMarkerItem::setToolTip(const QString &tip)
{
#if QT_CONFIG(tooltip)
    m_textItem->setToolTip(tip);
#endif
}

QLegend::MarkerShape LegendMarkerItem::markerShape() const
{
    return m_markerShape;
}

void LegendMarkerItem::setMarkerShape(QLegend::MarkerShape shape)
{
    m_markerShape = shape;
}

void LegendMarkerItem::updateMarkerShapeAndSize()
{
    const QLegend::MarkerShape shape = effectiveMarkerShape();

    ItemType itemType = TypeRect;
    QRectF newRect = m_defaultMarkerRect;
    if (shape == QLegend::MarkerShapeFromSeries) {
        QScatterSeries *scatter = qobject_cast<QScatterSeries *>(m_marker->series());
        if (scatter) {
            newRect.setSize(QSizeF(scatter->markerSize(), scatter->markerSize()));
            if (scatter->markerShape() == QScatterSeries::MarkerShapeCircle)
                itemType = TypeCircle;
        } else if (qobject_cast<QLineSeries *>(m_marker->series())
                   || qobject_cast<QSplineSeries *>(m_marker->series())) {
            newRect.setHeight(m_seriesPen.width());
            newRect.setWidth(qRound(m_defaultMarkerRect.width() * 1.5));
            itemType = TypeLine;
        }
    } else if (shape == QLegend::MarkerShapeCircle) {
        itemType = TypeCircle;
    }

    if (!m_markerItem || m_itemType != itemType) {
        m_itemType = itemType;
        QPointF oldPos;
        if (m_markerItem) {
            oldPos = m_markerItem->pos();
            delete m_markerItem;
        }
        if (itemType == TypeRect)
            m_markerItem = new QGraphicsRectItem(this);
        else if (itemType == TypeCircle)
            m_markerItem = new QGraphicsEllipseItem(this);
        else
            m_markerItem =  new QGraphicsLineItem(this);
        // Immediately update the position to the approximate correct position to avoid marker
        // jumping around when changing markers
        m_markerItem->setPos(oldPos);
    }
    setItemBrushAndPen();

    if (newRect != m_markerRect) {
        if (useMaxWidth() && m_marker->m_legend->d_ptr->maxMarkerWidth() < newRect.width())
            m_marker->invalidateAllItems();
        m_markerRect = newRect;
        setItemRect();
        emit markerRectChanged();
        updateGeometry();
    }
}

QLegend::MarkerShape LegendMarkerItem::effectiveMarkerShape() const
{
    QLegend::MarkerShape shape = m_markerShape;
    if (shape == QLegend::MarkerShapeDefault)
        shape = m_marker->m_legend->markerShape();
    return shape;
}

qreal LegendMarkerItem::effectiveMarkerWidth() const
{
    return useMaxWidth() ? m_marker->m_legend->d_ptr->maxMarkerWidth()
                         : m_markerRect.width();
}

void LegendMarkerItem::setItemBrushAndPen()
{
    if (m_markerItem) {
        QAbstractGraphicsShapeItem *shapeItem =
                qgraphicsitem_cast<QGraphicsRectItem *>(m_markerItem);
        if (!shapeItem)
            shapeItem = qgraphicsitem_cast<QGraphicsEllipseItem *>(m_markerItem);
        if (shapeItem) {
            if (effectiveMarkerShape() == QLegend::MarkerShapeFromSeries) {
                shapeItem->setPen(m_seriesPen);
                shapeItem->setBrush(m_seriesBrush);
            } else {
                shapeItem->setPen(m_pen);
                shapeItem->setBrush(m_brush);
            }
        } else {
            // Must be line item, it has no brush.
            QGraphicsLineItem *lineItem =
                    qgraphicsitem_cast<QGraphicsLineItem *>(m_markerItem);
            if (lineItem)
                lineItem->setPen(m_seriesPen);
        }
    }
}

void LegendMarkerItem::setItemRect()
{
    if (m_itemType == TypeRect) {
        static_cast<QGraphicsRectItem *>(m_markerItem)->setRect(m_markerRect);
    } else if (m_itemType == TypeCircle) {
        static_cast<QGraphicsEllipseItem *>(m_markerItem)->setRect(m_markerRect);
    } else {
        qreal y = m_markerRect.height() / 2.0;
        QLineF line(0.0, y, m_markerRect.width(), y);
        static_cast<QGraphicsLineItem *>(m_markerItem)->setLine(line);
    }
}

bool LegendMarkerItem::useMaxWidth() const
{
    return (m_marker->m_legend->alignment() == Qt::AlignLeft
            || m_marker->m_legend->alignment() == Qt::AlignRight);
}

#include "moc_legendmarkeritem_p.cpp"

QT_CHARTS_END_NAMESPACE
