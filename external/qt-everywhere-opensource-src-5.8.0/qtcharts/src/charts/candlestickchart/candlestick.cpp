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

#include <QtCharts/QCandlestickSet>
#include <QtGui/QPainter>
#include <private/abstractdomain_p.h>
#include <private/candlestick_p.h>
#include <private/qchart_p.h>

QT_CHARTS_BEGIN_NAMESPACE

Candlestick::Candlestick(QCandlestickSet *set, AbstractDomain *domain, QGraphicsObject *parent)
    : QGraphicsObject(parent),
      m_set(set),
      m_domain(domain),
      m_timePeriod(0.0),
      m_maximumColumnWidth(-1.0), // no maximum column width by default
      m_minimumColumnWidth(-1.0), // no minimum column width by default
      m_bodyWidth(0.5),
      m_bodyOutlineVisible(true),
      m_capsWidth(0.5),
      m_capsVisible(false),
      m_brush(QChartPrivate::defaultBrush()),
      m_pen(QChartPrivate::defaultPen()),
      m_hovering(false),
      m_mousePressed(false)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    setFlag(QGraphicsObject::ItemIsSelectable);
}

Candlestick::~Candlestick()
{
    // End hover event, if candlestick is deleted during it.
    if (m_hovering)
        emit hovered(false, m_set);
}

void Candlestick::setTimePeriod(qreal timePeriod)
{
    m_timePeriod = timePeriod;
}

void Candlestick::setMaximumColumnWidth(qreal maximumColumnWidth)
{
    m_maximumColumnWidth = maximumColumnWidth;
}

void Candlestick::setMinimumColumnWidth(qreal minimumColumnWidth)
{
    m_minimumColumnWidth = minimumColumnWidth;
}

void Candlestick::setBodyWidth(qreal bodyWidth)
{
    m_bodyWidth = bodyWidth;
}

void Candlestick::setBodyOutlineVisible(bool bodyOutlineVisible)
{
    m_bodyOutlineVisible = bodyOutlineVisible;
}

void Candlestick::setCapsWidth(qreal capsWidth)
{
    m_capsWidth = capsWidth;
}

void Candlestick::setCapsVisible(bool capsVisible)
{
    m_capsVisible = capsVisible;
}

void Candlestick::setIncreasingColor(const QColor &color)
{
    m_increasingColor = color;

    update();
}

void Candlestick::setDecreasingColor(const QColor &color)
{
    m_decreasingColor = color;

    update();
}

void Candlestick::setBrush(const QBrush &brush)
{
    m_brush = brush;

    update();
}

void Candlestick::setPen(const QPen &pen)
{
    qreal widthDiff = pen.widthF() - m_pen.widthF();
    m_boundingRect.adjust(-widthDiff, -widthDiff, widthDiff, widthDiff);

    m_pen = pen;

    update();
}

void Candlestick::setLayout(const CandlestickData &data)
{
    m_data = data;

    updateGeometry(m_domain);
    update();
}

void Candlestick::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_mousePressed = true;
    emit pressed(m_set);
    QGraphicsItem::mousePressEvent(event);
}

void Candlestick::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    m_hovering = true;
    emit hovered(m_hovering, m_set);
}

void Candlestick::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    m_hovering = false;
    emit hovered(m_hovering, m_set);
}

void Candlestick::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(m_set);
    if (m_mousePressed)
        emit clicked(m_set);
    m_mousePressed = false;
    QGraphicsItem::mouseReleaseEvent(event);
}

void Candlestick::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // For candlestick a pressed signal needs to be explicitly fired for mouseDoubleClickEvent.
    emit pressed(m_set);
    emit doubleClicked(m_set);
    QGraphicsItem::mouseDoubleClickEvent(event);
}

QRectF Candlestick::boundingRect() const
{
    return m_boundingRect;
}

void Candlestick::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    bool increasingTrend = (m_data.m_open < m_data.m_close);
    QColor color = increasingTrend ? m_increasingColor : m_decreasingColor;

    QBrush brush(m_brush);
    brush.setColor(color);

    painter->save();
    painter->setBrush(brush);
    painter->setPen(m_pen);
    painter->setClipRect(m_boundingRect);
    if (m_capsVisible)
        painter->drawPath(m_capsPath);
    painter->drawPath(m_wicksPath);
    if (!m_bodyOutlineVisible)
        painter->setPen(QColor(Qt::transparent));
    painter->drawRect(m_bodyRect);
    painter->restore();
}

void Candlestick::updateGeometry(AbstractDomain *domain)
{
    m_domain = domain;

    prepareGeometryChange();

    m_capsPath = QPainterPath();
    m_wicksPath = QPainterPath();
    m_boundingRect = QRectF();

    if (!m_data.m_series->chart())
        return;

    QList<QAbstractAxis *> axes = m_data.m_series->chart()->axes(Qt::Horizontal, m_data.m_series);
    if (axes.isEmpty())
        return;

    QAbstractAxis *axisX = axes.value(0);
    if (!axisX)
        return;

    qreal columnWidth = 0.0;
    qreal columnCenter = 0.0;
    switch (axisX->type()) {
    case QAbstractAxis::AxisTypeBarCategory:
        columnWidth = 1.0 / m_data.m_seriesCount;
        columnCenter = m_data.m_index - 0.5
                       + m_data.m_seriesIndex * columnWidth
                       + columnWidth / 2.0;
        break;
    case QAbstractAxis::AxisTypeDateTime:
    case QAbstractAxis::AxisTypeValue:
        columnWidth = m_timePeriod;
        columnCenter = m_data.m_timestamp;
        break;
    default:
        qWarning() << "Unexpected axis type";
        return;
    }

    const qreal bodyWidth = m_bodyWidth * columnWidth;
    const qreal bodyLeft = columnCenter - (bodyWidth / 2.0);
    const qreal bodyRight = bodyLeft + bodyWidth;

    const qreal upperBody = qMax(m_data.m_open, m_data.m_close);
    const qreal lowerBody = qMin(m_data.m_open, m_data.m_close);
    const bool upperWickVisible = (m_data.m_high > upperBody);
    const bool lowerWickVisible = (m_data.m_low < lowerBody);

    QPointF geometryPoint;
    bool validData;

    // upper extreme
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(bodyLeft, m_data.m_high), validData);
    if (!validData)
        return;
    const qreal geometryUpperExtreme = geometryPoint.y();
    // upper body
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(bodyLeft, upperBody), validData);
    if (!validData)
        return;
    const qreal geometryBodyLeft = geometryPoint.x();
    const qreal geometryUpperBody = geometryPoint.y();
    // lower body
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(bodyRight, lowerBody), validData);
    if (!validData)
        return;
    const qreal geometryBodyRight = geometryPoint.x();
    const qreal geometryLowerBody = geometryPoint.y();
    // lower extreme
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(bodyRight, m_data.m_low), validData);
    if (!validData)
        return;
    const qreal geometryLowerExtreme = geometryPoint.y();

    // Real Body
    m_bodyRect.setCoords(geometryBodyLeft, geometryUpperBody, geometryBodyRight, geometryLowerBody);
    if (m_maximumColumnWidth != -1.0) {
        if (m_bodyRect.width() > m_maximumColumnWidth) {
            qreal extra = (m_bodyRect.width() - m_maximumColumnWidth) / 2.0;
            m_bodyRect.adjust(extra, 0.0, 0.0, 0.0);
            m_bodyRect.setWidth(m_maximumColumnWidth);
        }
    }
    if (m_minimumColumnWidth != -1.0) {
        if (m_bodyRect.width() < m_minimumColumnWidth) {
            qreal extra = (m_minimumColumnWidth - m_bodyRect.width()) / 2.0;
            m_bodyRect.adjust(-extra, 0.0, 0.0, 0.0);
            m_bodyRect.setWidth(m_minimumColumnWidth);
        }
    }

    const qreal geometryCapsExtra = (m_bodyRect.width() - (m_bodyRect.width() * m_capsWidth)) /2.0;
    const qreal geometryCapsLeft = m_bodyRect.left() + geometryCapsExtra;
    const qreal geometryCapsRight = m_bodyRect.right() - geometryCapsExtra;

    // Upper Wick and Cap
    if (upperWickVisible) {
        m_capsPath.moveTo(geometryCapsLeft, geometryUpperExtreme);
        m_capsPath.lineTo(geometryCapsRight, geometryUpperExtreme);
        m_wicksPath.moveTo((geometryCapsLeft + geometryCapsRight) / 2.0, geometryUpperExtreme);
        m_wicksPath.lineTo((geometryCapsLeft + geometryCapsRight) / 2.0, geometryUpperBody);
    }
    // Lower Wick and Cap
    if (lowerWickVisible) {
        m_capsPath.moveTo(geometryCapsLeft, geometryLowerExtreme);
        m_capsPath.lineTo(geometryCapsRight, geometryLowerExtreme);
        m_wicksPath.moveTo((geometryCapsLeft + geometryCapsRight) / 2.0, geometryLowerBody);
        m_wicksPath.lineTo((geometryCapsLeft + geometryCapsRight) / 2.0, geometryLowerExtreme);
    }
    m_wicksPath.closeSubpath();

    // bounding rectangle top
    qreal boundingRectTop;
    if (upperWickVisible)
        boundingRectTop = m_wicksPath.boundingRect().top();
    else
        boundingRectTop = m_bodyRect.top();
    boundingRectTop = qMax(boundingRectTop, parentItem()->boundingRect().top());
    // bounding rectangle right
    qreal boundingRectRight = qMin(m_bodyRect.right(), parentItem()->boundingRect().right());
    // bounding rectangle bottom
    qreal boundingRectBottom;
    if (lowerWickVisible)
        boundingRectBottom = m_wicksPath.boundingRect().bottom();
    else
        boundingRectBottom = m_bodyRect.bottom();
    boundingRectBottom = qMin(boundingRectBottom, parentItem()->boundingRect().bottom());
    // bounding rectangle left
    qreal boundingRectLeft = qMax(m_bodyRect.left(), parentItem()->boundingRect().left());

    m_boundingRect.setTop(boundingRectTop);
    m_boundingRect.setRight(boundingRectRight);
    m_boundingRect.setBottom(boundingRectBottom);
    m_boundingRect.setLeft(boundingRectLeft);

    qreal extra = m_pen.widthF();
    m_boundingRect.adjust(-extra, -extra, extra, extra);
}

#include "moc_candlestick_p.cpp"

QT_CHARTS_END_NAMESPACE
