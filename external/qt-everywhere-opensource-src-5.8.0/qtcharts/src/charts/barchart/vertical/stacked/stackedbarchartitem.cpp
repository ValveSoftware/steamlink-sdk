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

#include <private/stackedbarchartitem_p.h>
#include <private/bar_p.h>
#include <private/qbarset_p.h>
#include <private/qabstractbarseries_p.h>
#include <QtCharts/QBarSet>

QT_CHARTS_BEGIN_NAMESPACE

StackedBarChartItem::StackedBarChartItem(QAbstractBarSeries *series, QGraphicsItem* item) :
    AbstractBarChartItem(series, item)
{
    connect(series, SIGNAL(labelsPositionChanged(QAbstractBarSeries::LabelsPosition)),
            this, SLOT(handleLabelsPositionChanged()));
    connect(series, SIGNAL(labelsFormatChanged(QString)), this, SLOT(positionLabels()));
}

void StackedBarChartItem::initializeLayout()
{
    qreal categoryCount = m_series->d_func()->categoryCount();
    qreal setCount = m_series->count();
    qreal barWidth = m_series->d_func()->barWidth();

    m_layout.clear();
    for(int category = 0; category < categoryCount; category++) {
        for (int set = 0; set < setCount; set++) {
            QRectF rect;
            QPointF topLeft;
            QPointF bottomRight;

            if (domain()->type() == AbstractDomain::XLogYDomain || domain()->type() == AbstractDomain::LogXLogYDomain) {
                topLeft = domain()->calculateGeometryPoint(QPointF(category - barWidth / 2, domain()->minY()), m_validData);
                bottomRight = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, domain()->minY()), m_validData);
            } else {
                topLeft = domain()->calculateGeometryPoint(QPointF(category - barWidth / 2, 0), m_validData);
                bottomRight = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, 0), m_validData);
            }

            if (!m_validData)
                 return;

            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
            m_layout.append(rect.normalized());
        }
    }
}

QVector<QRectF> StackedBarChartItem::calculateLayout()
{
    QVector<QRectF> layout;
    // Use temporary qreals for accuracy
    qreal categoryCount = m_series->d_func()->categoryCount();
    qreal setCount = m_series->count();
    qreal barWidth = m_series->d_func()->barWidth();

    for(int category = 0; category < categoryCount; category++) {
        qreal positiveSum = 0;
        qreal negativeSum = 0;
        for (int set = 0; set < setCount; set++) {
            qreal value = m_series->barSets().at(set)->at(category);
            QRectF rect;
            QPointF topLeft;
            QPointF bottomRight;
            if (value < 0) {
                bottomRight = domain()->calculateGeometryPoint(QPointF(category - barWidth / 2, value + negativeSum), m_validData);
                if (domain()->type() == AbstractDomain::XLogYDomain || domain()->type() == AbstractDomain::LogXLogYDomain)
                    topLeft = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, set ? negativeSum : domain()->minY()), m_validData);
                else
                    topLeft = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, set ? negativeSum : 0), m_validData);
                negativeSum += value;
            } else {
                topLeft = domain()->calculateGeometryPoint(QPointF(category - barWidth / 2, value + positiveSum), m_validData);
                if (domain()->type() == AbstractDomain::XLogYDomain || domain()->type() == AbstractDomain::LogXLogYDomain)
                    bottomRight = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, set ? positiveSum : domain()->minY()), m_validData);
                else
                    bottomRight = domain()->calculateGeometryPoint(QPointF(category + barWidth / 2, set ? positiveSum : 0), m_validData);
                positiveSum += value;
            }

            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
            layout.append(rect.normalized());
        }
    }
    return layout;
}

void StackedBarChartItem::handleLabelsPositionChanged()
{
    positionLabels();
}

void StackedBarChartItem::positionLabels()
{
    positionLabelsVertical();
}

#include "moc_stackedbarchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
