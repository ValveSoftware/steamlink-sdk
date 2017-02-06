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


#ifndef CHARTS_H
#define CHARTS_H
#include "model.h"
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSharedPointer>
#include <QtCharts/QChartGlobal>

QT_CHARTS_BEGIN_NAMESPACE
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class Chart
{
public:
    virtual ~Chart() {};
    virtual QChart *createChart(const DataTable &table) = 0;
    virtual QString name() = 0;
    virtual QString category() = 0;
    virtual QString subCategory() = 0;

};

namespace Charts
{

    typedef QList<Chart *> ChartList;

    inline ChartList &chartList()
    {
        static ChartList list;
        return list;
    }

    inline bool findChart(Chart *chart)
    {
        ChartList &list = chartList();
        if (list.contains(chart))
            return true;

        foreach (Chart *item, list) {
            if (item->name() == chart->name() && item->category() == chart->category() && item->subCategory() == chart->subCategory())
                return true;
        }
        return false;
    }

    inline void addChart(Chart *chart)
    {
        ChartList &list = chartList();
        if (!findChart(chart))
            list.append(chart);
    }
}

template <class T>
class ChartWrapper
{
public:
    QSharedPointer<T> chart;
    ChartWrapper() : chart(new T) { Charts::addChart(chart.data()); }
};

#define DECLARE_CHART(chartType) static ChartWrapper<chartType> chartType;
#define DECLARE_CHART_TEMPLATE(chartType,chartName) static ChartWrapper<chartType> chartName;

#endif
