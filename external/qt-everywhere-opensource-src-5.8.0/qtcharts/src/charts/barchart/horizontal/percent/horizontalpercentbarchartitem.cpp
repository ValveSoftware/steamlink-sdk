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

#include <private/horizontalpercentbarchartitem_p.h>
#include <private/qabstractbarseries_p.h>
#include <private/qbarset_p.h>
#include <private/bar_p.h>

QT_CHARTS_BEGIN_NAMESPACE

HorizontalPercentBarChartItem::HorizontalPercentBarChartItem(QAbstractBarSeries *series, QGraphicsItem* item)
    : AbstractBarChartItem(series, item)
{
}

void HorizontalPercentBarChartItem::initializeLayout()
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
            if (domain()->type() == AbstractDomain::LogXYDomain || domain()->type() == AbstractDomain::LogXLogYDomain) {
                topLeft = domain()->calculateGeometryPoint(QPointF(domain()->minX(), category - barWidth / 2), m_validData);
                bottomRight = domain()->calculateGeometryPoint(QPointF(domain()->minX(), category + barWidth / 2), m_validData);
            } else {
                topLeft = domain()->calculateGeometryPoint(QPointF(0, category - barWidth / 2), m_validData);
                bottomRight = domain()->calculateGeometryPoint(QPointF(0, category + barWidth / 2), m_validData);
            }

            if (!m_validData)
                 return;

            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
            m_layout.append(rect.normalized());
        }
    }
}

QVector<QRectF> HorizontalPercentBarChartItem::calculateLayout()
{
    QVector<QRectF> layout;

    // Use temporary qreals for accuracy
    qreal categoryCount = m_series->d_func()->categoryCount();
    qreal setCount = m_series->count();
    qreal barWidth = m_series->d_func()->barWidth();

    for(int category = 0; category < categoryCount; category++) {
        qreal sum = 0;
        qreal categorySum = m_series->d_func()->categorySum(category);
        for (int set = 0; set < setCount; set++) {
            qreal value = m_series->barSets().at(set)->at(category);
            QRectF rect;
            qreal topX = 0;
            if (sum > 0)
                topX = 100 * sum / categorySum;
            qreal bottomX = 0;
            qreal newSum = value + sum;
            if (newSum > 0)
                bottomX = 100 * newSum / categorySum;
            QPointF topLeft;
            if (domain()->type() == AbstractDomain::LogXYDomain || domain()->type() == AbstractDomain::LogXLogYDomain)
                topLeft = domain()->calculateGeometryPoint(QPointF(set ? topX : domain()->minX(), category - barWidth/2), m_validData);
            else
                topLeft = domain()->calculateGeometryPoint(QPointF(set ? topX : 0, category - barWidth/2), m_validData);
            QPointF bottomRight = domain()->calculateGeometryPoint(QPointF(bottomX, category + barWidth/2), m_validData);

            rect.setTopLeft(topLeft);
            rect.setBottomRight(bottomRight);
            layout.append(rect.normalized());
            sum = newSum;
        }
    }
    return layout;
}

void HorizontalPercentBarChartItem::handleUpdatedBars()
{
    // Handle changes in pen, brush, labels etc.
    int categoryCount = m_series->d_func()->categoryCount();
    int setCount = m_series->count();
    int itemIndex(0);
    static const QString valueTag(QLatin1String("@value"));

    for (int category = 0; category < categoryCount; category++) {
        for (int set = 0; set < setCount; set++) {
            QBarSetPrivate *barSet = m_series->d_func()->barsetAt(set)->d_ptr.data();
            Bar *bar = m_bars.at(itemIndex);
            bar->setPen(barSet->m_pen);
            bar->setBrush(barSet->m_brush);
            bar->update();

            QGraphicsTextItem *label = m_labels.at(itemIndex);
            qreal p = m_series->d_func()->percentageAt(set, category) * 100.0;
            QString vString(presenter()->numberToString(p, 'f', 0));
            QString valueLabel;
            if (p == 0) {
                label->setVisible(false);
            } else {
                label->setVisible(m_series->isLabelsVisible());
                if (m_series->labelsFormat().isEmpty()) {
                    vString.append(QStringLiteral("%"));
                    valueLabel = vString;
                } else {
                    valueLabel = m_series->labelsFormat();
                    valueLabel.replace(valueTag, vString);
                }
            }
            label->setHtml(valueLabel);
            label->setFont(barSet->m_labelFont);
            label->setDefaultTextColor(barSet->m_labelBrush.color());
            label->update();
            itemIndex++;
        }
    }
}

#include "moc_horizontalpercentbarchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE

