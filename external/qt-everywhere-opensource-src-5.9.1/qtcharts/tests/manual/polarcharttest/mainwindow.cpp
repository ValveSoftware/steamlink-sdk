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

#include "mainwindow.h"
#include "chartview.h"
#include <QtCharts/QScatterSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QPolarChart>
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include <QtCore/QDateTime>

QT_CHARTS_USE_NAMESPACE
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_angularTickCount(9),
    m_radialTickCount(11),
    m_labelsAngle(0),
    m_angularMin(0.0),
    m_angularMax(40000.0),
    m_radialMin(0.0),
    m_radialMax(30000.0),
    m_angularShadesVisible(false),
    m_radialShadesVisible(false),
    m_labelsVisible(true),
    m_titleVisible(true),
    m_gridVisible(true),
    m_arrowVisible(true),
    m_minorGridVisible(true),
    m_minorArrowVisible(true),
    m_angularShadesBrush(new QBrush(Qt::NoBrush)),
    m_radialShadesBrush(new QBrush(Qt::NoBrush)),
    m_labelBrush(new QBrush(Qt::black)),
    m_titleBrush(new QBrush(Qt::black)),
    m_backgroundBrush(new QBrush(Qt::white)),
    m_plotAreaBackgroundBrush(new QBrush(Qt::NoBrush)),
    m_angularShadesPen(new QPen(Qt::NoPen)),
    m_radialShadesPen(new QPen(Qt::NoPen)),
    m_gridPen(new QPen(QRgb(0x010101))),  // Note: Pure black is default color, so it gets overridden by
    m_arrowPen(new QPen(QRgb(0x010101))), // default theme if set to that initially. This is an example of workaround.
    m_minorGridPen(new QPen(QBrush(QRgb(0x010101)), 1, Qt::DashLine)),
    m_backgroundPen(new QPen(Qt::NoPen)),
    m_plotAreaBackgroundPen(new QPen(Qt::NoPen)),
    m_labelFormat(QString("%.2f")),
    m_animationOptions(QChart::NoAnimation),
    m_angularTitle(QString("Angular Title")),
    m_radialTitle(QString("Radial Title")),
    m_base(2.0),
    m_dateFormat(QString("mm-ss-zzz")),
    m_chart(0),
    m_angularAxis(0),
    m_radialAxis(0),
    m_angularAxisMode(AxisModeNone),
    m_radialAxisMode(AxisModeNone),
    m_moreCategories(false),
    m_series1(0),
    m_series2(0),
    m_series3(0),
    m_series4(0),
    m_series5(0),
    m_series6(0),
    m_series7(0)
{
    ui->setupUi(this);

    ui->angularTicksSpin->setValue(m_angularTickCount);
    ui->radialTicksSpin->setValue(m_radialTickCount);
    ui->anglesSpin->setValue(m_labelsAngle);
    ui->radialMinSpin->setValue(m_radialMin);
    ui->radialMaxSpin->setValue(m_radialMax);
    ui->angularMinSpin->setValue(m_angularMin);
    ui->angularMaxSpin->setValue(m_angularMax);
    ui->angularShadesComboBox->setCurrentIndex(0);
    ui->radialShadesComboBox->setCurrentIndex(0);
    ui->labelFormatEdit->setText(m_labelFormat);
    ui->dateFormatEdit->setText(m_dateFormat);
    ui->moreCategoriesCheckBox->setChecked(m_moreCategories);

    ui->series1checkBox->setChecked(true);
    ui->series2checkBox->setChecked(true);
    ui->series3checkBox->setChecked(true);
    ui->series4checkBox->setChecked(true);
    ui->series5checkBox->setChecked(true);
    ui->series6checkBox->setChecked(true);
    ui->series7checkBox->setChecked(true);

    m_currentLabelFont.setFamily(ui->labelFontComboBox->currentFont().family());
    m_currentLabelFont.setPixelSize(15);
    m_currentTitleFont.setFamily(ui->titleFontComboBox->currentFont().family());
    m_currentTitleFont.setPixelSize(30);

    ui->labelFontSizeSpin->setValue(m_currentLabelFont.pixelSize());
    ui->titleFontSizeSpin->setValue(m_currentTitleFont.pixelSize());

    ui->logBaseSpin->setValue(m_base);

    initXYValueChart();
    setAngularAxis(AxisModeValue);
    setRadialAxis(AxisModeValue);

    ui->angularAxisComboBox->setCurrentIndex(int(m_angularAxisMode));
    ui->radialAxisComboBox->setCurrentIndex(int(m_radialAxisMode));

    connect(ui->angularTicksSpin, SIGNAL(valueChanged(int)),
            this, SLOT(angularTicksChanged(int)));
    connect(ui->radialTicksSpin, SIGNAL(valueChanged(int)),
            this, SLOT(radialTicksChanged(int)));
    connect(ui->angularMinorTicksSpin, SIGNAL(valueChanged(int)),
            this, SLOT(angularMinorTicksChanged(int)));
    connect(ui->radialMinorTicksSpin, SIGNAL(valueChanged(int)),
            this, SLOT(radialMinorTicksChanged(int)));
    connect(ui->anglesSpin, SIGNAL(valueChanged(int)),
            this, SLOT(anglesChanged(int)));
    connect(ui->radialMinSpin, SIGNAL(valueChanged(double)),
            this, SLOT(radialMinChanged(double)));
    connect(ui->radialMaxSpin, SIGNAL(valueChanged(double)),
            this, SLOT(radialMaxChanged(double)));
    connect(ui->angularMinSpin, SIGNAL(valueChanged(double)),
            this, SLOT(angularMinChanged(double)));
    connect(ui->angularMaxSpin, SIGNAL(valueChanged(double)),
            this, SLOT(angularMaxChanged(double)));
    connect(ui->angularShadesComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(angularShadesIndexChanged(int)));
    connect(ui->radialShadesComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(radialShadesIndexChanged(int)));
    connect(ui->animationsComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(animationIndexChanged(int)));
    connect(ui->labelFormatEdit, SIGNAL(textEdited(QString)),
            this, SLOT(labelFormatEdited(QString)));
    connect(ui->labelFontComboBox, SIGNAL(currentFontChanged(QFont)),
            this, SLOT(labelFontChanged(QFont)));
    connect(ui->labelFontSizeSpin, SIGNAL(valueChanged(int)),
            this, SLOT(labelFontSizeChanged(int)));
    connect(ui->labelComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(labelsIndexChanged(int)));
    connect(ui->titleFontComboBox, SIGNAL(currentFontChanged(QFont)),
            this, SLOT(titleFontChanged(QFont)));
    connect(ui->titleFontSizeSpin, SIGNAL(valueChanged(int)),
            this, SLOT(titleFontSizeChanged(int)));
    connect(ui->titleComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(titleIndexChanged(int)));
    connect(ui->gridComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(gridIndexChanged(int)));
    connect(ui->minorGridComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(minorGridIndexChanged(int)));
    connect(ui->gridLineColorComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(gridLineColorIndexChanged(int)));
    connect(ui->arrowComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(arrowIndexChanged(int)));
    connect(ui->logBaseSpin, SIGNAL(valueChanged(double)),
            this, SLOT(logBaseChanged(double)));
    connect(ui->angularAxisComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(angularAxisIndexChanged(int)));
    connect(ui->radialAxisComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(radialAxisIndexChanged(int)));
    connect(ui->niceNumbersCheckBox, SIGNAL(clicked()),
            this, SLOT(niceNumbersChecked()));
    connect(ui->dateFormatEdit, SIGNAL(textEdited(QString)),
            this, SLOT(dateFormatEdited(QString)));
    connect(ui->moreCategoriesCheckBox, SIGNAL(clicked()),
            this, SLOT(moreCategoriesChecked()));
    connect(ui->categoryLabelLocationCheckBox, SIGNAL(clicked()),
            this, SLOT(categoryLabelLocationChecked()));
    connect(ui->series1checkBox, SIGNAL(clicked()),
            this, SLOT(series1CheckBoxChecked()));
    connect(ui->series2checkBox, SIGNAL(clicked()),
            this, SLOT(series2CheckBoxChecked()));
    connect(ui->series3checkBox, SIGNAL(clicked()),
            this, SLOT(series3CheckBoxChecked()));
    connect(ui->series4checkBox, SIGNAL(clicked()),
            this, SLOT(series4CheckBoxChecked()));
    connect(ui->series5checkBox, SIGNAL(clicked()),
            this, SLOT(series5CheckBoxChecked()));
    connect(ui->series6checkBox, SIGNAL(clicked()),
            this, SLOT(series6CheckBoxChecked()));
    connect(ui->series7checkBox, SIGNAL(clicked()),
            this, SLOT(series7CheckBoxChecked()));
    connect(ui->themeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(themeIndexChanged(int)));
    connect(ui->backgroundComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(backgroundIndexChanged(int)));
    connect(ui->plotAreaComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(plotAreaIndexChanged(int)));

    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_angularShadesBrush;
    delete m_radialShadesBrush;
    delete m_angularShadesPen;
    delete m_radialShadesPen;
}

void MainWindow::initXYValueChart()
{
    qreal seriesAngularMin = 1;
    qreal seriesAngularMax = 46000;
    qreal seriesRadialMin = 1;
    qreal seriesRadialMax = 23500;
    qreal radialDimension = seriesRadialMax - seriesRadialMin;
    qreal angularDimension = seriesAngularMax - seriesAngularMin;

    // Scatter series, points outside min-max ranges should not be drawn
    m_series1 = new QScatterSeries();
    m_series1->setName("scatter");
    qreal scatterCount = 10;
    qreal scatterAngularStep = angularDimension / scatterCount;
    qreal scatterRadialStep = radialDimension / scatterCount;
    for (qreal i = 0.0; i < scatterCount; i++) {
        m_series1->append((i * scatterAngularStep) + seriesAngularMin, (i * scatterRadialStep) + seriesRadialMin);
        //qDebug() << m_series1->points().last();
    }
    m_series1->setMarkerSize(10);
    *m_series1 << QPointF(50, 50) << QPointF(150, 150) << QPointF(250, 250) << QPointF(350, 350) << QPointF(450, 450);
    *m_series1 << QPointF(1050, 0.50) << QPointF(1150, 0.25) << QPointF(1250, 0.12) << QPointF(1350, 0.075) << QPointF(1450, 0.036);
    *m_series1 << QPointF(0.50, 2000) << QPointF(0.25, 3500) << QPointF(0.12, 5000) << QPointF(0.075, 6500) << QPointF(0.036, 8000);

    // Line series, points outside min-max ranges should not be drawn,
    // but lines should be properly interpolated at chart edges
    m_series2 = new QLineSeries();
    m_series2->setName("line 1");
    qreal lineCount = 100;
    qreal lineAngularStep = angularDimension / lineCount;
    qreal lineRadialStep = radialDimension / lineCount;
    for (qreal i = 0.0; i < lineCount; i++) {
        m_series2->append((i * lineAngularStep) + seriesAngularMin, (i * lineRadialStep) + seriesRadialMin);
        //qDebug() << m_series2->points().last();
    }
    QPen series2Pen = QPen(Qt::blue, 10);
    //series2Pen.setStyle(Qt::DashDotDotLine);
    m_series2->setPen(series2Pen);

    m_series3 = new QLineSeries();
    m_series3->setName("Area upper");
    lineCount = 87;
    lineAngularStep = angularDimension / lineCount;
    lineRadialStep = radialDimension / lineCount;
    for (qreal i = 1.0; i <= lineCount; i++) {
        m_series3->append((i * lineAngularStep) + seriesAngularMin, (i * lineRadialStep) + seriesRadialMin + 200.0);
        //qDebug() << m_series3->points().last();
    }

    m_series4 = new QLineSeries();
    m_series4->setName("Area lower");
    lineCount = 89;
    lineAngularStep = angularDimension / lineCount;
    lineRadialStep = radialDimension / lineCount;
    for (qreal i = 1.0; i <= lineCount; i++) {
        m_series4->append((i * lineAngularStep) + seriesAngularMin + 100.0, (i * lineRadialStep) + seriesRadialMin + i * 300.0);
        //qDebug() << m_series4->points().last();
    }

    m_series5 = new QAreaSeries();
    m_series5->setName("area");
    m_series5->setUpperSeries(m_series3);
    m_series5->setLowerSeries(m_series4);
    m_series5->setOpacity(0.5);

    m_series6 = new QSplineSeries();
    m_series6->setName("spline");
    qreal ad = angularDimension / 20;
    qreal rd = radialDimension / 10;
    m_series6->append(seriesAngularMin, seriesRadialMin + rd * 2);
    m_series6->append(seriesAngularMin + ad, seriesRadialMin + rd * 5);
    m_series6->append(seriesAngularMin + ad * 2, seriesRadialMin + rd * 4);
    m_series6->append(seriesAngularMin + ad * 3, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 4, seriesRadialMin + rd * 11);
    m_series6->append(seriesAngularMin + ad * 5, seriesRadialMin + rd * 12);
    m_series6->append(seriesAngularMin + ad * 6, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 7, seriesRadialMin + rd * 11);
    m_series6->append(seriesAngularMin + ad * 8, seriesRadialMin + rd * 12);
    m_series6->append(seriesAngularMin + ad * 9, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 10, seriesRadialMin + rd * 4);
    m_series6->append(seriesAngularMin + ad * 10, seriesRadialMin + rd * 8);
    m_series6->append(seriesAngularMin + ad * 11, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 12, seriesRadialMin + rd * 11);
    m_series6->append(seriesAngularMin + ad * 13, seriesRadialMin + rd * 12);
    m_series6->append(seriesAngularMin + ad * 14, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 15, seriesRadialMin + rd * 3);
    m_series6->append(seriesAngularMin + ad * 16, seriesRadialMin + rd * 2);
    m_series6->append(seriesAngularMin + ad * 17, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 18, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 19, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 20, seriesRadialMin + rd * 6);
    m_series6->append(seriesAngularMin + ad * 19, seriesRadialMin + rd * 2);
    m_series6->append(seriesAngularMin + ad * 18, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 17, seriesRadialMin + rd * 7);
    m_series6->append(seriesAngularMin + ad * 16, seriesRadialMin + rd * 3);
    m_series6->append(seriesAngularMin + ad * 15, seriesRadialMin + rd * 1);
    m_series6->append(seriesAngularMin + ad * 14, seriesRadialMin + rd * 7);
    m_series6->append(seriesAngularMin + ad * 13, seriesRadialMin + rd * 5);
    m_series6->append(seriesAngularMin + ad * 12, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 11, seriesRadialMin + rd * 1);
    m_series6->append(seriesAngularMin + ad * 10, seriesRadialMin + rd * 4);
    m_series6->append(seriesAngularMin + ad * 9, seriesRadialMin + rd * 1);
    m_series6->append(seriesAngularMin + ad * 8, seriesRadialMin + rd * 2);
    m_series6->append(seriesAngularMin + ad * 7, seriesRadialMin + rd * 4);
    m_series6->append(seriesAngularMin + ad * 6, seriesRadialMin + rd * 8);
    m_series6->append(seriesAngularMin + ad * 5, seriesRadialMin + rd * 12);
    m_series6->append(seriesAngularMin + ad * 4, seriesRadialMin + rd * 9);
    m_series6->append(seriesAngularMin + ad * 3, seriesRadialMin + rd * 8);
    m_series6->append(seriesAngularMin + ad * 2, seriesRadialMin + rd * 7);
    m_series6->append(seriesAngularMin + ad, seriesRadialMin + rd * 4);
    m_series6->append(seriesAngularMin, seriesRadialMin + rd * 10);

    m_series6->setPointsVisible(true);
    QPen series6Pen = QPen(Qt::red, 10);
    //series6Pen.setStyle(Qt::DashDotDotLine);
    m_series6->setPen(series6Pen);

    // m_series7 shows points at category intersections
    m_series7 = new QScatterSeries();
    m_series7->setName("Category check");
    m_series7->setMarkerSize(7);
    m_series7->setBrush(QColor(Qt::red));
    m_series7->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    *m_series7 << QPointF(1000, 1000)
               << QPointF(1000, 2000)
               << QPointF(1000, 4000)
               << QPointF(1000, 9000)
               << QPointF(1000, 14000)
               << QPointF(1000, 16500)
               << QPointF(1000, 19000)

               << QPointF(4000, 1000)
               << QPointF(4000, 2000)
               << QPointF(4000, 4000)
               << QPointF(4000, 9000)
               << QPointF(4000, 14000)
               << QPointF(4000, 16500)
               << QPointF(4000, 19000)

               << QPointF(7000, 1000)
               << QPointF(7000, 2000)
               << QPointF(7000, 4000)
               << QPointF(7000, 9000)
               << QPointF(7000, 14000)
               << QPointF(7000, 16500)
               << QPointF(7000, 19000)

               << QPointF(12000, 1000)
               << QPointF(12000, 2000)
               << QPointF(12000, 4000)
               << QPointF(12000, 9000)
               << QPointF(12000, 14000)
               << QPointF(12000, 16500)
               << QPointF(12000, 19000)

               << QPointF(17000, 1000)
               << QPointF(17000, 2000)
               << QPointF(17000, 4000)
               << QPointF(17000, 9000)
               << QPointF(17000, 14000)
               << QPointF(17000, 16500)
               << QPointF(17000, 19000)

               << QPointF(22000, 1000)
               << QPointF(22000, 2000)
               << QPointF(22000, 4000)
               << QPointF(22000, 9000)
               << QPointF(22000, 14000)
               << QPointF(22000, 16500)
               << QPointF(22000, 19000)

               << QPointF(28000, 1000)
               << QPointF(28000, 2000)
               << QPointF(28000, 4000)
               << QPointF(28000, 9000)
               << QPointF(28000, 14000)
               << QPointF(28000, 16500)
               << QPointF(28000, 19000);

    m_chart = new QPolarChart();

    m_chart->addSeries(m_series1);
    m_chart->addSeries(m_series2);
    m_chart->addSeries(m_series3);
    m_chart->addSeries(m_series4);
    m_chart->addSeries(m_series5);
    m_chart->addSeries(m_series6);
    m_chart->addSeries(m_series7);

    connect(m_series1, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series2, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series3, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series4, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series5, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series6, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series7, SIGNAL(clicked(QPointF)), this, SLOT(seriesClicked(QPointF)));
    connect(m_series1, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series2, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series3, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series4, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series5, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series6, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));
    connect(m_series7, SIGNAL(hovered(QPointF, bool)), this, SLOT(seriesHovered(QPointF, bool)));

    m_chart->setTitle("Use arrow keys to scroll and +/- to zoom");
    m_chart->setAnimationOptions(m_animationOptions);
    //m_chart->legend()->setVisible(false);
    m_chart->setAcceptHoverEvents(true);
    m_chart->setBackgroundBrush(*m_backgroundBrush);
    m_chart->setBackgroundPen(*m_backgroundPen);
    m_chart->setPlotAreaBackgroundBrush(*m_plotAreaBackgroundBrush);
    m_chart->setPlotAreaBackgroundPen(*m_plotAreaBackgroundPen);
}

void MainWindow::setAngularAxis(MainWindow::AxisMode mode)
{
    if (m_angularAxis) {
        m_chart->removeAxis(m_angularAxis);
        delete m_angularAxis;
        m_angularAxis = 0;
    }

    m_angularAxisMode = mode;

    switch (m_angularAxisMode) {
    case AxisModeNone:
        return;
    case AxisModeValue:
        m_angularAxis = new QValueAxis();
        static_cast<QValueAxis *>(m_angularAxis)->setTickCount(m_angularTickCount);
        static_cast<QValueAxis *>(m_angularAxis)->setLabelFormat(m_labelFormat);
        break;
    case AxisModeLogValue:
        m_angularAxis = new QLogValueAxis();
        static_cast<QLogValueAxis *>(m_angularAxis)->setBase(m_base);
        static_cast<QLogValueAxis *>(m_angularAxis)->setLabelFormat(m_labelFormat);
        break;
    case AxisModeDateTime:
        m_angularAxis = new QDateTimeAxis();
        static_cast<QDateTimeAxis *>(m_angularAxis)->setTickCount(m_angularTickCount);
        static_cast<QDateTimeAxis *>(m_angularAxis)->setFormat(m_dateFormat);
        break;
    case AxisModeCategory:
        m_angularAxis = new QCategoryAxis();
        applyCategories();
        break;
    default:
        qWarning() << "Unsupported AxisMode";
        break;
    }

    m_angularAxis->setLabelsAngle(m_labelsAngle);
    m_angularAxis->setLabelsFont(m_currentLabelFont);
    m_angularAxis->setLabelsBrush(*m_labelBrush);
    m_angularAxis->setLabelsVisible(m_labelsVisible);
    m_angularAxis->setShadesBrush(*m_angularShadesBrush);
    m_angularAxis->setShadesPen(*m_angularShadesPen);
    m_angularAxis->setShadesVisible(m_angularShadesVisible);
    m_angularAxis->setTitleFont(m_currentTitleFont);
    m_angularAxis->setTitleBrush(*m_titleBrush);
    m_angularAxis->setTitleVisible(m_titleVisible);
    m_angularAxis->setTitleText(m_angularTitle);
    m_angularAxis->setGridLinePen(*m_gridPen);
    m_angularAxis->setGridLineVisible(m_gridVisible);
    m_angularAxis->setLinePen(*m_arrowPen);
    m_angularAxis->setLineVisible(m_arrowVisible);
    m_angularAxis->setMinorGridLinePen(*m_minorGridPen);
    m_angularAxis->setMinorGridLineVisible(m_minorGridVisible);

    m_chart->addAxis(m_angularAxis, QPolarChart::PolarOrientationAngular);

    m_series1->attachAxis(m_angularAxis);
    m_series2->attachAxis(m_angularAxis);
    m_series3->attachAxis(m_angularAxis);
    m_series4->attachAxis(m_angularAxis);
    m_series5->attachAxis(m_angularAxis);
    m_series6->attachAxis(m_angularAxis);
    m_series7->attachAxis(m_angularAxis);

    applyRanges();

    //connect(m_angularAxis, SIGNAL(rangeChanged(qreal, qreal)), this, SLOT(angularRangeChanged(qreal, qreal)));
}

void MainWindow::setRadialAxis(MainWindow::AxisMode mode)
{
    if (m_radialAxis) {
        m_chart->removeAxis(m_radialAxis);
        delete m_radialAxis;
        m_radialAxis = 0;
    }

    m_radialAxisMode = mode;

    switch (m_radialAxisMode) {
    case AxisModeNone:
        return;
    case AxisModeValue:
        m_radialAxis = new QValueAxis();
        static_cast<QValueAxis *>(m_radialAxis)->setTickCount(m_radialTickCount);
        static_cast<QValueAxis *>(m_radialAxis)->setLabelFormat(m_labelFormat);
        break;
    case AxisModeLogValue:
        m_radialAxis = new QLogValueAxis();
        static_cast<QLogValueAxis *>(m_radialAxis)->setBase(m_base);
        static_cast<QLogValueAxis *>(m_radialAxis)->setLabelFormat(m_labelFormat);
        break;
    case AxisModeDateTime:
        m_radialAxis = new QDateTimeAxis();
        static_cast<QDateTimeAxis *>(m_radialAxis)->setTickCount(m_radialTickCount);
        static_cast<QDateTimeAxis *>(m_radialAxis)->setFormat(m_dateFormat);
        break;
    case AxisModeCategory:
        m_radialAxis = new QCategoryAxis();
        applyCategories();
        break;
    default:
        qWarning() << "Unsupported AxisMode";
        break;
    }

    m_radialAxis->setLabelsAngle(m_labelsAngle);
    m_radialAxis->setLabelsFont(m_currentLabelFont);
    m_radialAxis->setLabelsBrush(*m_labelBrush);
    m_radialAxis->setLabelsVisible(m_labelsVisible);
    m_radialAxis->setShadesBrush(*m_radialShadesBrush);
    m_radialAxis->setShadesPen(*m_radialShadesPen);
    m_radialAxis->setShadesVisible(m_radialShadesVisible);
    m_radialAxis->setTitleFont(m_currentTitleFont);
    m_radialAxis->setTitleBrush(*m_titleBrush);
    m_radialAxis->setTitleVisible(m_titleVisible);
    m_radialAxis->setTitleText(m_radialTitle);
    m_radialAxis->setGridLinePen(*m_gridPen);
    m_radialAxis->setGridLineVisible(m_gridVisible);
    m_radialAxis->setLinePen(*m_arrowPen);
    m_radialAxis->setLineVisible(m_arrowVisible);
    m_radialAxis->setMinorGridLinePen(*m_minorGridPen);
    m_radialAxis->setMinorGridLineVisible(m_minorGridVisible);

    m_chart->addAxis(m_radialAxis, QPolarChart::PolarOrientationRadial);

    m_series1->attachAxis(m_radialAxis);
    m_series2->attachAxis(m_radialAxis);
    m_series3->attachAxis(m_radialAxis);
    m_series4->attachAxis(m_radialAxis);
    m_series5->attachAxis(m_radialAxis);
    m_series6->attachAxis(m_radialAxis);
    m_series7->attachAxis(m_radialAxis);

    applyRanges();

    series1CheckBoxChecked();
    series2CheckBoxChecked();
    series3CheckBoxChecked();
    series4CheckBoxChecked();
    series5CheckBoxChecked();
    series6CheckBoxChecked();
    series7CheckBoxChecked();

    //connect(m_radialAxis, SIGNAL(rangeChanged(qreal, qreal)), this, SLOT(radialRangeChanged(qreal, qreal)));
}

void MainWindow::applyRanges()
{
    if (ui->niceNumbersCheckBox->isChecked()) {
        if (m_angularAxisMode == AxisModeValue) {
            static_cast<QValueAxis *>(m_angularAxis)->applyNiceNumbers();
            m_angularMin = static_cast<QValueAxis *>(m_angularAxis)->min();
            m_angularMax = static_cast<QValueAxis *>(m_angularAxis)->max();
            m_angularTickCount = static_cast<QValueAxis *>(m_angularAxis)->tickCount();
        }
        if (m_radialAxisMode == AxisModeValue) {
            static_cast<QValueAxis *>(m_radialAxis)->applyNiceNumbers();
            m_radialMin = static_cast<QValueAxis *>(m_radialAxis)->min();
            m_radialMax = static_cast<QValueAxis *>(m_radialAxis)->max();
            m_radialTickCount = static_cast<QValueAxis *>(m_radialAxis)->tickCount();
        }
    }

    if (m_angularAxis)
        m_angularAxis->setRange(m_angularMin, m_angularMax);
    if (m_radialAxis)
        m_radialAxis->setRange(m_radialMin, m_radialMax);
}

void MainWindow::angularTicksChanged(int value)
{
    m_angularTickCount = value;
    if (m_angularAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_angularAxis)->setTickCount(m_angularTickCount);
    else if (m_angularAxisMode == AxisModeDateTime)
        static_cast<QDateTimeAxis *>(m_angularAxis)->setTickCount(m_angularTickCount);
}

void MainWindow::radialTicksChanged(int value)
{
    m_radialTickCount = value;
    if (m_radialAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_radialAxis)->setTickCount(m_radialTickCount);
    else if (m_radialAxisMode == AxisModeDateTime)
        static_cast<QDateTimeAxis *>(m_radialAxis)->setTickCount(m_radialTickCount);
}

void MainWindow::angularMinorTicksChanged(int value)
{
    // Minor tick valid only for QValueAxis
    m_angularMinorTickCount = value;
    if (m_angularAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_angularAxis)->setMinorTickCount(m_angularMinorTickCount);
}

void MainWindow::radialMinorTicksChanged(int value)
{
    // Minor tick valid only for QValueAxis
    m_radialMinorTickCount = value;
    if (m_radialAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_radialAxis)->setMinorTickCount(m_radialMinorTickCount);
}

void MainWindow::anglesChanged(int value)
{
    m_labelsAngle = value;
    m_radialAxis->setLabelsAngle(m_labelsAngle);
    m_angularAxis->setLabelsAngle(m_labelsAngle);
}

void MainWindow::angularMinChanged(double value)
{
    m_angularMin = value;
    if (m_angularAxisMode != AxisModeDateTime) {
        m_angularAxis->setMin(m_angularMin);
    } else {
        QDateTime dateTime;
        dateTime.setMSecsSinceEpoch(qint64(m_angularMin));
        m_angularAxis->setMin(dateTime);
    }
}

void MainWindow::angularMaxChanged(double value)
{
    m_angularMax = value;
    if (m_angularAxisMode != AxisModeDateTime) {
        m_angularAxis->setMax(m_angularMax);
    } else {
        QDateTime dateTime;
        dateTime.setMSecsSinceEpoch(qint64(m_angularMax));
        m_angularAxis->setMax(dateTime);
    }
}

void MainWindow::radialMinChanged(double value)
{
    m_radialMin = value;
    if (m_radialAxisMode != AxisModeDateTime) {
        m_radialAxis->setMin(m_radialMin);
    } else {
        QDateTime dateTime;
        dateTime.setMSecsSinceEpoch(qint64(m_radialMin));
        m_radialAxis->setMin(dateTime);
    }
}

void MainWindow::radialMaxChanged(double value)
{
    m_radialMax = value;
    if (m_radialAxisMode != AxisModeDateTime) {
        m_radialAxis->setMax(m_radialMax);
    } else {
        QDateTime dateTime;
        dateTime.setMSecsSinceEpoch(qint64(m_radialMax));
        m_radialAxis->setMax(dateTime);
    }
}

void MainWindow::angularShadesIndexChanged(int index)
{
    delete m_angularShadesBrush;
    delete m_angularShadesPen;

    switch (index) {
    case 0:
        m_angularShadesBrush = new QBrush(Qt::NoBrush);
        m_angularShadesPen = new QPen(Qt::NoPen);
        m_angularShadesVisible = false;
        break;
    case 1:
        m_angularShadesBrush = new QBrush(Qt::lightGray);
        m_angularShadesPen = new QPen(Qt::NoPen);
        m_angularShadesVisible = true;
        break;
    case 2:
        m_angularShadesBrush = new QBrush(Qt::yellow);
        m_angularShadesPen = new QPen(Qt::DotLine);
        m_angularShadesPen->setWidth(2);
        m_angularShadesVisible = true;
        break;
    default:
        break;
    }

    m_angularAxis->setShadesBrush(*m_angularShadesBrush);
    m_angularAxis->setShadesPen(*m_angularShadesPen);
    m_angularAxis->setShadesVisible(m_angularShadesVisible);
}

void MainWindow::radialShadesIndexChanged(int index)
{
    delete m_radialShadesBrush;
    delete m_radialShadesPen;

    switch (index) {
    case 0:
        m_radialShadesBrush = new QBrush(Qt::NoBrush);
        m_radialShadesPen = new QPen(Qt::NoPen);
        m_radialShadesVisible = false;
        break;
    case 1:
        m_radialShadesBrush = new QBrush(Qt::green);
        m_radialShadesPen = new QPen(Qt::NoPen);
        m_radialShadesVisible = true;
        break;
    case 2:
        m_radialShadesBrush = new QBrush(Qt::blue);
        m_radialShadesPen = new QPen(Qt::DotLine);
        m_radialShadesPen->setWidth(2);
        m_radialShadesVisible = true;
        break;
    default:
        break;
    }

    m_radialAxis->setShadesBrush(*m_radialShadesBrush);
    m_radialAxis->setShadesPen(*m_radialShadesPen);
    m_radialAxis->setShadesVisible(m_radialShadesVisible);
}

void MainWindow::labelFormatEdited(const QString &text)
{
    m_labelFormat = text;
    if (m_angularAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_angularAxis)->setLabelFormat(m_labelFormat);
    else if (m_angularAxisMode == AxisModeLogValue)
        static_cast<QLogValueAxis *>(m_angularAxis)->setLabelFormat(m_labelFormat);

    if (m_radialAxisMode == AxisModeValue)
        static_cast<QValueAxis *>(m_radialAxis)->setLabelFormat(m_labelFormat);
    else if (m_radialAxisMode == AxisModeLogValue)
        static_cast<QLogValueAxis *>(m_radialAxis)->setLabelFormat(m_labelFormat);
}

void MainWindow::labelFontChanged(const QFont &font)
{
    m_currentLabelFont = font;
    m_currentLabelFont.setPixelSize(ui->labelFontSizeSpin->value());
    m_angularAxis->setLabelsFont(m_currentLabelFont);
    m_radialAxis->setLabelsFont(m_currentLabelFont);
}

void MainWindow::labelFontSizeChanged(int value)
{
    m_currentLabelFont = ui->labelFontComboBox->currentFont();
    m_currentLabelFont.setPixelSize(value);
    m_angularAxis->setLabelsFont(m_currentLabelFont);
    m_radialAxis->setLabelsFont(m_currentLabelFont);
}

void MainWindow::animationIndexChanged(int index)
{
    switch (index) {
    case 0:
        m_animationOptions = QChart::NoAnimation;
        break;
    case 1:
        m_animationOptions = QChart::SeriesAnimations;
        break;
    case 2:
        m_animationOptions = QChart::GridAxisAnimations;
        break;
    case 3:
        m_animationOptions = QChart::AllAnimations;
        break;
    default:
        break;
    }

    m_chart->setAnimationOptions(m_animationOptions);
}

void MainWindow::labelsIndexChanged(int index)
{
    delete m_labelBrush;

    switch (index) {
    case 0:
        m_labelBrush = new QBrush(Qt::NoBrush);
        m_labelsVisible = false;
        break;
    case 1:
        m_labelBrush = new QBrush(Qt::black);
        m_labelsVisible = true;
        break;
    case 2:
        m_labelBrush = new QBrush(Qt::white);
        m_labelsVisible = true;
        break;
    default:
        break;
    }

    m_radialAxis->setLabelsBrush(*m_labelBrush);
    m_radialAxis->setLabelsVisible(m_labelsVisible);
    m_angularAxis->setLabelsBrush(*m_labelBrush);
    m_angularAxis->setLabelsVisible(m_labelsVisible);
}

void MainWindow::titleIndexChanged(int index)
{
    delete m_titleBrush;

    switch (index) {
    case 0:
        m_titleBrush = new QBrush(Qt::NoBrush);
        m_titleVisible = false;
        m_angularTitle = QString();
        m_radialTitle = QString();
        break;
    case 1:
        m_titleBrush = new QBrush(Qt::NoBrush);
        m_titleVisible = true;
        m_angularTitle = QString();
        m_radialTitle = QString();
        break;
    case 2:
        m_titleBrush = new QBrush(Qt::NoBrush);
        m_titleVisible = false;
        m_angularTitle = QString("Invisible Ang. Title!");
        m_radialTitle = QString("Invisible Rad. Title!");
        break;
    case 3:
        m_titleBrush = new QBrush(Qt::black);
        m_titleVisible = true;
        m_angularTitle = QString("Angular Title");
        m_radialTitle = QString("Radial Title");
        break;
    case 4:
        m_titleBrush = new QBrush(Qt::white);
        m_titleVisible = true;
        m_angularTitle = QString("Angular Blue Title");
        m_radialTitle = QString("Radial Blue Title");
        break;
    default:
        break;
    }

    m_radialAxis->setTitleBrush(*m_titleBrush);
    m_radialAxis->setTitleVisible(m_titleVisible);
    m_radialAxis->setTitleText(m_radialTitle);
    m_angularAxis->setTitleBrush(*m_titleBrush);
    m_angularAxis->setTitleVisible(m_titleVisible);
    m_angularAxis->setTitleText(m_angularTitle);
}

void MainWindow::titleFontChanged(const QFont &font)
{
    m_currentTitleFont = font;
    m_currentTitleFont.setPixelSize(ui->titleFontSizeSpin->value());
    m_angularAxis->setTitleFont(m_currentTitleFont);
    m_radialAxis->setTitleFont(m_currentTitleFont);
}

void MainWindow::titleFontSizeChanged(int value)
{
    m_currentTitleFont = ui->titleFontComboBox->currentFont();
    m_currentTitleFont.setPixelSize(value);
    m_angularAxis->setTitleFont(m_currentTitleFont);
    m_radialAxis->setTitleFont(m_currentTitleFont);
}

void MainWindow::gridIndexChanged(int index)
{
    delete m_gridPen;

    switch (index) {
    case 0:
        m_gridPen = new QPen(Qt::NoPen);
        m_gridVisible = false;
        break;
    case 1:
        m_gridPen = new QPen(Qt::black);
        m_gridVisible = true;
        break;
    case 2:
        m_gridPen = new QPen(Qt::red);
        m_gridPen->setStyle(Qt::DashDotLine);
        m_gridPen->setWidth(3);
        m_gridVisible = true;
        break;
    default:
        break;
    }

    m_angularAxis->setGridLinePen(*m_gridPen);
    m_angularAxis->setGridLineVisible(m_gridVisible);
    m_radialAxis->setGridLinePen(*m_gridPen);
    m_radialAxis->setGridLineVisible(m_gridVisible);
}

void MainWindow::minorGridIndexChanged(int index)
{
    delete m_minorGridPen;

    switch (index) {
    case 0:
        m_minorGridPen = new QPen(Qt::NoPen);
        m_minorGridVisible = false;
        break;
    case 1:
        m_minorGridPen = new QPen(Qt::black);
        m_minorGridPen->setStyle(Qt::DashLine);
        m_minorGridVisible = true;
        break;
    case 2:
        m_minorGridPen = new QPen(Qt::green);
        m_minorGridPen->setStyle(Qt::DotLine);
        m_minorGridPen->setWidth(1);
        m_minorGridVisible = true;
        break;
    default:
        break;
    }

    m_angularAxis->setMinorGridLinePen(*m_minorGridPen);
    m_angularAxis->setMinorGridLineVisible(m_minorGridVisible);
    m_radialAxis->setMinorGridLinePen(*m_minorGridPen);
    m_radialAxis->setMinorGridLineVisible(m_minorGridVisible);
}

void MainWindow::gridLineColorIndexChanged(int index)
{
    QColor color;
    switch (index) {
    case 0:
        color = Qt::black;
        break;
    case 1:
        color = Qt::green;
        break;
    case 2:
        color = Qt::red;
        break;
    default:
        break;
    }

    m_angularAxis->setGridLineColor(color);
    m_radialAxis->setGridLineColor(color);
    m_angularAxis->setMinorGridLineColor(color);
    m_radialAxis->setMinorGridLineColor(color);
}

void MainWindow::arrowIndexChanged(int index)
{
    delete m_arrowPen;

    switch (index) {
    case 0:
        m_arrowPen = new QPen(Qt::NoPen);
        m_arrowVisible = false;
        break;
    case 1:
        m_arrowPen = new QPen(Qt::black);
        m_arrowVisible = true;
        break;
    case 2:
        m_arrowPen = new QPen(Qt::red);
        m_arrowPen->setStyle(Qt::DashDotLine);
        m_arrowPen->setWidth(3);
        m_arrowVisible = true;
        break;
    default:
        break;
    }

    m_angularAxis->setLinePen(*m_arrowPen);
    m_angularAxis->setLineVisible(m_arrowVisible);
    m_radialAxis->setLinePen(*m_arrowPen);
    m_radialAxis->setLineVisible(m_arrowVisible);
}

void MainWindow::angularRangeChanged(qreal min, qreal max)
{
    if (!qFuzzyCompare(qreal(ui->angularMinSpin->value()), min))
        ui->angularMinSpin->setValue(min);
    if (!qFuzzyCompare(qreal(ui->angularMaxSpin->value()), max))
        ui->angularMaxSpin->setValue(max);
}

void MainWindow::radialRangeChanged(qreal min, qreal max)
{
    if (!qFuzzyCompare(qreal(ui->radialMinSpin->value()), min))
        ui->radialMinSpin->setValue(min);
    if (!qFuzzyCompare(qreal(ui->radialMaxSpin->value()), max))
        ui->radialMaxSpin->setValue(max);
}

void MainWindow::angularAxisIndexChanged(int index)
{
    switch (index) {
    case 0:
        setAngularAxis(AxisModeNone);
        break;
    case 1:
        setAngularAxis(AxisModeValue);
        angularMinorTicksChanged(ui->angularMinorTicksSpin->value());
        break;
    case 2:
        setAngularAxis(AxisModeLogValue);
        break;
    case 3:
        setAngularAxis(AxisModeDateTime);
        break;
    case 4:
        setAngularAxis(AxisModeCategory);
        break;
    default:
        qWarning("Invalid Index!");
    }
}

void MainWindow::radialAxisIndexChanged(int index)
{
    switch (index) {
    case 0:
        setRadialAxis(AxisModeNone);
        break;
    case 1:
        setRadialAxis(AxisModeValue);
        radialMinorTicksChanged(ui->radialMinorTicksSpin->value());
        break;
    case 2:
        setRadialAxis(AxisModeLogValue);
        break;
    case 3:
        setRadialAxis(AxisModeDateTime);
        break;
    case 4:
        setRadialAxis(AxisModeCategory);
        break;
    default:
        qWarning("Invalid Index!");
    }
}

void MainWindow::logBaseChanged(double value)
{
    m_base = value;
    if (m_angularAxisMode == AxisModeLogValue)
        static_cast<QLogValueAxis *>(m_angularAxis)->setBase(m_base);
    if (m_radialAxisMode == AxisModeLogValue)
        static_cast<QLogValueAxis *>(m_radialAxis)->setBase(m_base);
}

void MainWindow::niceNumbersChecked()
{
    if (ui->niceNumbersCheckBox->isChecked())
        applyRanges();
}

void MainWindow::dateFormatEdited(const QString &text)
{
    m_dateFormat = text;
    if (m_angularAxisMode == AxisModeDateTime)
        static_cast<QDateTimeAxis *>(m_angularAxis)->setFormat(m_dateFormat);
    if (m_radialAxisMode == AxisModeDateTime)
        static_cast<QDateTimeAxis *>(m_radialAxis)->setFormat(m_dateFormat);
}

void MainWindow::moreCategoriesChecked()
{
    applyCategories();
    m_moreCategories = ui->moreCategoriesCheckBox->isChecked();
}

void MainWindow::categoryLabelLocationChecked()
{
    applyCategories();
}

void MainWindow::series1CheckBoxChecked()
{
    if (ui->series1checkBox->isChecked())
        m_series1->setVisible(true);
    else
        m_series1->setVisible(false);
}

void MainWindow::series2CheckBoxChecked()
{
    if (ui->series2checkBox->isChecked())
        m_series2->setVisible(true);
    else
        m_series2->setVisible(false);
}

void MainWindow::series3CheckBoxChecked()
{
    if (ui->series3checkBox->isChecked())
        m_series3->setVisible(true);
    else
        m_series3->setVisible(false);
}

void MainWindow::series4CheckBoxChecked()
{
    if (ui->series4checkBox->isChecked())
        m_series4->setVisible(true);
    else
        m_series4->setVisible(false);
}

void MainWindow::series5CheckBoxChecked()
{
    if (ui->series5checkBox->isChecked())
        m_series5->setVisible(true);
    else
        m_series5->setVisible(false);
}

void MainWindow::series6CheckBoxChecked()
{
    if (ui->series6checkBox->isChecked())
        m_series6->setVisible(true);
    else
        m_series6->setVisible(false);
}

void MainWindow::series7CheckBoxChecked()
{
    if (ui->series7checkBox->isChecked())
        m_series7->setVisible(true);
    else
        m_series7->setVisible(false);
}

void MainWindow::themeIndexChanged(int index)
{
    m_chart->setTheme(QChart::ChartTheme(index));
}

void MainWindow::seriesHovered(QPointF point, bool state)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    if (series) {
        if (state) {
            QString str("'%3' - %1 x %2");
            ui->hoverLabel->setText(str.arg(point.x()).arg(point.y()).arg(series->name()));
        } else {
            ui->hoverLabel->setText("No hover");
        }
    } else {
        qDebug() << "seriesHovered - invalid sender!";
    }
}

void MainWindow::seriesClicked(const QPointF &point)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    if (series) {
        QString str("'%3' clicked at: %1 x %2");
        m_angularTitle = str.arg(point.x()).arg(point.y()).arg(series->name());
        m_angularAxis->setTitleText(m_angularTitle);
    } else {
        qDebug() << "seriesClicked -  invalid sender!";
    }
}

void MainWindow::backgroundIndexChanged(int index)
{
    delete m_backgroundBrush;
    delete m_backgroundPen;

    switch (index) {
    case 0:
        m_backgroundBrush = new QBrush(Qt::white);
        m_backgroundPen = new QPen(Qt::NoPen);
        break;
    case 1:
        m_backgroundBrush = new QBrush(Qt::blue);
        m_backgroundPen = new QPen(Qt::NoPen);
        break;
    case 2:
        m_backgroundBrush = new QBrush(Qt::yellow);
        m_backgroundPen = new QPen(Qt::black, 2);
        break;
    default:
        break;
    }
    m_chart->setBackgroundBrush(*m_backgroundBrush);
    m_chart->setBackgroundPen(*m_backgroundPen);
}

void MainWindow::plotAreaIndexChanged(int index)
{
    delete m_plotAreaBackgroundBrush;
    delete m_plotAreaBackgroundPen;

    switch (index) {
    case 0:
        m_plotAreaBackgroundBrush = new QBrush(Qt::green);
        m_plotAreaBackgroundPen = new QPen(Qt::green);
        m_chart->setPlotAreaBackgroundVisible(false);
        break;
    case 1:
        m_plotAreaBackgroundBrush = new QBrush(Qt::magenta);
        m_plotAreaBackgroundPen = new QPen(Qt::NoPen);
        m_chart->setPlotAreaBackgroundVisible(true);
        break;
    case 2:
        m_plotAreaBackgroundBrush = new QBrush(Qt::lightGray);
        m_plotAreaBackgroundPen = new QPen(Qt::red, 6);
        m_chart->setPlotAreaBackgroundVisible(true);
        break;
    default:
        break;
    }
    m_chart->setPlotAreaBackgroundBrush(*m_plotAreaBackgroundBrush);
    m_chart->setPlotAreaBackgroundPen(*m_plotAreaBackgroundPen);
}

void MainWindow::applyCategories()
{
    // Basic layout is three categories, extended has five
    if (m_angularAxisMode == AxisModeCategory) {
        QCategoryAxis *angCatAxis = static_cast<QCategoryAxis *>(m_angularAxis);
        if (angCatAxis->count() == 0) {
            angCatAxis->setStartValue(4000);
            angCatAxis->append("Category A", 7000);
            angCatAxis->append("Category B", 12000);
            angCatAxis->append("Category C", 17000);
        }
        if (angCatAxis->count() == 3 && ui->moreCategoriesCheckBox->isChecked()) {
            angCatAxis->setStartValue(1000);
            angCatAxis->replaceLabel("Category A", "Cat A");
            angCatAxis->replaceLabel("Category B", "Cat B");
            angCatAxis->replaceLabel("Category C", "Cat C");
            angCatAxis->append("Cat D", 22000);
            angCatAxis->append("Cat E", 28000);
        } else if (angCatAxis->count() == 5 && !ui->moreCategoriesCheckBox->isChecked()) {
            angCatAxis->setStartValue(4000);
            angCatAxis->replaceLabel("Cat A", "Category A");
            angCatAxis->replaceLabel("Cat B", "Category B");
            angCatAxis->replaceLabel("Cat C", "Category C");
            angCatAxis->remove("Cat D");
            angCatAxis->remove("Cat E");
        }
        if (ui->categoryLabelLocationCheckBox->isChecked())
            angCatAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
        else
            angCatAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionCenter);
    }

    if (m_radialAxisMode == AxisModeCategory) {
        QCategoryAxis *radCatAxis = static_cast<QCategoryAxis *>(m_radialAxis);
        if (radCatAxis->count() == 0) {
            radCatAxis->setStartValue(2000);
            radCatAxis->append("Category 1", 4000);
            radCatAxis->append("Category 2", 9000);
            radCatAxis->append("Category 3", 14000);
        }
        if (radCatAxis->count() == 3 && ui->moreCategoriesCheckBox->isChecked()) {
            radCatAxis->setStartValue(1000);
            radCatAxis->replaceLabel("Category 1", "Cat 1");
            radCatAxis->replaceLabel("Category 2", "Cat 2");
            radCatAxis->replaceLabel("Category 3", "Cat 3");
            radCatAxis->append("Cat 4", 16500);
            radCatAxis->append("Cat 5", 19000);
        } else if (radCatAxis->count() == 5 && !ui->moreCategoriesCheckBox->isChecked()) {
            radCatAxis->setStartValue(2000);
            radCatAxis->replaceLabel("Cat 1", "Category 1");
            radCatAxis->replaceLabel("Cat 2", "Category 2");
            radCatAxis->replaceLabel("Cat 3", "Category 3");
            radCatAxis->remove("Cat 4");
            radCatAxis->remove("Cat 5");
        }
        if (ui->categoryLabelLocationCheckBox->isChecked())
            radCatAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
        else
            radCatAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionCenter);
    }
}
