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
#include <private/chartpresenter_p.h>
#include <private/verticalaxis_p.h>

QT_CHARTS_BEGIN_NAMESPACE

VerticalAxis::VerticalAxis(QAbstractAxis *axis, QGraphicsItem *item, bool intervalAxis)
    : CartesianChartAxis(axis, item, intervalAxis)
{
}

VerticalAxis::~VerticalAxis()
{
}

QSizeF VerticalAxis::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(constraint);
    QSizeF sh(0, 0);

    if (axis()->titleText().isEmpty() || !titleItem()->isVisible())
        return sh;

    switch (which) {
    case Qt::MinimumSize: {
        QRectF titleRect = ChartPresenter::textBoundingRect(axis()->titleFont(),
                                                            QStringLiteral("..."));
        sh = QSizeF(titleRect.height() + (titlePadding() * 2.0), titleRect.width());
        break;
    }
    case Qt::MaximumSize:
    case Qt::PreferredSize: {
        QRectF titleRect = ChartPresenter::textBoundingRect(axis()->titleFont(), axis()->titleText());
        sh = QSizeF(titleRect.height() + (titlePadding() * 2.0), titleRect.width());
        break;
    }
    default:
        break;
    }

    return sh;
}

void VerticalAxis::updateGeometry()
{
    const QVector<qreal> &layout = ChartAxisElement::layout();

    if (layout.isEmpty() && axis()->type() != QAbstractAxis::AxisTypeLogValue)
        return;

    QStringList labelList = labels();

    QList<QGraphicsItem *> labels = labelItems();
    QList<QGraphicsItem *> arrow = arrowItems();
    QGraphicsTextItem *title = titleItem();

    Q_ASSERT(labels.size() == labelList.size());
    Q_ASSERT(layout.size() == labelList.size());

    const QRectF &axisRect = axisGeometry();
    const QRectF &gridRect = gridGeometry();

    qreal height = axisRect.bottom();

    //arrow
    QGraphicsLineItem *arrowItem = static_cast<QGraphicsLineItem*>(arrow.at(0));

    //arrow position
    if (axis()->alignment() == Qt::AlignLeft)
        arrowItem->setLine(axisRect.right(), gridRect.top(), axisRect.right(), gridRect.bottom());
    else if (axis()->alignment() == Qt::AlignRight)
        arrowItem->setLine(axisRect.left(), gridRect.top(), axisRect.left(), gridRect.bottom());

    //title
    QRectF titleBoundingRect;
    QString titleText = axis()->titleText();
    qreal availableSpace = axisRect.height() - labelPadding();
    if (!titleText.isEmpty() && titleItem()->isVisible()) {
        availableSpace -= titlePadding() * 2.0;
        qreal minimumLabelWidth = ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                   QStringLiteral("...")).width();
        qreal titleSpace = availableSpace - minimumLabelWidth;
        title->setHtml(ChartPresenter::truncatedText(axis()->titleFont(), titleText, qreal(90.0),
                                                     titleSpace, gridRect.height(),
                                                     titleBoundingRect));
        title->setTextWidth(titleBoundingRect.height());

        titleBoundingRect = title->boundingRect();

        QPointF center = gridRect.center() - titleBoundingRect.center();
        if (axis()->alignment() == Qt::AlignLeft) {
            title->setPos(axisRect.left() - titleBoundingRect.width() / 2.0
                          + titleBoundingRect.height() / 2.0 + titlePadding(), center.y());
        } else if (axis()->alignment() == Qt::AlignRight) {
            title->setPos(axisRect.right() - titleBoundingRect.width() / 2.0
                          - titleBoundingRect.height() / 2.0 - titlePadding(), center.y());
        }

        title->setTransformOriginPoint(titleBoundingRect.center());
        title->setRotation(270);

        availableSpace -= titleBoundingRect.height();
    }

    QList<QGraphicsItem *> lines = gridItems();
    QList<QGraphicsItem *> shades = shadeItems();

    for (int i = 0; i < layout.size(); ++i) {
        //items
        QGraphicsLineItem *gridItem = static_cast<QGraphicsLineItem *>(lines.at(i));
        QGraphicsLineItem *tickItem = static_cast<QGraphicsLineItem *>(arrow.at(i + 1));
        QGraphicsTextItem *labelItem = static_cast<QGraphicsTextItem *>(labels.at(i));

        //grid line
        if (axis()->isReverse()) {
            gridItem->setLine(gridRect.left(), gridRect.top() + gridRect.bottom() - layout[i],
                              gridRect.right(), gridRect.top() + gridRect.bottom() - layout[i]);
        } else {
            gridItem->setLine(gridRect.left(), layout[i], gridRect.right(), layout[i]);
        }

        //label text wrapping
        QString text;
        if (axis()->isReverse() && axis()->type() != QAbstractAxis::AxisTypeCategory)
            text = labelList.at(labelList.count() - i - 1);
        else
            text = labelList.at(i);

        QRectF boundingRect;
        // don't truncate empty labels
        if (text.isEmpty()) {
            labelItem->setHtml(text);
        } else {
            qreal labelHeight = (axisRect.height() / layout.count()) - (2 * labelPadding());
            QString truncatedText = ChartPresenter::truncatedText(axis()->labelsFont(), text,
                                                                  axis()->labelsAngle(),
                                                                  availableSpace,
                                                                  labelHeight, boundingRect);
            labelItem->setTextWidth(ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                     truncatedText).width());
            labelItem->setHtml(truncatedText);
        }

        //label transformation origin point
        const QRectF &rect = labelItem->boundingRect();
        QPointF center = rect.center();
        labelItem->setTransformOriginPoint(center.x(), center.y());
        qreal widthDiff = rect.width() - boundingRect.width();
        qreal heightDiff = rect.height() - boundingRect.height();

        //ticks and label position
        QPointF labelPos;
        if (axis()->alignment() == Qt::AlignLeft) {
            if (axis()->isReverse()) {
                labelPos = QPointF(axisRect.right() - rect.width() + (widthDiff / 2.0)
                                  - labelPadding(),
                                  gridRect.top() + gridRect.bottom()
                                  - layout[layout.size() - i - 1] - center.y());
                tickItem->setLine(axisRect.right() - labelPadding(),
                                  gridRect.top() + gridRect.bottom() - layout[i],
                                  axisRect.right(),
                                  gridRect.top() + gridRect.bottom() - layout[i]);
            } else {
                labelPos = QPointF(axisRect.right() - rect.width() + (widthDiff / 2.0)
                                  - labelPadding(),
                                  layout[i] - center.y());
                tickItem->setLine(axisRect.right() - labelPadding(), layout[i],
                                  axisRect.right(), layout[i]);
            }
        } else if (axis()->alignment() == Qt::AlignRight) {
            if (axis()->isReverse()) {
                tickItem->setLine(axisRect.left(),
                                  gridRect.top() + gridRect.bottom() - layout[i],
                                  axisRect.left() + labelPadding(),
                                  gridRect.top() + gridRect.bottom() - layout[i]);
                labelPos = QPointF(axisRect.left() + labelPadding() - (widthDiff / 2.0),
                                  gridRect.top() + gridRect.bottom()
                                  - layout[layout.size() - i - 1] - center.y());
            } else {
                labelPos = QPointF(axisRect.left() + labelPadding() - (widthDiff / 2.0),
                                  layout[i] - center.y());
                tickItem->setLine(axisRect.left(), layout[i],
                                  axisRect.left() + labelPadding(), layout[i]);
            }
        }

        //label in between
        bool forceHide = false;
        bool labelOnValue = false;
        if (intervalAxis() && (i + 1) != layout.size()) {
            qreal lowerBound;
            qreal upperBound;
            if (axis()->isReverse()) {
                lowerBound = qMax(gridRect.top() + gridRect.bottom() - layout[i + 1],
                        gridRect.top());
                upperBound = qMin(gridRect.top() + gridRect.bottom() - layout[i],
                                  gridRect.bottom());
            } else {
                lowerBound = qMin(layout[i], gridRect.bottom());
                upperBound = qMax(layout[i + 1], gridRect.top());
            }
            const qreal delta = lowerBound - upperBound;
            if (axis()->type() != QAbstractAxis::AxisTypeCategory) {
                // Hide label in case visible part of the category at the grid edge is too narrow
                if (delta < boundingRect.height()
                    && (lowerBound == gridRect.bottom() || upperBound == gridRect.top())) {
                    forceHide = true;
                } else {
                    labelPos.setY(lowerBound - (delta / 2.0) - center.y());
                }
            } else {
                QCategoryAxis *categoryAxis = static_cast<QCategoryAxis *>(axis());
                if (categoryAxis->labelsPosition() == QCategoryAxis::AxisLabelsPositionCenter) {
                    if (delta < boundingRect.height()
                        && (lowerBound == gridRect.bottom() || upperBound == gridRect.top())) {
                        forceHide = true;
                    } else {
                        labelPos.setY(lowerBound - (delta / 2.0) - center.y());
                    }
                } else if (categoryAxis->labelsPosition()
                           == QCategoryAxis::AxisLabelsPositionOnValue) {
                    labelOnValue = true;
                    if (axis()->isReverse()) {
                        labelPos.setY(gridRect.top() + gridRect.bottom()
                                          - layout[i + 1] - center.y());
                    } else {
                        labelPos.setY(upperBound - center.y());
                    }
                }
            }
        }

        // Round to full pixel via QPoint to avoid one pixel clipping on the edge in some cases
        labelItem->setPos(labelPos.toPoint());

        //label overlap detection - compensate one pixel for rounding errors
        if (axis()->isReverse()) {
            if (forceHide)
                labelItem->setVisible(false);
        } else if (labelItem->pos().y() + boundingRect.height() > height || forceHide ||
            ((labelItem->pos().y() + (heightDiff / 2.0) - 1.0) > axisRect.bottom()
             && !labelOnValue) ||
            (labelItem->pos().y() + (heightDiff / 2.0) < (axisRect.top() - 1.0) && !labelOnValue)) {
            labelItem->setVisible(false);
        }
        else {
            labelItem->setVisible(true);
            height=labelItem->pos().y();
        }

        //shades
        QGraphicsRectItem *shadeItem = 0;
        if (i == 0)
            shadeItem = static_cast<QGraphicsRectItem *>(shades.at(0));
        else if (i % 2)
            shadeItem = static_cast<QGraphicsRectItem *>(shades.at((i / 2) + 1));
        if (shadeItem) {
            qreal lowerBound;
            qreal upperBound;
            if (i == 0) {
                if (axis()->isReverse()) {
                    upperBound = gridRect.top();
                    lowerBound = gridRect.top() + gridRect.bottom() - layout[i];
                } else {
                    lowerBound = gridRect.bottom();
                    upperBound = layout[0];
                }
            } else {
                if (axis()->isReverse()) {
                        upperBound = gridRect.top() + gridRect.bottom() - layout[i];
                    if (i == layout.size() - 1) {
                        lowerBound = gridRect.bottom();
                    } else {
                        lowerBound = qMax(gridRect.top() + gridRect.bottom() - layout[i + 1],
                                gridRect.top());
                    }
                } else {
                    lowerBound = layout[i];
                    if (i == layout.size() - 1)
                        upperBound = gridRect.top();
                    else
                        upperBound = qMax(layout[i + 1], gridRect.top());
                }

            }
            if (lowerBound > gridRect.bottom())
                lowerBound = gridRect.bottom();
            if (upperBound < gridRect.top())
                upperBound = gridRect.top();
            shadeItem->setRect(gridRect.left(), upperBound, gridRect.width(),
                               lowerBound - upperBound);
            if (shadeItem->rect().height() <= 0.0)
                shadeItem->setVisible(false);
            else
                shadeItem->setVisible(true);
        }

        // check if the grid line and the axis tick should be shown
        const bool gridLineVisible = (gridItem->line().p1().y() >= gridRect.top()
                                      && gridItem->line().p1().y() <= gridRect.bottom());
        gridItem->setVisible(gridLineVisible);
        tickItem->setVisible(gridLineVisible);
    }

    updateMinorTickGeometry();

    // begin/end grid line in case labels between
    if (intervalAxis()) {
        QGraphicsLineItem *gridLine;
        gridLine = static_cast<QGraphicsLineItem *>(lines.at(layout.size()));
        gridLine->setLine(gridRect.left(), gridRect.top(), gridRect.right(), gridRect.top());
        gridLine->setVisible(true);
        gridLine = static_cast<QGraphicsLineItem*>(lines.at(layout.size() + 1));
        gridLine->setLine(gridRect.left(), gridRect.bottom(), gridRect.right(), gridRect.bottom());
        gridLine->setVisible(true);
    }
}

void VerticalAxis::updateMinorTickGeometry()
{
    if (!axis())
        return;

    QVector<qreal> layout = ChartAxisElement::layout();
    int minorTickCount = 0;
    qreal tickSpacing = 0.0;
    QVector<qreal> minorTickSpacings;
    switch (axis()->type()) {
    case QAbstractAxis::AxisTypeValue: {
        const QValueAxis *valueAxis = qobject_cast<QValueAxis *>(axis());

        minorTickCount = valueAxis->minorTickCount();

        if (valueAxis->tickCount() >= 2)
            tickSpacing = layout.at(0) - layout.at(1);

        for (int i = 0; i < minorTickCount; ++i) {
            const qreal ratio = (1.0 / qreal(minorTickCount + 1)) * qreal(i + 1);
            minorTickSpacings.append(tickSpacing * ratio);
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
            // Calculate tickSpacing as a difference between visible ticks
            // whenever it is possible. Virtual ticks will not be correctly
            // positioned when the layout is animating.
            tickSpacing = layout.at(0) - layout.at(1);
            layout.prepend(layout.at(0) + tickSpacing);
            layout.append(layout.at(layout.size() - 1) - tickSpacing);
        } else {
            const qreal logMax = qLn(logValueAxis->max());
            const qreal logMin = qLn(logValueAxis->min());
            const qreal logExtraMaxTick = qLn(qPow(base, qFloor(logMax / logBase) + 1.0));
            const qreal logExtraMinTick = qLn(qPow(base, qCeil(logMin / logBase) - 1.0));
            const qreal edge = gridGeometry().bottom();
            const qreal delta = gridGeometry().height() / qAbs(logMax - logMin);
            const qreal extraMaxTick = edge - (logExtraMaxTick - qMin(logMin, logMax)) * delta;
            const qreal extraMinTick = edge - (logExtraMinTick - qMin(logMin, logMax)) * delta;

            // Calculate tickSpacing using one (if layout.size() == 1) or two
            // (if layout.size() == 0) "virtual" ticks. In both cases animation
            // will not work as expected. This should be fixed later.
            layout.prepend(extraMinTick);
            layout.append(extraMaxTick);
            tickSpacing = layout.at(0) - layout.at(1);
        }

        const qreal minorTickStepValue = qFabs(base - 1.0) / qreal(minorTickCount + 1);
        for (int i = 0; i < minorTickCount; ++i) {
            const qreal x = minorTickStepValue * qreal(i + 1) + 1.0;
            const qreal minorTickSpacing = tickSpacing * (qLn(x) / logBase);
            minorTickSpacings.append(minorTickSpacing);
        }
        break;
    }
    default:
        // minor ticks are not supported
        break;
    }

    if (minorTickCount < 1 || tickSpacing == 0.0 || minorTickSpacings.count() != minorTickCount)
        return;

    for (int i = 0; i < layout.size() - 1; ++i) {
        for (int j = 0; j < minorTickCount; ++j) {
            const int minorItemIndex = i * minorTickCount + j;
            QGraphicsLineItem *minorGridLineItem =
                    static_cast<QGraphicsLineItem *>(minorGridItems().value(minorItemIndex));
            QGraphicsLineItem *minorArrowLineItem =
                    static_cast<QGraphicsLineItem *>(minorArrowItems().value(minorItemIndex));
            if (!minorGridLineItem || !minorArrowLineItem)
                continue;

            const qreal minorTickSpacing = minorTickSpacings.value(j, 0.0);

            qreal minorGridLineItemY = 0.0;
            if (axis()->isReverse()) {
                minorGridLineItemY = qFloor(gridGeometry().top() + gridGeometry().bottom()
                                            - layout.at(i) + minorTickSpacing);
            } else {
                minorGridLineItemY = qCeil(layout.at(i) - minorTickSpacing);
            }

            qreal minorArrowLineItemX1;
            qreal minorArrowLineItemX2;
            switch (axis()->alignment()) {
            case Qt::AlignLeft:
                minorArrowLineItemX1 = gridGeometry().left() - labelPadding() / 2.0;
                minorArrowLineItemX2 = gridGeometry().left();
                break;
            case Qt::AlignRight:
                minorArrowLineItemX1 = gridGeometry().right();
                minorArrowLineItemX2 = gridGeometry().right() + labelPadding() / 2.0;
                break;
            default:
                minorArrowLineItemX1 = 0.0;
                minorArrowLineItemX2 = 0.0;
                break;
            }

            minorGridLineItem->setLine(gridGeometry().left(), minorGridLineItemY,
                                       gridGeometry().right(), minorGridLineItemY);
            minorArrowLineItem->setLine(minorArrowLineItemX1, minorGridLineItemY,
                                        minorArrowLineItemX2, minorGridLineItemY);

            // check if the minor grid line and the minor axis arrow should be shown
            bool minorGridLineVisible = (minorGridLineItemY >= gridGeometry().top()
                                         && minorGridLineItemY <= gridGeometry().bottom());
            minorGridLineItem->setVisible(minorGridLineVisible);
            minorArrowLineItem->setVisible(minorGridLineVisible);
        }
    }
}

QT_CHARTS_END_NAMESPACE
