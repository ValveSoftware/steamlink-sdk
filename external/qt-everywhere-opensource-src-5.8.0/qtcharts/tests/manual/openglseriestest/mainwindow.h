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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "datasource.h"
#include <QtWidgets/QMainWindow>
#include <QtCharts/QChart>

QT_BEGIN_NAMESPACE
class QBrush;
class QPen;

namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE


QT_CHARTS_USE_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void xMinChanged(double value);
    void xMaxChanged(double value);
    void yMinChanged(double value);
    void yMaxChanged(double value);
    void animationIndexChanged(int index);
    void xRangeChanged(qreal min, qreal max);
    void yRangeChanged(qreal min, qreal max);
    void xAxisIndexChanged(int index);
    void yAxisIndexChanged(int index);
    void backgroundIndexChanged(int index);
    void plotAreaIndexChanged(int index);
    void themeIndexChanged(int index);
    void addSeriesClicked();
    void removeSeriesClicked();
    void addGLSeriesClicked();
    void countIndexChanged(int index);
    void colorIndexChanged(int index);
    void widthIndexChanged(int index);
    void antiAliasCheckBoxClicked(bool checked);
    void handleHovered(const QPointF &point, bool state);
    void handleClicked(const QPointF &point);
    void handlePressed(const QPointF &point);
    void handleReleased(const QPointF &point);
    void handleDoubleClicked(const QPointF &point);

private:
    enum AxisMode {
        AxisModeNone,
        AxisModeValue,
        AxisModeLogValue,
        AxisModeDateTime,
        AxisModeCategory
    };

    void initXYValueChart();
    void setXAxis(AxisMode mode);
    void setYAxis(AxisMode mode);

    void applyRanges();
    void applyCategories();
    void addSeries(bool gl);

    Ui::MainWindow *ui;

    qreal m_xMin;
    qreal m_xMax;
    qreal m_yMin;
    qreal m_yMax;
    QBrush *m_backgroundBrush;
    QBrush *m_plotAreaBackgroundBrush;
    QPen *m_backgroundPen;
    QPen *m_plotAreaBackgroundPen;
    QChart::AnimationOptions m_animationOptions;

    QChart *m_chart;
    QAbstractAxis *m_xAxis;
    QAbstractAxis *m_yAxis;
    AxisMode m_xAxisMode;
    AxisMode m_yAxisMode;

    QList<QXYSeries *> m_seriesList;
    DataSource m_dataSource;
    int m_pointCount;
};

#endif // MAINWINDOW_H
