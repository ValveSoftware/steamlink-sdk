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

#include <private/horizontalaxis_p.h>
#include <private/qabstractaxis_p.h>
#include <private/chartpresenter_p.h>
#include <QtCharts/QCategoryAxis>
#include <QtCore/QtMath>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

HorizontalAxis::HorizontalAxis(QAbstractAxis *axis, QGraphicsItem *item, bool intervalAxis)
    : CartesianChartAxis(axis, item, intervalAxis)
{
}

HorizontalAxis::~HorizontalAxis()
{
}

void HorizontalAxis::updateGeometry()
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

    //arrow
    QGraphicsLineItem *arrowItem = static_cast<QGraphicsLineItem *>(arrow.at(0));

    if (axis()->alignment() == Qt::AlignTop)
        arrowItem->setLine(gridRect.left(), axisRect.bottom(), gridRect.right(), axisRect.bottom());
    else if (axis()->alignment() == Qt::AlignBottom)
        arrowItem->setLine(gridRect.left(), axisRect.top(), gridRect.right(), axisRect.top());

    qreal width = 0;
    const QLatin1String ellipsis("...");

    //title
    QRectF titleBoundingRect;
    QString titleText = axis()->titleText();
    qreal availableSpace = axisRect.height() - labelPadding();
    if (!titleText.isEmpty() && titleItem()->isVisible()) {
        availableSpace -= titlePadding() * 2.0;
        qreal minimumLabelHeight = ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                    QStringLiteral("...")).height();
        qreal titleSpace = availableSpace - minimumLabelHeight;
        title->setHtml(ChartPresenter::truncatedText(axis()->titleFont(), titleText, qreal(0.0),
                                                     gridRect.width(), titleSpace,
                                                     titleBoundingRect));
        title->setTextWidth(titleBoundingRect.width());

        titleBoundingRect = title->boundingRect();

        QPointF center = gridRect.center() - titleBoundingRect.center();
        if (axis()->alignment() == Qt::AlignTop)
            title->setPos(center.x(), axisRect.top() + titlePadding());
        else  if (axis()->alignment() == Qt::AlignBottom)
            title->setPos(center.x(), axisRect.bottom() - titleBoundingRect.height() - titlePadding());

        availableSpace -= titleBoundingRect.height();
    }

    if (layout.isEmpty() && axis()->type() == QAbstractAxis::AxisTypeLogValue)
        return;

    QList<QGraphicsItem *> lines = gridItems();
    QList<QGraphicsItem *> shades = shadeItems();
    QList<QGraphicsItem *> minorLines = minorGridItems();
    QList<QGraphicsItem *> minorArrows = minorArrowItems();

    for (int i = 0; i < layout.size(); ++i) {
        //items
        QGraphicsLineItem *gridItem = static_cast<QGraphicsLineItem*>(lines.at(i));
        QGraphicsLineItem *tickItem = static_cast<QGraphicsLineItem*>(arrow.at(i + 1));
        QGraphicsTextItem *labelItem = static_cast<QGraphicsTextItem *>(labels.at(i));

        //grid line
        if (axis()->isReverse()) {
            gridItem->setLine(gridRect.right() - layout[i] + gridRect.left(), gridRect.top(),
                    gridRect.right() - layout[i] + gridRect.left(), gridRect.bottom());
        } else {
            gridItem->setLine(layout[i], gridRect.top(), layout[i], gridRect.bottom());
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
        } else  {
            qreal labelWidth = axisRect.width() / layout.count() - (2 * labelPadding());
            QString truncatedText = ChartPresenter::truncatedText(axis()->labelsFont(), text,
                                                                  axis()->labelsAngle(),
                                                                  labelWidth,
                                                                  availableSpace, boundingRect);
            labelItem->setTextWidth(ChartPresenter::textBoundingRect(axis()->labelsFont(),
                                                                     truncatedText).width());
            labelItem->setHtml(truncatedText);
        }

        //label transformation origin point
        const QRectF& rect = labelItem->boundingRect();
        QPointF center = rect.center();
        labelItem->setTransformOriginPoint(center.x(), center.y());
        qreal heightDiff = rect.height() - boundingRect.height();
        qreal widthDiff = rect.width() - boundingRect.width();

        //ticks and label position
        QPointF labelPos;
        if (axis()->alignment() == Qt::AlignTop) {
            if (axis()->isReverse()) {
                labelPos = QPointF(gridRect.right() - layout[layout.size() - i - 1]
                        + gridRect.left() - center.x(),
                        axisRect.bottom() - rect.height()
                        + (heightDiff / 2.0) - labelPadding());
                tickItem->setLine(gridRect.right() + gridRect.left() - layout[i],
                                  axisRect.bottom(),
                                  gridRect.right() + gridRect.left() - layout[i],
                                  axisRect.bottom() - labelPadding());
            } else {
                labelPos = QPointF(layout[i] - center.x(), axisRect.bottom() - rect.height()
                                  + (heightDiff / 2.0) - labelPadding());
                tickItem->setLine(layout[i], axisRect.bottom(),
                                  layout[i], axisRect.bottom() - labelPadding());
            }
        } else if (axis()->alignment() == Qt::AlignBottom) {
            if (axis()->isReverse()) {
                labelPos = QPointF(gridRect.right() - layout[layout.size() - i - 1]
                        + gridRect.left() - center.x(),
                        axisRect.top() - (heightDiff / 2.0) + labelPadding());
                tickItem->setLine(gridRect.right() + gridRect.left() - layout[i], axisRect.top(),
                                  gridRect.right() + gridRect.left() - layout[i],
                                  axisRect.top() + labelPadding());
            } else {
                labelPos = QPointF(layout[i] - center.x(), axisRect.top() - (heightDiff / 2.0)
                                  + labelPadding());
                tickItem->setLine(layout[i], axisRect.top(),
                                  layout[i], axisRect.top() + labelPadding());
            }
        }

        //label in between
        bool forceHide = false;
        if (intervalAxis() && (i + 1) != layout.size()) {
            qreal leftBound;
            qreal rightBound;
            if (axis()->isReverse()) {
                leftBound = qMax(gridRect.right() + gridRect.left() - layout[i + 1],
                        gridRect.left());
                rightBound = qMin(gridRect.right() + gridRect.left() - layout[i], gridRect.right());
            } else {
                leftBound = qMax(layout[i], gridRect.left());
                rightBound = qMin(layout[i + 1], gridRect.right());
            }
            const qreal delta = rightBound - leftBound;
            if (axis()->type() != QAbstractAxis::AxisTypeCategory) {
                // Hide label in case visible part of the category at the grid edge is too narrow
                if (delta < boundingRect.width()
                    && (leftBound == gridRect.left() || rightBound == gridRect.right())) {
                    forceHide = true;
                } else {
                    labelPos.setX(leftBound + (delta / 2.0) - center.x());
                }
            } else {
                QCategoryAxis *categoryAxis = static_cast<QCategoryAxis *>(axis());
                if (categoryAxis->labelsPosition() == QCategoryAxis::AxisLabelsPositionCenter) {
                    if (delta < boundingRect.width()
                        && (leftBound == gridRect.left() || rightBound == gridRect.right())) {
                        forceHide = true;
                    } else {
                        labelPos.setX(leftBound + (delta / 2.0) - center.x());
                    }
                } else if (categoryAxis->labelsPosition()
                           == QCategoryAxis::AxisLabelsPositionOnValue) {
                    if (axis()->isReverse())
                        labelPos.setX(leftBound - center.x());
                    else
                        labelPos.setX(rightBound - center.x());
                }
            }
        }

        // Round to full pixel via QPoint to avoid one pixel clipping on the edge in some cases
        labelItem->setPos(labelPos.toPoint());

        //label overlap detection - compensate one pixel for rounding errors
        if ((labelItem->pos().x() < width && labelItem->toPlainText() == ellipsis) || forceHide ||
            (labelItem->pos().x() + (widthDiff / 2.0)) < (axisRect.left() - 1.0) ||
            (labelItem->pos().x() + (widthDiff / 2.0) - 1.0) > axisRect.right()) {
            labelItem->setVisible(false);
        } else {
            labelItem->setVisible(true);
            width = boundingRect.width() + labelItem->pos().x();
        }

        //shades
        QGraphicsRectItem *shadeItem = 0;
        if (i == 0)
            shadeItem = static_cast<QGraphicsRectItem *>(shades.at(0));
        else if (i % 2)
            shadeItem = static_cast<QGraphicsRectItem *>(shades.at((i / 2) + 1));
        if (shadeItem) {
            qreal leftBound;
            qreal rightBound;
            if (i == 0) {
                if (axis()->isReverse()) {
                    leftBound = gridRect.right() + gridRect.left() - layout[i];
                    rightBound = gridRect.right();
                } else {
                    leftBound = gridRect.left();
                    rightBound = layout[0];
                }
            } else {
                if (axis()->isReverse()) {
                    rightBound = gridRect.right() + gridRect.left() - layout[i];
                    if (i == layout.size() - 1) {
                        leftBound = gridRect.left();
                    } else {
                        leftBound = qMax(gridRect.right() + gridRect.left() - layout[i + 1],
                                gridRect.left());
                    }
                } else {
                    leftBound = layout[i];
                    if (i == layout.size() - 1)
                        rightBound = gridRect.right();
                    else
                        rightBound = qMin(layout[i + 1], gridRect.right());
                }
            }
            if (leftBound < gridRect.left())
                leftBound = gridRect.left();
            if (rightBound > gridRect.right())
                rightBound = gridRect.right();
            shadeItem->setRect(leftBound, gridRect.top(), rightBound - leftBound,
                               gridRect.height());
            if (shadeItem->rect().width() <= 0.0)
                shadeItem->setVisible(false);
            else
                shadeItem->setVisible(true);
        }

        // check if the grid line and the axis tick should be shown
        qreal x = gridItem->line().p1().x();
        if (x < gridRect.left() || x > gridRect.right()) {
            gridItem->setVisible(false);
            tickItem->setVisible(false);
        } else {
            gridItem->setVisible(true);
            tickItem->setVisible(true);
        }

        // add minor ticks
        QValueAxis *valueAxis = qobject_cast<QValueAxis *>(axis());
        if ((i + 1) != layout.size() && valueAxis) {
            int minorTickCount = valueAxis->minorTickCount();
            if (minorTickCount != 0) {
                qreal minorTickDistance = (layout[i] - layout[i + 1]) / qreal(minorTickCount + 1);
                if (axis()->isReverse())
                    minorTickDistance = -minorTickDistance;
                for (int j = 0; j < minorTickCount; j++) {
                    QGraphicsLineItem *minorGridItem =
                        static_cast<QGraphicsLineItem *>(minorLines.at(i * minorTickCount + j));
                    QGraphicsLineItem *minorArrowItem =
                        static_cast<QGraphicsLineItem *>(minorArrows.at(i * minorTickCount + j));
                    minorGridItem->setLine(gridItem->line().p1().x()
                                           - minorTickDistance * qreal(j + 1),
                                           gridRect.top(),
                                           gridItem->line().p1().x()
                                           - minorTickDistance * qreal(j + 1),
                                           gridRect.bottom());
                    if (axis()->alignment() == Qt::AlignTop) {
                        minorArrowItem->setLine(minorGridItem->line().p1().x(),
                                                axisRect.bottom(),
                                                minorGridItem->line().p1().x(),
                                                axisRect.bottom() - labelPadding() / 2);
                    } else if (axis()->alignment() == Qt::AlignBottom){
                        minorArrowItem->setLine(minorGridItem->line().p1().x(),
                                                axisRect.top(),
                                                minorGridItem->line().p1().x(),
                                                axisRect.top() + labelPadding() / 2);
                    }

                    // check if the minor grid line and the axis tick should be shown
                    qreal minorXPos = minorGridItem->line().p1().x();
                    if (minorXPos < gridRect.left() || minorXPos > gridRect.right()) {
                        minorGridItem->setVisible(false);
                        minorArrowItem->setVisible(false);
                    } else {
                        minorGridItem->setVisible(true);
                        minorArrowItem->setVisible(true);
                    }
                }
            }
        }
    }

    //begin/end grid line in case labels between
    if (intervalAxis()) {
        QGraphicsLineItem *gridLine;
        gridLine = static_cast<QGraphicsLineItem *>(lines.at(layout.size()));
        gridLine->setLine(gridRect.right(), gridRect.top(), gridRect.right(), gridRect.bottom());
        gridLine->setVisible(true);
        gridLine = static_cast<QGraphicsLineItem*>(lines.at(layout.size()+1));
        gridLine->setLine(gridRect.left(), gridRect.top(), gridRect.left(), gridRect.bottom());
        gridLine->setVisible(true);
    }
}

QSizeF HorizontalAxis::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(constraint);
    QSizeF sh(0,0);

    if (axis()->titleText().isEmpty() || !titleItem()->isVisible())
        return sh;

    switch (which) {
    case Qt::MinimumSize: {
        QRectF titleRect = ChartPresenter::textBoundingRect(axis()->titleFont(),
                                                            QStringLiteral("..."));
        sh = QSizeF(titleRect.width(), titleRect.height() + (titlePadding() * 2.0));
        break;
    }
    case Qt::MaximumSize:
    case Qt::PreferredSize: {
        QRectF titleRect = ChartPresenter::textBoundingRect(axis()->titleFont(), axis()->titleText());
        sh = QSizeF(titleRect.width(), titleRect.height() + (titlePadding() * 2.0));
        break;
    }
    default:
        break;
    }

    return sh;
}

QT_CHARTS_END_NAMESPACE
