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

#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QtCharts/QChartView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE
class QGridLayout;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QCandlestickSet;
class QHCandlestickModelMapper;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private:
    QGridLayout *createSeriesControlsLayout();
    QGridLayout *createSetsControlsLayout();
    QGridLayout *createCandlestickControlsLayout();
    QGridLayout *createMiscellaneousControlsLayout();
    QGridLayout *createModelMapperControlsLayout();

    qreal randomValue(int min, int max) const;
    QCandlestickSet *randomSet(qreal timestamp);

    void updateAxes();

private slots:
    void addSeries();
    void removeSeries();
    void removeAllSeries();
    void addSet();
    void insertSet();
    void removeSet();
    void removeAllSets();
    void changeMaximumColumnWidth(double width);
    void changeMinimumColumnWidth(double width);
    void bodyOutlineVisibleToggled(bool visible);
    void capsVisibleToggled(bool visible);
    void changeBodyWidth(double width);
    void changeCapsWidth(double width);
    void customIncreasingColorToggled(bool custom);
    void customDecreasingColorToggled(bool custom);
    void antialiasingToggled(bool enabled);
    void animationToggled(bool enabled);
    void legendToggled(bool visible);
    void titleToggled(bool visible);
    void changeChartTheme(int themeIndex);
    void changeAxisX(int axisXIndex);
    void attachModelMapper();
    void detachModelMapper();

private:
    QChart *m_chart;
    QChartView *m_chartView;
    QAbstractAxis *m_axisX;
    QAbstractAxis *m_axisY;
    qreal m_maximumColumnWidth;
    qreal m_minimumColumnWidth;
    bool m_bodyOutlineVisible;
    bool m_capsVisible;
    qreal m_bodyWidth;
    qreal m_capsWidth;
    bool m_customIncreasingColor;
    bool m_customDecreasingColor;
    QHCandlestickModelMapper *m_hModelMapper;
};

#endif // MAINWIDGET_H
