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

#ifndef TST_QABSTRACTAXIS_H
#define TST_QABSTRACTAXIS_H

#include <QtTest/QtTest>
#include <tst_definitions.h>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QChartView>

QT_CHARTS_USE_NAMESPACE

class tst_QAbstractAxis : public QObject
{
    Q_OBJECT

public slots:
    virtual void initTestCase();
    virtual void cleanupTestCase();
    virtual void init(QAbstractAxis* axis,QAbstractSeries* series);
    virtual void cleanup();

private slots:
    void axisPen_data();
    void axisPen();
    void axisPenColor_data();
    void axisPenColor();
    void gridLinePen_data();
    void gridLinePen();
    void minorGridLinePen_data();
    void minorGridLinePen();
    void lineVisible_data();
    void lineVisible();
    void gridLineVisible_data();
    void gridLineVisible();
    void minorGridLineVisible_data();
    void minorGridLineVisible();
    void gridLineColor_data();
    void gridLineColor();
    void minorGridLineColor_data();
    void minorGridLineColor();
    void visible_data();
    void visible();
    void labelsAngle_data();
    void labelsAngle();
    void labelsBrush_data();
    void labelsBrush();
    void labelsColor_data();
    void labelsColor();
    void labelsFont_data();
    void labelsFont();
    void labelsVisible_data();
    void labelsVisible();
    void orientation_data();
    void orientation();
    void setMax_data();
    void setMax();
    void setMin_data();
    void setMin();
    void setRange_data();
    void setRange();
    void shadesBorderColor_data();
    void shadesBorderColor();
    void shadesBrush_data();
    void shadesBrush();
    void shadesColor_data();
    void shadesColor();
    void shadesPen_data();
    void shadesPen();
    void shadesVisible_data();
    void shadesVisible();
    void show_data();
    void show();
    void hide_data();
    void hide();

protected:
    void qabstractaxis();
protected:
    QChartView* m_view;
    QChart* m_chart;
    QAbstractAxis* m_axis;
    QAbstractSeries* m_series;
};

#endif
