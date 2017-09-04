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

#include <QtCharts/qcategoryaxis.h>
#include <QtCharts/qlogvalueaxis.h>
#include <QtCore/qmath.h>
#include <QtGui/qtextdocument.h>
#include <private/chartpresenter_p.h>
#include <private/linearrowitem_p.h>
#include <private/polarchartaxisradial_p.h>

QT_CHARTS_BEGIN_NAMESPACE

PolarChartAxisRadial::PolarChartAxisRadial(QAbstractAxis *axis, QGraphicsItem *item,
                                           bool intervalAxis)
    : PolarChartAxis(axis, item, intervalAxis)
{
}

PolarChartAxisRadial::~PolarChartAxisRadial()
{
}

void PolarChartAxisRadial::updateGeometry()
{
    const QVector<qreal> &layout = this->layout();
    if (layout.isEmpty() && axis()->type() != QAbstractAxis::AxisTypeLogValue)
        return;

    createAxisLabels(layout);
    QStringList labelList = labels();
    QPointF center = axisGeometry().center();
    QList<QGraphicsItem *> arrowItemList = arrowItems();
    QList<QGraphicsItem *> gridItemList = gridItems();
    QList<QGraphicsItem *> labelItemList = labelItems();
    QList<QGraphicsItem *> shadeItemList = shadeItems();
    QList<QGraphicsItem *> minorGridItemList = minorGridItems();
    QList<QGraphicsItem *> minorArrowItemList = minorArrowItems();
    QGraphicsTextItem* title = titleItem();
    qreal radius = axisGeometry().height() / 2.0;

    QLineF line(center, center + QPointF(0, -radius));
    QGraphicsLineItem *axisLine = static_cast<QGraphicsLineItem *>(arrowItemList.at(0));
    axisLine->setLine(line);

    QRectF previousLabelRect;
    bool firstShade = true;
    bool nextTickVisible = false;
    if (layout.size())
        nextTickVisible = !(layout.at(0) < 0.0 || layout.at(0) > radius);

    for (int i = 0; i < layout.size(); ++i) {
        qreal radialCoordinate = layout.at(i);

        QGraphicsEllipseItem *gridItem = static_cast<QGraphicsEllipseItem *>(gridItemList.at(i));
        QGraphicsLineItem *tickItem = static_cast<QGraphicsLineItem *>(arrowItemList.at(i + 1));
        QGraphicsTextItem *labelItem = static_cast<QGraphicsTextItem *>(labelItemList.at(i));
        QGraphicsPathItem *shadeItem = 0;
        if (i == 0)
            shadeItem = static_cast<QGraphicsPathItem *>(shadeItemList.at(0));
        else if (i % 2)
            shadeItem = static_cast<QGraphicsPathItem *>(shadeItemList.at((i / 2) + 1));

        // Ignore ticks outside valid range
        bool currentTickVisible = nextTickVisible;
        if ((i == layout.size() - 1)
            || layout.at(i + 1) < 0.0
            || layout.at(i + 1) > radius) {
            nextTickVisible = false;
        } else {
            nextTickVisible = true;
        }

        qreal labelCoordinate = radialCoordinate;
        bool labelVisible = currentTickVisible;
        qreal labelPad = labelPadding() / 2.0;
        bool centeredLabel = true; // Only used with interval axes
        if (intervalAxis()) {
            qreal farEdge;
            if (i == (layout.size() - 1))
                farEdge = radius;
            else
                farEdge = qMin(radius, layout.at(i + 1));

            // Adjust the labelCoordinate to show it if next tick is visible
            if (nextTickVisible)
                labelCoordinate = qMax(qreal(0.0), labelCoordinate);

            if (axis()->type() == QAbstractAxis::AxisTypeCategory) {
                QCategoryAxis *categoryAxis = static_cast<QCategoryAxis *>(axis());
                if (categoryAxis->labelsPosition() == QCategoryAxis::AxisLabelsPositionOnValue)
                    centeredLabel = false;
            }
            if (centeredLabel) {
                labelCoordinate = (labelCoordinate + farEdge) / 2.0;
                if (labelCoordinate > 0.0 && labelCoordinate < radius)
                    labelVisible =  true;
                else
                    labelVisible = false;
            } else {
                labelVisible = nextTickVisible;
                labelCoordinate = farEdge;
            }
        }

        // Radial axis label
        if (axis()->labelsVisible() && labelVisible) {
            QRectF boundingRect = ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                   labelList.at(i),
                                                                   axis()->labelsAngle());
            labelItem->setTextWidth(boundingRect.width());
            labelItem->setHtml(labelList.at(i));
            QRectF labelRect = labelItem->boundingRect();
            QPointF labelCenter = labelRect.center();
            labelItem->setTransformOriginPoint(labelCenter.x(), labelCenter.y());
            boundingRect.moveCenter(labelCenter);
            QPointF positionDiff(labelRect.topLeft() - boundingRect.topLeft());
            QPointF labelPoint = center;
            if (intervalAxis() && centeredLabel)
                labelPoint += QPointF(labelPad, -labelCoordinate - (boundingRect.height() / 2.0));
            else
                labelPoint += QPointF(labelPad, labelPad - labelCoordinate);
            labelRect.moveTopLeft(labelPoint);
            labelItem->setPos(labelRect.topLeft() + positionDiff);

            // Label overlap detection
            labelRect.setSize(boundingRect.size());
            if ((i && previousLabelRect.intersects(labelRect))
                 || !axisGeometry().contains(labelRect)) {
                labelVisible = false;
            } else {
                previousLabelRect = labelRect;
                labelVisible = true;
            }
        }

        labelItem->setVisible(labelVisible);
        if (!currentTickVisible) {
            gridItem->setVisible(false);
            tickItem->setVisible(false);
            if (shadeItem)
                shadeItem->setVisible(false);
            continue;
        }

        // Radial grid line
        QRectF gridRect;
        gridRect.setWidth(radialCoordinate * 2.0);
        gridRect.setHeight(radialCoordinate * 2.0);
        gridRect.moveCenter(center);

        gridItem->setRect(gridRect);
        gridItem->setVisible(true);

        // Tick
        QLineF tickLine(-tickWidth(), 0.0, tickWidth(), 0.0);
        tickLine.translate(center.rx(), gridRect.top());
        tickItem->setLine(tickLine);
        tickItem->setVisible(true);

        // Shades
        if (i % 2 || (i == 0 && !nextTickVisible)) {
            QPainterPath path;
            if (i == 0) {
                // If first tick is also the last, we need to custom fill the inner circle
                // or it won't get filled.
                QRectF innerCircle(0.0, 0.0, layout.at(0) * 2.0, layout.at(0) * 2.0);
                innerCircle.moveCenter(center);
                path.addEllipse(innerCircle);
            } else {
                QRectF otherGridRect;
                if (!nextTickVisible) { // Last visible tick
                    otherGridRect = axisGeometry();
                } else {
                    qreal otherGridRectDimension = layout.at(i + 1) * 2.0;
                    otherGridRect.setWidth(otherGridRectDimension);
                    otherGridRect.setHeight(otherGridRectDimension);
                    otherGridRect.moveCenter(center);
                }
                path.addEllipse(gridRect);
                path.addEllipse(otherGridRect);

                // Add additional shading in first visible shade item if there is a partial tick
                // to be filled at the center (log & category axes)
                if (firstShade) {
                    QGraphicsPathItem *specialShadeItem = static_cast<QGraphicsPathItem *>(shadeItemList.at(0));
                    if (layout.at(i - 1) > 0.0) {
                        QRectF innerCircle(0.0, 0.0, layout.at(i - 1) * 2.0, layout.at(i - 1) * 2.0);
                        innerCircle.moveCenter(center);
                        QPainterPath specialPath;
                        specialPath.addEllipse(innerCircle);
                        specialShadeItem->setPath(specialPath);
                        specialShadeItem->setVisible(true);
                    } else {
                        specialShadeItem->setVisible(false);
                    }
                }
            }
            shadeItem->setPath(path);
            shadeItem->setVisible(true);
            firstShade = false;
        }
    }

    updateMinorTickGeometry();

    // Title, along the 0 axis
    QString titleText = axis()->titleText();
    if (!titleText.isEmpty() && axis()->isTitleVisible()) {
        QRectF truncatedRect;
        title->setHtml(ChartPresenter::truncatedText(axis()->titleFont(), titleText, qreal(0.0),
                                                     radius, radius, truncatedRect));
        title->setTextWidth(truncatedRect.width());

        QRectF titleBoundingRect = title->boundingRect();
        QPointF titleCenter = titleBoundingRect.center();
        QPointF arrowCenter = axisLine->boundingRect().center();
        QPointF titleCenterDiff = arrowCenter - titleCenter;
        title->setPos(titleCenterDiff.x() - titlePadding() - (titleBoundingRect.height() / 2.0), titleCenterDiff.y());
        title->setTransformOriginPoint(titleCenter);
        title->setRotation(270.0);
    }

    QGraphicsLayoutItem::updateGeometry();
}

Qt::Orientation PolarChartAxisRadial::orientation() const
{
    return Qt::Vertical;
}

void PolarChartAxisRadial::createItems(int count)
{
     if (arrowItems().count() == 0) {
        // radial axis center line
        QGraphicsLineItem *arrow = new LineArrowItem(this, presenter()->rootItem());
        arrow->setPen(axis()->linePen());
        arrowGroup()->addToGroup(arrow);
    }

    QGraphicsTextItem *title = titleItem();
    title->setFont(axis()->titleFont());
    title->setDefaultTextColor(axis()->titleBrush().color());
    title->setHtml(axis()->titleText());

    for (int i = 0; i < count; ++i) {
        QGraphicsLineItem *arrow = new QGraphicsLineItem(presenter()->rootItem());
        QGraphicsEllipseItem *grid = new QGraphicsEllipseItem(presenter()->rootItem());
        QGraphicsTextItem *label = new QGraphicsTextItem(presenter()->rootItem());
        label->document()->setDocumentMargin(ChartPresenter::textMargin());
        arrow->setPen(axis()->linePen());
        grid->setPen(axis()->gridLinePen());
        label->setFont(axis()->labelsFont());
        label->setDefaultTextColor(axis()->labelsBrush().color());
        label->setRotation(axis()->labelsAngle());
        arrowGroup()->addToGroup(arrow);
        gridGroup()->addToGroup(grid);
        labelGroup()->addToGroup(label);
        if (gridItems().size() == 1 || (((gridItems().size() + 1) % 2) && gridItems().size() > 0)) {
            QGraphicsPathItem *shade = new QGraphicsPathItem(presenter()->rootItem());
            shade->setPen(axis()->shadesPen());
            shade->setBrush(axis()->shadesBrush());
            shadeGroup()->addToGroup(shade);
        }
    }
}

void PolarChartAxisRadial::handleArrowPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, arrowItems())
        static_cast<QGraphicsLineItem *>(item)->setPen(pen);
}

void PolarChartAxisRadial::handleGridPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, gridItems())
        static_cast<QGraphicsEllipseItem *>(item)->setPen(pen);
}

void PolarChartAxisRadial::handleMinorArrowPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, minorArrowItems())
        static_cast<QGraphicsLineItem *>(item)->setPen(pen);
}

void PolarChartAxisRadial::handleMinorGridPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, minorGridItems())
        static_cast<QGraphicsEllipseItem *>(item)->setPen(pen);
}

void PolarChartAxisRadial::handleGridLineColorChanged(const QColor &color)
{
    foreach (QGraphicsItem *item, gridItems()) {
        QGraphicsEllipseItem *ellipseItem = static_cast<QGraphicsEllipseItem *>(item);
        QPen pen = ellipseItem->pen();
        pen.setColor(color);
        ellipseItem->setPen(pen);
    }
}

void PolarChartAxisRadial::handleMinorGridLineColorChanged(const QColor &color)
{
    foreach (QGraphicsItem *item, minorGridItems()) {
        QGraphicsEllipseItem *ellipseItem = static_cast<QGraphicsEllipseItem *>(item);
        QPen pen = ellipseItem->pen();
        pen.setColor(color);
        ellipseItem->setPen(pen);
    }
}

QSizeF PolarChartAxisRadial::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(which);
    Q_UNUSED(constraint);
    return QSizeF(-1.0, -1.0);
}

qreal PolarChartAxisRadial::preferredAxisRadius(const QSizeF &maxSize)
{
    qreal radius = maxSize.height() / 2.0;
    if (maxSize.width() < maxSize.height())
        radius = maxSize.width() / 2.0;
    return radius;
}

void PolarChartAxisRadial::updateMinorTickGeometry()
{
    if (!axis())
        return;

    QVector<qreal> layout = ChartAxisElement::layout();
    int minorTickCount = 0;
    qreal tickRadius = 0.0;
    QVector<qreal> minorTickRadiuses;
    switch (axis()->type()) {
    case QAbstractAxis::AxisTypeValue: {
        const QValueAxis *valueAxis = qobject_cast<QValueAxis *>(axis());

        minorTickCount = valueAxis->minorTickCount();

        if (valueAxis->tickCount() >= 2)
            tickRadius = layout.at(1) - layout.at(0);

        for (int i = 0; i < minorTickCount; ++i) {
            const qreal ratio = (1.0 / qreal(minorTickCount + 1)) * qreal(i + 1);
            minorTickRadiuses.append(tickRadius * ratio);
        }
        break;
    }
    case QAbstractAxis::AxisTypeLogValue: {
        const QLogValueAxis *logValueAxis = qobject_cast<QLogValueAxis *>(axis());
        const qreal base = logValueAxis->base();
        const qreal logBase = qLn(base);

        minorTickCount = logValueAxis->minorTickCount();
        if (minorTickCount < 0)
            minorTickCount = qMax(int(qFloor(base) - 2.0), 0);

        // Two "virtual" ticks are required to make sure that all minor ticks
        // are displayed properly (even for the partially visible segments of
        // the chart).
        if (layout.size() >= 2) {
            // Calculate tickRadius as a difference between visible ticks
            // whenever it is possible. Virtual ticks will not be correctly
            // positioned when the layout is animating.
            tickRadius = layout.at(1) - layout.at(0);
            layout.prepend(layout.at(0) - tickRadius);
            layout.append(layout.at(layout.size() - 1) + tickRadius);
        } else {
            const qreal logMax = qLn(logValueAxis->max());
            const qreal logMin = qLn(logValueAxis->min());
            const qreal logExtraMaxTick = qLn(qPow(base, qFloor(logMax / logBase) + 1.0));
            const qreal logExtraMinTick = qLn(qPow(base, qCeil(logMin / logBase) - 1.0));
            const qreal edge = qMin(logMin, logMax);
            const qreal delta = (axisGeometry().width() / 2.0) / qAbs(logMax - logMin);
            const qreal extraMaxTick = edge + (logExtraMaxTick - edge) * delta;
            const qreal extraMinTick = edge + (logExtraMinTick - edge) * delta;

            // Calculate tickRadius using one (if layout.size() == 1) or two
            // (if layout.size() == 0) "virtual" ticks. In both cases animation
            // will not work as expected. This should be fixed later.
            layout.prepend(extraMinTick);
            layout.append(extraMaxTick);
            tickRadius = layout.at(1) - layout.at(0);
        }

        const qreal minorTickStepValue = qFabs(base - 1.0) / qreal(minorTickCount + 1);
        for (int i = 0; i < minorTickCount; ++i) {
            const qreal x = minorTickStepValue * qreal(i + 1) + 1.0;
            const qreal minorTickSpacing = tickRadius * (qLn(x) / logBase);
            minorTickRadiuses.append(minorTickSpacing);
        }
        break;
    }
    default:
        // minor ticks are not supported
        break;
    }

    if (minorTickCount < 1 || tickRadius == 0.0 || minorTickRadiuses.count() != minorTickCount)
        return;

    const QPointF axisCenter = axisGeometry().center();
    const qreal axisRadius = axisGeometry().height() / 2.0;

    for (int i = 0; i < layout.size() - 1; ++i) {
        for (int j = 0; j < minorTickCount; ++j) {
            const int minorItemIndex = i * minorTickCount + j;
            QGraphicsEllipseItem *minorGridLineItem =
                    static_cast<QGraphicsEllipseItem *>(minorGridItems().at(minorItemIndex));
            QGraphicsLineItem *minorArrowLineItem =
                    static_cast<QGraphicsLineItem *>(minorArrowItems().at(minorItemIndex));
            if (!minorGridLineItem || !minorArrowLineItem)
                continue;

            const qreal minorTickRadius = layout.at(i) + minorTickRadiuses.value(j, 0.0);
            const qreal minorTickDiameter = minorTickRadius * 2.0;

            QRectF minorGridLine(0.0, 0.0, minorTickDiameter, minorTickDiameter);
            minorGridLine.moveCenter(axisCenter);
            minorGridLineItem->setRect(minorGridLine);

            QLineF minorArrowLine(-tickWidth() + 1.0, 0.0, tickWidth() - 1.0, 0.0);
            minorArrowLine.translate(axisCenter.x(), minorGridLine.top());
            minorArrowLineItem->setLine(minorArrowLine);

            // check if the minor grid line and the minor axis arrow should be shown
            bool minorGridLineVisible = (minorTickRadius >= 0.0 && minorTickRadius <= axisRadius);
            minorGridLineItem->setVisible(minorGridLineVisible);
            minorArrowLineItem->setVisible(minorGridLineVisible);
        }
    }
}

void PolarChartAxisRadial::updateMinorTickItems()
{
    int currentCount = minorArrowItems().size();
    int expectedCount = 0;
    if (axis()->type() == QAbstractAxis::AxisTypeValue) {
        QValueAxis *valueAxis = qobject_cast<QValueAxis *>(axis());
        expectedCount = valueAxis->minorTickCount() * (valueAxis->tickCount() - 1);
        expectedCount = qMax(expectedCount, 0);
    } else if (axis()->type() == QAbstractAxis::AxisTypeLogValue) {
        QLogValueAxis *logValueAxis = qobject_cast<QLogValueAxis *>(axis());

        int minorTickCount = logValueAxis->minorTickCount();
        if (minorTickCount < 0)
            minorTickCount = qMax(int(qFloor(logValueAxis->base()) - 2.0), 0);

        expectedCount = minorTickCount * (logValueAxis->tickCount() + 1);
        expectedCount = qMax(expectedCount, logValueAxis->minorTickCount());
    } else {
        // minor ticks are not supported
        return;
    }

    int diff = expectedCount - currentCount;
    if (diff > 0) {
        for (int i = 0; i < diff; ++i) {
            QGraphicsEllipseItem *minorGridItem = new QGraphicsEllipseItem(presenter()->rootItem());
            minorGridItem->setPen(axis()->minorGridLinePen());
            minorGridGroup()->addToGroup(minorGridItem);

            QGraphicsLineItem *minorArrowItem = new QGraphicsLineItem(presenter()->rootItem());
            minorArrowItem->setPen(axis()->linePen());
            minorArrowGroup()->addToGroup(minorArrowItem);
        }
    } else {
        QList<QGraphicsItem *> minorGridItemsList = minorGridItems();
        QList<QGraphicsItem *> minorArrowItemsList = minorArrowItems();
        for (int i = 0; i > diff; --i) {
            if (!minorGridItemsList.isEmpty())
                delete minorGridItemsList.takeLast();

            if (!minorArrowItemsList.isEmpty())
                delete minorArrowItemsList.takeLast();
        }
    }
}

#include "moc_polarchartaxisradial_p.cpp"

QT_CHARTS_END_NAMESPACE
