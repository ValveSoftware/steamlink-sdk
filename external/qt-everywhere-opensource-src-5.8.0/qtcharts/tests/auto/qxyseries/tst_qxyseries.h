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

#ifndef TST_QXYSERIES_H
#define TST_QXYSERIES_H

#include <QtTest/QtTest>
#include <QtCharts/QXYSeries>
#include <QtCharts/QChartView>
#include <QtGui/QStandardItemModel>
#include <tst_definitions.h>

QT_CHARTS_USE_NAMESPACE

class tst_QXYSeries : public QObject
{
    Q_OBJECT

public slots:
    virtual void initTestCase();
    virtual void cleanupTestCase();
    virtual void init();
    virtual void cleanup();

private slots:
    void seriesName();
    void seriesVisible();
    void pointLabelsFormat();
    void pointLabelsVisible();
    void pointLabelsFont();
    void pointLabelsColor();
    void pointLabelsClipping();
    void seriesOpacity();
    void oper_data();
    void oper();
    void pen_data();
    void pen();
    void pointsVisible_raw_data();
    void pointsVisible_raw();
    void append_raw_data();
    void append_raw();
    void append_chart_data();
    void append_chart();
    void append_chart_animation_data();
    void append_chart_animation();
    void chart_append_data();
    void chart_append();
    void count_raw_data();
    void count_raw();
    void remove_raw_data();
    void remove_raw();
    void remove_chart_data();
    void remove_chart();
    void remove_chart_animation_data();
    void remove_chart_animation();
    void clear_raw_data();
    void clear_raw();
    void clear_chart_data();
    void clear_chart();
    void clear_chart_animation_data();
    void clear_chart_animation();
    void replace_raw_data();
    void replace_raw();
    void replace_chart_data();
    void replace_chart();
    void replace_chart_animation_data();
    void replace_chart_animation();
    void insert_data();
    void insert();
    void changedSignals();
protected:
    void append_data();
    void count_data();
    void pointsVisible_data();

protected:
    QChartView* m_view;
    QChart* m_chart;
    QXYSeries* m_series;
};

#endif
