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

#include <private/piesliceitem_p.h>
#include <private/piechartitem_p.h>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <private/chartpresenter_p.h>
#include <QtGui/QPainter>
#include <QtCore/QtMath>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtCore/QTime>
#include <QtGui/QTextDocument>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

QPointF offset(qreal angle, qreal length)
{
    qreal dx = qSin(angle * (M_PI / 180)) * length;
    qreal dy = qCos(angle * (M_PI / 180)) * length;
    return QPointF(dx, -dy);
}

PieSliceItem::PieSliceItem(QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_hovered(false),
      m_mousePressed(false)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    setZValue(ChartPresenter::PieSeriesZValue);
    setFlag(QGraphicsItem::ItemIsSelectable);
    m_labelItem = new QGraphicsTextItem(this);
    m_labelItem->document()->setDocumentMargin(1.0);
}

PieSliceItem::~PieSliceItem()
{
    // If user is hovering over the slice and it gets destroyed we do
    // not get a hover leave event. So we must emit the signal here.
    if (m_hovered)
        emit hovered(false);
}

QRectF PieSliceItem::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath PieSliceItem::shape() const
{
    // Don't include the label and label arm.
    // This is used to detect a mouse clicks. We do not want clicks from label.
    return m_slicePath;
}

void PieSliceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
    painter->save();
    painter->setClipRect(parentItem()->boundingRect());
    painter->setPen(m_data.m_slicePen);
    painter->setBrush(m_data.m_sliceBrush);
    painter->drawPath(m_slicePath);
    painter->restore();

    if (m_data.m_isLabelVisible) {
        painter->save();

        // Pen for label arm not defined in the QPieSeries api, let's use brush's color instead
        painter->setBrush(m_data.m_labelBrush);

        if (m_data.m_labelPosition == QPieSlice::LabelOutside) {
            painter->setClipRect(parentItem()->boundingRect());
            painter->strokePath(m_labelArmPath, m_data.m_labelBrush.color());
        }

        painter->restore();
    }
}

void PieSliceItem::hoverEnterEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    m_hovered = true;
    emit hovered(true);
}

void PieSliceItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    m_hovered = false;
    emit hovered(false);
}

void PieSliceItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit pressed(event->buttons());
    m_mousePressed = true;
}

void PieSliceItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->buttons());
    if (m_mousePressed)
        emit clicked(event->buttons());
}

void PieSliceItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // For Pie slice a press signal needs to be explicitly fired for mouseDoubleClickEvent
    emit pressed(event->buttons());
    emit doubleClicked(event->buttons());
}

void PieSliceItem::setLayout(const PieSliceData &sliceData)
{
    m_data = sliceData;
    updateGeometry();
    update();
}

void PieSliceItem::updateGeometry()
{
    if (m_data.m_radius <= 0)
        return;

    prepareGeometryChange();

    // slice path
    qreal centerAngle;
    QPointF armStart;
    m_slicePath = slicePath(m_data.m_center, m_data.m_radius, m_data.m_startAngle, m_data.m_angleSpan, &centerAngle, &armStart);

    m_labelItem->setVisible(m_data.m_isLabelVisible);

    if (m_data.m_isLabelVisible) {
        // text rect
        m_labelTextRect = ChartPresenter::textBoundingRect(m_data.m_labelFont,
                                                           m_data.m_labelText,
                                                           0);

        QString label(m_data.m_labelText);
        m_labelItem->setDefaultTextColor(m_data.m_labelBrush.color());
        m_labelItem->setFont(m_data.m_labelFont);

        // text position
        if (m_data.m_labelPosition == QPieSlice::LabelOutside) {
            setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);

            // label arm path
            QPointF labelTextStart;
            m_labelArmPath = labelArmPath(armStart, centerAngle,
                                          m_data.m_radius * m_data.m_labelArmLengthFactor,
                                          m_labelTextRect.width(), &labelTextStart);

            m_labelTextRect.moveBottomLeft(labelTextStart);
            if (m_labelTextRect.left() < 0)
                m_labelTextRect.setLeft(0);
            else if (m_labelTextRect.left() < parentItem()->boundingRect().left())
                m_labelTextRect.setLeft(parentItem()->boundingRect().left());
            if (m_labelTextRect.right() > parentItem()->boundingRect().right())
                m_labelTextRect.setRight(parentItem()->boundingRect().right());

            label = ChartPresenter::truncatedText(m_data.m_labelFont, m_data.m_labelText,
                                                  qreal(0.0), m_labelTextRect.width(),
                                                  m_labelTextRect.height(), m_labelTextRect);
            m_labelArmPath = labelArmPath(armStart, centerAngle,
                                          m_data.m_radius * m_data.m_labelArmLengthFactor,
                                          m_labelTextRect.width(), &labelTextStart);
            m_labelTextRect.moveBottomLeft(labelTextStart);

            m_labelItem->setTextWidth(m_labelTextRect.width()
                                      + m_labelItem->document()->documentMargin());
            m_labelItem->setHtml(label);
            m_labelItem->setRotation(0);
            m_labelItem->setPos(m_labelTextRect.x(), m_labelTextRect.y() + 1.0);
        } else {
            // label inside
            setFlag(QGraphicsItem::ItemClipsChildrenToShape);
            m_labelItem->setTextWidth(m_labelTextRect.width()
                                      + m_labelItem->document()->documentMargin());
            m_labelItem->setHtml(label);

            QPointF textCenter;
            if (m_data.m_holeRadius > 0) {
                textCenter = m_data.m_center + offset(centerAngle, m_data.m_holeRadius
                                                      + (m_data.m_radius
                                                         - m_data.m_holeRadius) / 2);
            } else {
                textCenter = m_data.m_center + offset(centerAngle, m_data.m_radius / 2);
            }
            m_labelItem->setPos(textCenter.x() - m_labelItem->boundingRect().width() / 2,
                                textCenter.y() - m_labelTextRect.height() / 2);

            QPointF labelCenter = m_labelItem->boundingRect().center();
            m_labelItem->setTransformOriginPoint(labelCenter);

            if (m_data.m_labelPosition == QPieSlice::LabelInsideTangential) {
                m_labelItem->setRotation(m_data.m_startAngle + m_data.m_angleSpan / 2);
            } else if (m_data.m_labelPosition == QPieSlice::LabelInsideNormal) {
                if (m_data.m_startAngle + m_data.m_angleSpan / 2 < 180)
                    m_labelItem->setRotation(m_data.m_startAngle + m_data.m_angleSpan / 2 - 90);
                else
                    m_labelItem->setRotation(m_data.m_startAngle + m_data.m_angleSpan / 2 + 90);
            } else {
                m_labelItem->setRotation(0);
            }
        }
        // Hide label if it's outside the bounding rect of parent item
        QRectF labelRect(m_labelItem->boundingRect());
        labelRect.moveTopLeft(m_labelItem->pos());
        if ((parentItem()->boundingRect().left()
                    < (labelRect.left() + m_labelItem->document()->documentMargin() + 1.0))
                && (parentItem()->boundingRect().right()
                    > (labelRect.right() - m_labelItem->document()->documentMargin() - 1.0))
                && (parentItem()->boundingRect().top()
                    < (labelRect.top() + m_labelItem->document()->documentMargin() + 1.0))
                && (parentItem()->boundingRect().bottom()
                    > (labelRect.bottom() - m_labelItem->document()->documentMargin() - 1.0)))
            m_labelItem->show();
        else
            m_labelItem->hide();
    }

    //  bounding rect
    if (m_data.m_isLabelVisible)
        m_boundingRect = m_slicePath.boundingRect().united(m_labelArmPath.boundingRect()).united(m_labelTextRect);
    else
        m_boundingRect = m_slicePath.boundingRect();

    // Inflate bounding rect by 2/3 pen width to make sure it encompasses whole slice also for thick pens
    // and miter joins.
    int penWidth = (m_data.m_slicePen.width() * 2) / 3;
    m_boundingRect = m_boundingRect.adjusted(-penWidth, -penWidth, penWidth, penWidth);
}

QPointF PieSliceItem::sliceCenter(QPointF point, qreal radius, QPieSlice *slice)
{
    if (slice->isExploded()) {
        qreal centerAngle = slice->startAngle() + (slice->angleSpan() / 2);
        qreal len = radius * slice->explodeDistanceFactor();
        point += offset(centerAngle, len);
    }
    return point;
}

QPainterPath PieSliceItem::slicePath(QPointF center, qreal radius, qreal startAngle, qreal angleSpan, qreal *centerAngle, QPointF *armStart)
{
    // calculate center angle
    *centerAngle = startAngle + (angleSpan / 2);

    // calculate slice rectangle
    QRectF rect(center.x() - radius, center.y() - radius, radius * 2, radius * 2);

    // slice path
    QPainterPath path;
    if (m_data.m_holeRadius > 0) {
        QRectF insideRect(center.x() - m_data.m_holeRadius, center.y() - m_data.m_holeRadius, m_data.m_holeRadius * 2, m_data.m_holeRadius * 2);
        path.arcMoveTo(rect, -startAngle + 90);
        path.arcTo(rect, -startAngle + 90, -angleSpan);
        path.arcTo(insideRect, -startAngle + 90 - angleSpan, angleSpan);
        path.closeSubpath();
    } else {
        path.moveTo(rect.center());
        path.arcTo(rect, -startAngle + 90, -angleSpan);
        path.closeSubpath();
    }

    // calculate label arm start point
    *armStart = center;
    *armStart += offset(*centerAngle, radius + PIESLICE_LABEL_GAP);

    return path;
}

QPainterPath PieSliceItem::labelArmPath(QPointF start, qreal angle, qreal length, qreal textWidth, QPointF *textStart)
{
    // Normalize the angle to 0-360 range
    // NOTE: We are using int here on purpose. Depenging on platform and hardware
    // qreal can be a double, float or something the user gives to the Qt configure
    // (QT_COORD_TYPE). Compilers do not seem to support modulo for double or float
    // but there are fmod() and fmodf() functions for that. So instead of some #ifdef
    // that might break we just use int. Precision for this is just fine for our needs.
    int normalized = angle * 10.0;
    normalized = normalized % 3600;
    if (normalized < 0)
        normalized += 3600;
    angle = (qreal) normalized / 10.0;

    // prevent label arm pointing straight down because it will look bad
    if (angle < 180 && angle > 170)
        angle = 170;
    if (angle > 180 && angle < 190)
        angle = 190;

    // line from slice to label
    QPointF parm1 = start + offset(angle, length);

    // line to underline the label
    QPointF parm2 = parm1;
    if (angle < 180) { // arm swings the other way on the left side
        parm2 += QPointF(textWidth, 0);
        *textStart = parm1;
    } else {
        parm2 += QPointF(-textWidth, 0);
        *textStart = parm2;
    }

    QPainterPath path;
    path.moveTo(start);
    path.lineTo(parm1);
    path.lineTo(parm2);

    return path;
}

#include "moc_piesliceitem_p.cpp"

QT_CHARTS_END_NAMESPACE

