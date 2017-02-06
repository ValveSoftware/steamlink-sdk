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

#include <QtCharts/QChartGlobal>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QCheckBox;
class QGridLayout;
QT_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

#define RealList QList<qreal>
class DataSerieDialog;

class MainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MainWidget(QWidget *parent = 0);

signals:

private:
    void initBackroundCombo(QGridLayout *grid);
    void initScaleControls(QGridLayout *grid);
    void initThemeCombo(QGridLayout *grid);
    void initCheckboxes(QGridLayout *grid);

private slots:
    void addSeries();
    void addSeries(QString series, int columnCount, int rowCount, QString dataCharacteristics, bool labelsEnabled);
    void backgroundChanged(int itemIndex);
    void autoScaleChanged(int value);
    void xMinChanged(int value);
    void xMaxChanged(int value);
    void yMinChanged(int value);
    void yMaxChanged(int value);
    void antiAliasToggled(bool enabled);
    void openGLToggled(bool enabled);
    void changeChartTheme(int themeIndex);
    QList<RealList> generateTestData(int columnCount, int rowCount, QString dataCharacteristics);
    QStringList generateLabels(int count);

private:
    DataSerieDialog *m_addSerieDialog;
    QChart *m_chart;
    QChartView *m_chartView;
    QCheckBox *m_autoScaleCheck;
    QSpinBox *m_xMinSpin;
    QSpinBox *m_xMaxSpin;
    QSpinBox *m_yMinSpin;
    QSpinBox *m_yMaxSpin;
    QString m_defaultSeriesName;
    QGridLayout *m_scatterLayout;
};

#endif // MAINWIDGET_H
