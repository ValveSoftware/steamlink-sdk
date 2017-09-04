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
#include <private/polarchartaxisangular_p.h>

QT_CHARTS_BEGIN_NAMESPACE

PolarChartAxisAngular::PolarChartAxisAngular(QAbstractAxis *axis, QGraphicsItem *item,
                                             bool intervalAxis)
    : PolarChartAxis(axis, item, intervalAxis)
{
}

PolarChartAxisAngular::~PolarChartAxisAngular()
{
}

void PolarChartAxisAngular::updateGeometry()
{
    QGraphicsLayoutItem::updateGeometry();

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
    QGraphicsTextItem *title = titleItem();

    QGraphicsEllipseItem *axisLine = static_cast<QGraphicsEllipseItem *>(arrowItemList.at(0));
    axisLine->setRect(axisGeometry());

    qreal radius = axisGeometry().height() / 2.0;

    QRectF previousLabelRect;
    QRectF firstLabelRect;

    qreal labelHeight = 0;

    bool firstShade = true;
    bool nextTickVisible = false;
    if (layout.size())
        nextTickVisible = !(layout.at(0) < 0.0 || layout.at(0) > 360.0);

    for (int i = 0; i < layout.size(); ++i) {
        qreal angularCoordinate = layout.at(i);

        QGraphicsLineItem *gridLineItem = static_cast<QGraphicsLineItem *>(gridItemList.at(i));
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
            || layout.at(i + 1) > 360.0) {
            nextTickVisible = false;
        } else {
            nextTickVisible = true;
        }

        qreal labelCoordinate = angularCoordinate;
        bool labelVisible = currentTickVisible;
        if (intervalAxis()) {
            qreal farEdge;
            if (i == (layout.size() - 1))
                farEdge = 360.0;
            else
                farEdge = qMin(qreal(360.0), layout.at(i + 1));

            // Adjust the labelCoordinate to show it if next tick is visible
            if (nextTickVisible)
                labelCoordinate = qMax(qreal(0.0), labelCoordinate);

            bool centeredLabel = true;
            if (axis()->type() == QAbstractAxis::AxisTypeCategory) {
                QCategoryAxis *categoryAxis = static_cast<QCategoryAxis *>(axis());
                if (categoryAxis->labelsPosition() == QCategoryAxis::AxisLabelsPositionOnValue)
                    centeredLabel = false;
            }
            if (centeredLabel) {
                labelCoordinate = (labelCoordinate + farEdge) / 2.0;
                // Don't display label once the category gets too small near the axis
                if (labelCoordinate < 5.0 || labelCoordinate > 355.0)
                    labelVisible = false;
                else
                    labelVisible = true;
            } else {
                labelVisible = nextTickVisible;
                labelCoordinate = farEdge;
            }
        }

        // Need this also in label calculations, so determine it first
        QLineF tickLine(QLineF::fromPolar(radius - tickWidth(), 90.0 - angularCoordinate).p2(),
                        QLineF::fromPolar(radius + tickWidth(), 90.0 - angularCoordinate).p2());
        tickLine.translate(center);

        // Angular axis label
        if (axis()->labelsVisible() && labelVisible) {
            QRectF boundingRect = ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                   labelList.at(i),
                                                                   axis()->labelsAngle());
            labelItem->setTextWidth(boundingRect.width());
            labelItem->setHtml(labelList.at(i));
            const QRectF &rect = labelItem->boundingRect();
            QPointF labelCenter = rect.center();
            labelItem->setTransformOriginPoint(labelCenter.x(), labelCenter.y());
            boundingRect.moveCenter(labelCenter);
            QPointF positionDiff(rect.topLeft() - boundingRect.topLeft());

            QPointF labelPoint;
            if (intervalAxis()) {
                QLineF labelLine = QLineF::fromPolar(radius + tickWidth(), 90.0 - labelCoordinate);
                labelLine.translate(center);
                labelPoint = labelLine.p2();
            } else {
                labelPoint = tickLine.p2();
            }

            QRectF labelRect = moveLabelToPosition(labelCoordinate, labelPoint, boundingRect);
            labelItem->setPos(labelRect.topLeft() + positionDiff);

            // Store height for title calculations
            qreal labelClearance = axisGeometry().top() - labelRect.top();
            labelHeight = qMax(labelHeight, labelClearance);

            // Label overlap detection
            if (i && (previousLabelRect.intersects(labelRect) || firstLabelRect.intersects(labelRect))) {
                labelVisible = false;
            } else {
                // Store labelRect for future comparison. Some area is deducted to make things look
                // little nicer, as usually intersection happens at label corner with angular labels.
                labelRect.adjust(-2.0, -4.0, -2.0, -4.0);
                if (firstLabelRect.isEmpty())
                    firstLabelRect = labelRect;

                previousLabelRect = labelRect;
                labelVisible = true;
            }
        }

        labelItem->setVisible(labelVisible);
        if (!currentTickVisible) {
            gridLineItem->setVisible(false);
            tickItem->setVisible(false);
            if (shadeItem)
                shadeItem->setVisible(false);
            continue;
        }

        // Angular grid line
        QLineF gridLine = QLineF::fromPolar(radius, 90.0 - angularCoordinate);
        gridLine.translate(center);
        gridLineItem->setLine(gridLine);
        gridLineItem->setVisible(true);

        // Tick
        tickItem->setLine(tickLine);
        tickItem->setVisible(true);

        // Shades
        if (i % 2 || (i == 0 && !nextTickVisible)) {
            QPainterPath path;
            path.moveTo(center);
            if (i == 0) {
                // If first tick is also the last, we need to custom fill the first partial arc
                // or it won't get filled.
                path.arcTo(axisGeometry(), 90.0 - layout.at(0), layout.at(0));
                path.closeSubpath();
            } else {
                qreal nextCoordinate;
                if (!nextTickVisible) // Last visible tick
                    nextCoordinate = 360.0;
                else
                    nextCoordinate = layout.at(i + 1);
                qreal arcSpan = angularCoordinate - nextCoordinate;
                path.arcTo(axisGeometry(), 90.0 - angularCoordinate, arcSpan);
                path.closeSubpath();

                // Add additional arc for first shade item if there is a partial arc to be filled
                if (firstShade) {
                    QGraphicsPathItem *specialShadeItem = static_cast<QGraphicsPathItem *>(shadeItemList.at(0));
                    if (layout.at(i - 1) > 0.0) {
                        QPainterPath specialPath;
                        specialPath.moveTo(center);
                        specialPath.arcTo(axisGeometry(), 90.0 - layout.at(i - 1), layout.at(i - 1));
                        specialPath.closeSubpath();
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

    // Title, centered above the chart
    QString titleText = axis()->titleText();
    if (!titleText.isEmpty() && axis()->isTitleVisible()) {
        QRectF truncatedRect;
        qreal availableTitleHeight = axisGeometry().height() - labelPadding() - titlePadding() * 2.0;
        qreal minimumLabelHeight = ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                    QStringLiteral("...")).height();
        availableTitleHeight -= minimumLabelHeight;
        title->setHtml(ChartPresenter::truncatedText(axis()->titleFont(), titleText, qreal(0.0),
                                                     axisGeometry().width(), availableTitleHeight,
                                                     truncatedRect));
        title->setTextWidth(truncatedRect.width());

        QRectF titleBoundingRect = title->boundingRect();
        QPointF titleCenter = center - titleBoundingRect.center();
        title->setPos(titleCenter.x(), axisGeometry().top() - titlePadding() * 2.0 - titleBoundingRect.height() - labelHeight);
    }
}

Qt::Orientation PolarChartAxisAngular::orientation() const
{
    return Qt::Horizontal;
}

void PolarChartAxisAngular::createItems(int count)
{
    if (arrowItems().count() == 0) {
        // angular axis line
        QGraphicsEllipseItem *arrow = new QGraphicsEllipseItem(presenter()->rootItem());
        arrow->setPen(axis()->linePen());
        arrowGroup()->addToGroup(arrow);
    }

    QGraphicsTextItem *title = titleItem();
    title->setFont(axis()->titleFont());
    title->setDefaultTextColor(axis()->titleBrush().color());
    title->setHtml(axis()->titleText());

    for (int i = 0; i < count; ++i) {
        QGraphicsLineItem *arrow = new QGraphicsLineItem(presenter()->rootItem());
        QGraphicsLineItem *grid = new QGraphicsLineItem(presenter()->rootItem());
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

void PolarChartAxisAngular::handleArrowPenChanged(const QPen &pen)
{
    bool first = true;
    foreach (QGraphicsItem *item, arrowItems()) {
        if (first) {
            first = false;
            // First arrow item is the outer circle of axis
            static_cast<QGraphicsEllipseItem *>(item)->setPen(pen);
        } else {
            static_cast<QGraphicsLineItem *>(item)->setPen(pen);
        }
    }
}

void PolarChartAxisAngular::handleGridPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, gridItems())
        static_cast<QGraphicsLineItem *>(item)->setPen(pen);
}

void PolarChartAxisAngular::handleMinorArrowPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, minorArrowItems()) {
        static_cast<QGraphicsLineItem *>(item)->setPen(pen);
    }
}

void PolarChartAxisAngular::handleMinorGridPenChanged(const QPen &pen)
{
    foreach (QGraphicsItem *item, minorGridItems())
        static_cast<QGraphicsLineItem *>(item)->setPen(pen);
}

void PolarChartAxisAngular::handleGridLineColorChanged(const QColor &color)
{
    foreach (QGraphicsItem *item, gridItems()) {
        QGraphicsLineItem *lineItem = static_cast<QGraphicsLineItem *>(item);
        QPen pen = lineItem->pen();
        pen.setColor(color);
        lineItem->setPen(pen);
    }
}

void PolarChartAxisAngular::handleMinorGridLineColorChanged(const QColor &color)
{
    foreach (QGraphicsItem *item, minorGridItems()) {
        QGraphicsLineItem *lineItem = static_cast<QGraphicsLineItem *>(item);
        QPen pen = lineItem->pen();
        pen.setColor(color);
        lineItem->setPen(pen);
    }
}

QSizeF PolarChartAxisAngular::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(which);
    Q_UNUSED(constraint);
    return QSizeF(-1, -1);
}

qreal PolarChartAxisAngular::preferredAxisRadius(const QSizeF &maxSize)
{
    qreal radius = maxSize.height() / 2.0;
    if (maxSize.width() < maxSize.height())
        radius = maxSize.width() / 2.0;

    if (axis()->labelsVisible()) {
        QVector<qreal> layout = calculateLayout();
        if (layout.isEmpty())
            return radius;

        createAxisLabels(layout);
        QStringList labelList = labels();
        QFont font = axis()->labelsFont();

        QRectF maxRect;
        maxRect.setSize(maxSize);
        maxRect.moveCenter(QPointF(0.0, 0.0));

        // This is a horrible way to find out the maximum radius for angular axis and its labels.
        // It just increments the radius down until everyhing fits the constraint size.
        // Proper way would be to actually calculate it but this seems to work reasonably fast as it is.
        bool nextTickVisible = false;
        for (int i = 0; i < layout.size(); ) {
            if ((i == layout.size() - 1)
                || layout.at(i + 1) < 0.0
                || layout.at(i + 1) > 360.0) {
                nextTickVisible = false;
            } else {
                nextTickVisible = true;
            }

            qreal labelCoordinate = layout.at(i);
            qreal labelVisible;

            if (intervalAxis()) {
                qreal farEdge;
                if (i == (layout.size() - 1))
                    farEdge = 360.0;
                else
                    farEdge = qMin(qreal(360.0), layout.at(i + 1));

                // Adjust the labelCoordinate to show it if next tick is visible
                if (nextTickVisible)
                    labelCoordinate = qMax(qreal(0.0), labelCoordinate);

                labelCoordinate = (labelCoordinate + farEdge) / 2.0;
            }

            if (labelCoordinate < 0.0 || labelCoordinate > 360.0)
                labelVisible = false;
            else
                labelVisible = true;

            if (!labelVisible) {
                i++;
                continue;
            }

            QRectF boundingRect = ChartPresenter::textBoundingRect(axis()->labelsFont(), labelList.at(i), axis()->labelsAngle());
            QPointF labelPoint = QLineF::fromPolar(radius + tickWidth(), 90.0 - labelCoordinate).p2();

            boundingRect = moveLabelToPosition(labelCoordinate, labelPoint, boundingRect);
            QRectF intersectRect = maxRect.intersected(boundingRect);
            if (boundingRect.isEmpty() || intersectRect == boundingRect) {
                i++;
            } else {
                qreal reduction(0.0);
                // If there is no intersection, reduce by smallest dimension of label rect to be on the safe side
                if (intersectRect.isEmpty()) {
                    reduction = qMin(boundingRect.height(), boundingRect.width());
                } else {
                    // Approximate needed radius reduction is the amount label rect exceeds max rect in either dimension.
                    // Could be further optimized by figuring out the proper math how to calculate exact needed reduction.
                    reduction = qMax(boundingRect.height() - intersectRect.height(),
                                     boundingRect.width() - intersectRect.width());
                }
                // Typically the approximated reduction is little low, so add one
                radius -= (reduction + 1.0);

                if (radius < 1.0) // safeguard
                    return 1.0;
            }
        }
    }

    if (!axis()->titleText().isEmpty() && axis()->isTitleVisible()) {
        QRectF titleRect = ChartPresenter::textBoundingRect(axis()->titleFont(), axis()->titleText());

        radius -= titlePadding() + (titleRect.height() / 2.0);
        if (radius < 1.0) // safeguard
            return 1.0;
    }

    return radius;
}

QRectF PolarChartAxisAngular::moveLabelToPosition(qreal angularCoordinate, QPointF labelPoint, QRectF labelRect) const
{
    if (angularCoordinate == 0.0)
        labelRect.moveCenter(labelPoint + QPointF(0, -labelRect.height() / 2.0));
    else if (angularCoordinate < 90.0)
        labelRect.moveBottomLeft(labelPoint);
    else if (angularCoordinate == 90.0)
        labelRect.moveCenter(labelPoint + QPointF(labelRect.width() / 2.0 + 2.0, 0)); // +2 so that it does not hit the radial axis
    else if (angularCoordinate < 180.0)
        labelRect.moveTopLeft(labelPoint);
    else if (angularCoordinate == 180.0)
        labelRect.moveCenter(labelPoint + QPointF(0, labelRect.height() / 2.0));
    else if (angularCoordinate < 270.0)
        labelRect.moveTopRight(labelPoint);
    else if (angularCoordinate == 270.0)
        labelRect.moveCenter(labelPoint + QPointF(-labelRect.width() / 2.0 - 2.0, 0)); // -2 so that it does not hit the radial axis
    else if (angularCoordinate < 360.0)
        labelRect.moveBottomRight(labelPoint);
    else
        labelRect.moveCenter(labelPoint + QPointF(0, -labelRect.height() / 2.0));
    return labelRect;
}

void PolarChartAxisAngular::updateMinorTickGeometry()
{
    if (!axis())
        return;

    QVector<qreal> layout = ChartAxisElement::layout();
    int minorTickCount = 0;
    qreal tickAngle = 0.0;
    QVector<qreal> minorTickAngles;
    switch (axis()->type()) {
    case QAbstractAxis::AxisTypeValue: {
        const QValueAxis *valueAxis = qobject_cast<QValueAxis *>(axis());

        minorTickCount = valueAxis->minorTickCount();

        if (valueAxis->tickCount() >= 2)
            tickAngle = layout.at(1) - layout.at(0);

        for (int i = 0; i < minorTickCount; ++i) {
            const qreal ratio = (1.0 / qreal(minorTickCount + 1)) * qreal(i + 1);
            minorTickAngles.append(tickAngle * ratio);
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
            // Calculate tickAngle as a difference between visible ticks
            // whenever it is possible. Virtual ticks will not be correctly
            // positioned when the layout is animating.
            tickAngle = layout.at(1) - layout.at(0);
            layout.prepend(layout.at(0) - tickAngle);
            layout.append(layout.at(layout.size() - 1) + tickAngle);
        } else {
            const qreal logMax = qLn(logValueAxis->max());
            const qreal logMin = qLn(logValueAxis->min());
            const qreal logExtraMaxTick = qLn(qPow(base, qFloor(logMax / logBase) + 1.0));
            const qreal logExtraMinTick = qLn(qPow(base, qCeil(logMin / logBase) - 1.0));
            const qreal edge = qMin(logMin, logMax);
            const qreal delta = 360.0 / qAbs(logMax - logMin);
            const qreal extraMaxTick = edge + (logExtraMaxTick - edge) * delta;
            const qreal extraMinTick = edge + (logExtraMinTick - edge) * delta;

            // Calculate tickAngle using one (if layout.size() == 1) or two
            // (if layout.size() == 0) "virtual" ticks. In both cases animation
            // will not work as expected. This should be fixed later.
            layout.prepend(extraMinTick);
            layout.append(extraMaxTick);
            tickAngle = layout.at(1) - layout.at(0);
        }

        const qreal minorTickStepValue = qFabs(base - 1.0) / qreal(minorTickCount + 1);
        for (int i = 0; i < minorTickCount; ++i) {
            const qreal x = minorTickStepValue * qreal(i + 1) + 1.0;
            const qreal minorTickAngle = tickAngle * (qLn(x) / logBase);
            minorTickAngles.append(minorTickAngle);
        }
        break;
    }
    default:
        // minor ticks are not supported
        break;
    }

    if (minorTickCount < 1 || tickAngle == 0.0 || minorTickAngles.count() != minorTickCount)
        return;

    const QPointF axisCenter = axisGeometry().center();
    const qreal axisRadius = axisGeometry().height() / 2.0;

    for (int i = 0; i < layout.size() - 1; ++i) {
        for (int j = 0; j < minorTickCount; ++j) {
            const int minorItemIndex = i * minorTickCount + j;
            QGraphicsLineItem *minorGridLineItem =
                    static_cast<QGraphicsLineItem *>(minorGridItems().at(minorItemIndex));
            QGraphicsLineItem *minorArrowLineItem =
                    static_cast<QGraphicsLineItem *>(minorArrowItems().at(minorItemIndex));
            if (!minorGridLineItem || !minorArrowLineItem)
                continue;

            const qreal minorTickAngle = 90.0 - layout.at(i) - minorTickAngles.value(j, 0.0);

            const QPointF minorArrowLinePt1 = QLineF::fromPolar(axisRadius - tickWidth() + 1,
                                                                minorTickAngle).p2();
            const QPointF minorArrowLinePt2 = QLineF::fromPolar(axisRadius + tickWidth() - 1,
                                                                minorTickAngle).p2();

            QLineF minorGridLine = QLineF::fromPolar(axisRadius, minorTickAngle);
            minorGridLine.translate(axisCenter);
            minorGridLineItem->setLine(minorGridLine);

            QLineF minorArrowLine(minorArrowLinePt1, minorArrowLinePt2);
            minorArrowLine.translate(axisCenter);
            minorArrowLineItem->setLine(minorArrowLine);

            // check if the minor grid line and the minor axis arrow should be shown
            const bool minorGridLineVisible = (minorTickAngle >= -270.0 && minorTickAngle <= 90.0);
            minorGridLineItem->setVisible(minorGridLineVisible);
            minorArrowLineItem->setVisible(minorGridLineVisible);
        }
    }
}

void PolarChartAxisAngular::updateMinorTickItems()
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
            QGraphicsLineItem *minorGridLineItem = new QGraphicsLineItem(this);
            minorGridLineItem->setPen(axis()->minorGridLinePen());
            minorGridGroup()->addToGroup(minorGridLineItem);

            QGraphicsLineItem *minorArrowLineItem = new QGraphicsLineItem(this);
            minorArrowLineItem->setPen(axis()->linePen());
            minorArrowGroup()->addToGroup(minorArrowLineItem);
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


#include "moc_polarchartaxisangular_p.cpp"

QT_CHARTS_END_NAMESPACE
