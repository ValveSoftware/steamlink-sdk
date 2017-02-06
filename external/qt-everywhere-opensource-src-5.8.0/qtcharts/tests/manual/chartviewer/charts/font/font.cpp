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
#include <QtCharts/QLineSeries>

class FontChart: public Chart
{
public:
    FontChart(int fontSize): m_fontSize(fontSize) {};
    QString name() { return QObject::tr("Font") + " " + QString::number(m_fontSize); }
    QString category()  { return QObject::tr("Font"); }
    QString subCategory() { return QString::null; }

    QChart *createChart(const DataTable &table)
    {
        QChart *chart = new QChart();
        chart->setTitle("Font size " + QString::number(m_fontSize));
        QString name("Series ");
        int nameIndex = 0;
        foreach (DataList list, table) {
            QLineSeries *series = new QLineSeries(chart);
            foreach (Data data, list)
                series->append(data.first);
            series->setName(name + QString::number(nameIndex));
            nameIndex++;
            chart->addSeries(series);
        }
        chart->createDefaultAxes();
        QFont font;
        font.setPixelSize(m_fontSize);
        chart->setTitleFont(font);
        chart->axisX()->setLabelsFont(font);
        chart->axisY()->setLabelsFont(font);
        chart->axisX()->setTitleText("Axis X");
        chart->axisY()->setTitleText("Axis Y");
        chart->axisX()->setTitleFont(font);
        chart->axisY()->setTitleFont(font);
        return chart;
    }

private:
    int m_fontSize;
};

class FontChart6: public FontChart
{
public:
    FontChart6(): FontChart(6) {};
};

class FontChart8: public FontChart
{
public:
    FontChart8(): FontChart(8) {};
};

class FontChart10: public FontChart
{
public:
    FontChart10(): FontChart(10) {};
};

class FontChart14: public FontChart
{
public:
    FontChart14(): FontChart(14) {};
};

class FontChart18: public FontChart
{
public:
    FontChart18(): FontChart(18) {};
};

class FontChart20: public FontChart
{
public:
    FontChart20(): FontChart(20) {};
};

class FontChart24: public FontChart
{
public:
    FontChart24(): FontChart(24) {};
};

class FontChart28: public FontChart
{
public:
    FontChart28(): FontChart(28) {};
};

class FontChart32: public FontChart
{
public:
    FontChart32(): FontChart(32) {};
};

DECLARE_CHART(FontChart6);
DECLARE_CHART(FontChart8);
DECLARE_CHART(FontChart10);
DECLARE_CHART(FontChart14);
DECLARE_CHART(FontChart18);
DECLARE_CHART(FontChart20);
DECLARE_CHART(FontChart24);
DECLARE_CHART(FontChart28);
DECLARE_CHART(FontChart32);
