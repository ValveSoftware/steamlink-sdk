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

#include "charts.h"
#include <QtCharts/QChart>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLineSeries>

class AreaChart: public Chart
{
public:
    QString name() { return QObject::tr("AreaChart"); }
    QString category()  { return QObject::tr("XYSeries"); }
    QString subCategory() { return QString::null; }

    QChart *createChart(const DataTable &table)
    {
        QChart *chart = new QChart();
        chart->setTitle("Area chart");

        // The lower series initialized to zero values
        QLineSeries *lowerSeries = 0;
        QString name("Series ");
        int nameIndex = 0;
        for (int i(0); i < table.count(); i++) {
            QLineSeries *upperSeries = new QLineSeries(chart);
            for (int j(0); j < table[i].count(); j++) {
                Data data = table[i].at(j);
                if (lowerSeries) {
                    const QList<QPointF>& points = lowerSeries->points();
                    upperSeries->append(QPointF(j, points[i].y() + data.first.y()));
                } else {
                    upperSeries->append(QPointF(j, data.first.y()));
                }
            }
            QAreaSeries *area = new QAreaSeries(upperSeries, lowerSeries);
            area->setName(name + QString::number(nameIndex));
            nameIndex++;
            chart->addSeries(area);
            chart->createDefaultAxes();
            lowerSeries = upperSeries;
        }
        return chart;
    }
};

DECLARE_CHART(AreaChart)

