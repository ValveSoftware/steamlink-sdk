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

#include <private/boxwhiskers_p.h>
#include <QtGui/QPainter>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsSceneMouseEvent>

QT_CHARTS_BEGIN_NAMESPACE

BoxWhiskers::BoxWhiskers(QBoxSet *set, AbstractDomain *domain, QGraphicsObject *parent) :
    QGraphicsObject(parent),
    m_boxSet(set),
    m_domain(domain),
    m_mousePressed(false)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    setFlag(QGraphicsObject::ItemIsSelectable);
}

BoxWhiskers::~BoxWhiskers()
{
}

void BoxWhiskers::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    emit pressed(m_boxSet);
    m_mousePressed = true;
}

void BoxWhiskers::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    emit hovered(true, m_boxSet);
}

void BoxWhiskers::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    emit hovered(false, m_boxSet);
}

void BoxWhiskers::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    emit released(m_boxSet);
    if (m_mousePressed)
        emit clicked(m_boxSet);
}

void BoxWhiskers::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    // For Box a press signal needs to be explicitly fired for mouseDoubleClickEvent
    emit pressed(m_boxSet);
    emit doubleClicked(m_boxSet);
}

void BoxWhiskers::setBrush(const QBrush &brush)
{
    m_brush = brush;
    m_outlinePen.setColor(m_brush.color());
    update();
}

void BoxWhiskers::setPen(const QPen &pen)
{
    qreal widthDiff = pen.widthF() - m_pen.widthF();
    m_boundingRect.adjust(-widthDiff, -widthDiff, widthDiff, widthDiff);

    m_pen = pen;
    m_medianPen = pen;
    m_medianPen.setCapStyle(Qt::FlatCap);
    m_outlinePen = pen;
    m_outlinePen.setStyle(Qt::SolidLine);
    m_outlinePen.setColor(m_brush.color());

    update();
}

void BoxWhiskers::setBoxWidth(const qreal width)
{
    m_boxWidth = width;
}

void BoxWhiskers::setLayout(const BoxWhiskersData &data)
{
    m_data = data;

    updateGeometry(m_domain);
    update();
}

QSizeF BoxWhiskers::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(which)
    Q_UNUSED(constraint)

    return QSizeF();
}

void BoxWhiskers::setGeometry(const QRectF &rect)
{
    Q_UNUSED(rect)
}

QRectF BoxWhiskers::boundingRect() const
{
    return m_boundingRect;
}

void BoxWhiskers::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setBrush(m_brush);
    painter->setClipRect(parentItem()->boundingRect());
    painter->setPen(m_pen);
    painter->drawPath(m_boxPath);
    if (!m_boxOutlined)
        painter->setPen(m_outlinePen);
    painter->drawRect(m_middleBox);
    painter->setPen(m_medianPen);
    qreal halfLine = m_pen.widthF() / 2.0;
    painter->drawLine(QLineF(m_geometryLeft - halfLine, m_geometryMedian,
                             m_geometryRight + halfLine, m_geometryMedian));
    painter->restore();
}

void BoxWhiskers::updateGeometry(AbstractDomain *domain)
{
    m_domain = domain;

    prepareGeometryChange();

    QPainterPath path;
    m_boxPath = path;
    m_boundingRect = m_boxPath.boundingRect();

    qreal columnWidth = 1.0 / m_data.m_seriesCount;
    qreal left = ((1.0 - m_boxWidth) / 2.0) * columnWidth + columnWidth * m_data.m_seriesIndex + m_data.m_index - 0.5;
    qreal barWidth = m_boxWidth * columnWidth;

    QPointF geometryPoint = m_domain->calculateGeometryPoint(QPointF(left, m_data.m_upperExtreme), m_validData);
    if (!m_validData)
        return;
    m_geometryLeft = geometryPoint.x();
    qreal geometryUpperExtreme = geometryPoint.y();
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(left + barWidth, m_data.m_upperQuartile), m_validData);
    if (!m_validData)
        return;
    m_geometryRight = geometryPoint.x();
    qreal geometryUpperQuartile = geometryPoint.y();
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(left, m_data.m_lowerQuartile), m_validData);
    if (!m_validData)
        return;
    qreal geometryLowerQuartile = geometryPoint.y();
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(left, m_data.m_lowerExtreme), m_validData);
    if (!m_validData)
        return;
    qreal geometryLowerExtreme = geometryPoint.y();
    geometryPoint = m_domain->calculateGeometryPoint(QPointF(left, m_data.m_median), m_validData);
    if (!m_validData)
        return;
    m_geometryMedian = geometryPoint.y();

    // Upper whisker
    path.moveTo(m_geometryLeft, geometryUpperExtreme);
    path.lineTo(m_geometryRight, geometryUpperExtreme);
    path.moveTo((m_geometryLeft + m_geometryRight) / 2.0, geometryUpperExtreme);
    path.lineTo((m_geometryLeft + m_geometryRight) / 2.0, geometryUpperQuartile);

    // Middle Box
    m_middleBox.setCoords(m_geometryLeft, geometryUpperQuartile, m_geometryRight, geometryLowerQuartile);

    // Lower whisker
    path.moveTo(m_geometryLeft, geometryLowerExtreme);
    path.lineTo(m_geometryRight, geometryLowerExtreme);
    path.moveTo((m_geometryLeft + m_geometryRight) / 2.0, geometryLowerQuartile);
    path.lineTo((m_geometryLeft + m_geometryRight) / 2.0, geometryLowerExtreme);

    path.closeSubpath();

    m_boxPath = path;
    m_boundingRect = m_boxPath.boundingRect();

    qreal extra = m_pen.widthF();
    m_boundingRect.adjust(-extra, -extra, extra, extra);
}

#include "moc_boxwhiskers_p.cpp"

QT_CHARTS_END_NAMESPACE
