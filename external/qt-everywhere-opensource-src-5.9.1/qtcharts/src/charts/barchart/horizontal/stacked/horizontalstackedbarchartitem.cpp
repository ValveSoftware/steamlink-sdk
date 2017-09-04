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

#include <private/horizontalstackedbarchartitem_p.h>
#include <private/qabstractbarseries_p.h>
#include <private/qbarset_p.h>
#include <private/bar_p.h>

QT_CHARTS_BEGIN_NAMESPACE

HorizontalStackedBarChartItem::HorizontalStackedBarChartItem(QAbstractBarSeries *series, QGraphicsItem* item)
    : AbstractBarChartItem(series, item)
{
}

void HorizontalStackedBarChartItem::initializeLayout(int set, int category,
                                                     int layoutIndex, bool resetAnimation)
{
    Q_UNUSED(set)
    Q_UNUSED(resetAnimation)

    QRectF rect;
    if (set > 0) {
        const QBarSet *barSet = m_series->barSets().at(set);
        const qreal value = barSet->at(category);
        int checkIndex = set;
        bool found = false;
        // Negative values stack to negative side and positive values to positive side, so we need
        // to find the previous set that stacks to the same side
        while (checkIndex > 0 && !found) {
            checkIndex--;
            QBarSet *checkSet = m_series->barSets().at(checkIndex);
            const qreal checkValue = checkSet->at(category);
            if ((value < 0.0) == (checkValue < 0.0)) {
                Bar *checkBar = m_indexForBarMap.value(checkSet).value(category);
                rect = m_layout.at(checkBar->layoutIndex());
                found = true;
                break;
            }
        }
        // If we didn't find a previous set to the same direction, just stack next to the first set
        if (!found) {
            QBarSet *firsSet = m_series->barSets().at(0);
            Bar *firstBar = m_indexForBarMap.value(firsSet).value(category);
            rect = m_layout.at(firstBar->layoutIndex());
        }
        if (value < 0)
            rect.setRight(rect.left());
        else
            rect.setLeft(rect.right());
    } else {
        QPointF topLeft;
        QPointF bottomRight;
        const qreal barWidth = m_series->d_func()->barWidth() * m_seriesWidth;

        if (domain()->type() == AbstractDomain::LogXYDomain
                || domain()->type() == AbstractDomain::LogXLogYDomain) {
            topLeft = topLeftPoint(category, barWidth, domain()->minX());
            bottomRight = bottomRightPoint(category, barWidth, domain()->minX());
        } else {
            topLeft = topLeftPoint(category, barWidth, 0.0);
            bottomRight = bottomRightPoint(category, barWidth, 0.0);
        }

        if (m_validData) {
            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
        }
    }
    m_layout[layoutIndex] = rect.normalized();
}

QPointF HorizontalStackedBarChartItem::topLeftPoint(int category, qreal barWidth, qreal value)
{
    return domain()->calculateGeometryPoint(
                QPointF(value, m_seriesPosAdjustment + category - (barWidth / 2)), m_validData);
}

QPointF HorizontalStackedBarChartItem::bottomRightPoint(int category, qreal barWidth, qreal value)
{
    return domain()->calculateGeometryPoint(
                QPointF(value, m_seriesPosAdjustment + category + (barWidth / 2)), m_validData);
}

QVector<QRectF> HorizontalStackedBarChartItem::calculateLayout()
{
    QVector<QRectF> layout;
    layout.resize(m_layout.size());

    const int setCount = m_series->count();
    const qreal barWidth = m_series->d_func()->barWidth() * m_seriesWidth;

    QVector<qreal> positiveSums(m_categoryCount, 0.0);
    QVector<qreal> negativeSums(m_categoryCount, 0.0);

    for (int set = 0; set < setCount; set++) {
        QBarSet *barSet = m_series->barSets().at(set);
        const QList<Bar *> bars = m_barMap.value(barSet);
        for (int i = 0; i < m_categoryCount; i++) {
            Bar *bar = bars.at(i);
            const int category = bar->index();
            qreal &positiveSum = positiveSums[category - m_firstCategory];
            qreal &negativeSum = negativeSums[category - m_firstCategory];
            qreal value = barSet->at(category);
            QRectF rect;
            QPointF topLeft;
            QPointF bottomRight;
            if (value < 0) {
                bottomRight = bottomRightPoint(category, barWidth, value + negativeSum);
                if (domain()->type() == AbstractDomain::XLogYDomain
                        || domain()->type() == AbstractDomain::LogXLogYDomain) {
                    topLeft = topLeftPoint(category, barWidth,
                                           set ? negativeSum : domain()->minX());
                } else {
                    topLeft = topLeftPoint(category, barWidth, set ? negativeSum : 0.0);
                }
                negativeSum += value;
            } else {
                bottomRight = bottomRightPoint(category, barWidth, value + positiveSum);
                if (domain()->type() == AbstractDomain::XLogYDomain
                        || domain()->type() == AbstractDomain::LogXLogYDomain) {
                    topLeft = topLeftPoint(category, barWidth,
                                           set ? positiveSum : domain()->minX());
                } else {
                    topLeft = topLeftPoint(category, barWidth,
                                           set ? positiveSum : 0.0);
                }
                positiveSum += value;
            }

            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
            rect = rect.normalized();
            layout[bar->layoutIndex()] = rect;

            // If animating, we need to reinitialize ~zero size bars with non-zero values
            // so the bar growth animation starts at correct spot. We shouldn't reset if rect
            // is already at correct position horizontally, so we check for that.
            if (m_animation && value != 0.0) {
                const QRectF &checkRect = m_layout.at(bar->layoutIndex());
                if (checkRect.isEmpty() &&
                        ((value < 0.0 && !qFuzzyCompare(checkRect.right(), rect.right()))
                         || (value > 0.0 && !qFuzzyCompare(checkRect.left(), rect.left())))) {
                    initializeLayout(set, category, bar->layoutIndex(), true);
                }
            }
        }
    }
    return layout;
}

#include "moc_horizontalstackedbarchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE

