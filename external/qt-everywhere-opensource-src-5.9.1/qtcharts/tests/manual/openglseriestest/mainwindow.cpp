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
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>

QT_CHARTS_USE_NAMESPACE
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_xMin(0.0),
    m_xMax(20.0),
    m_yMin(0.0),
    m_yMax(10.0),
    m_backgroundBrush(new QBrush(Qt::white)),
    m_plotAreaBackgroundBrush(new QBrush(Qt::NoBrush)),
    m_backgroundPen(new QPen(Qt::NoPen)),
    m_plotAreaBackgroundPen(new QPen(Qt::NoPen)),
    m_animationOptions(QChart::NoAnimation),
    m_chart(0),
    m_xAxis(0),
    m_yAxis(0),
    m_xAxisMode(AxisModeValue),
    m_yAxisMode(AxisModeValue),
    m_pointCount(100)
{
    ui->setupUi(this);

    ui->yMinSpin->setValue(m_yMin);
    ui->yMaxSpin->setValue(m_yMax);
    ui->xMinSpin->setValue(m_xMin);
    ui->xMaxSpin->setValue(m_xMax);

    initXYValueChart();
    setXAxis(AxisModeValue);
    setYAxis(AxisModeValue);

    connect(ui->yMinSpin, SIGNAL(valueChanged(double)),
            this, SLOT(yMinChanged(double)));
    connect(ui->yMaxSpin, SIGNAL(valueChanged(double)),
            this, SLOT(yMaxChanged(double)));
    connect(ui->xMinSpin, SIGNAL(valueChanged(double)),
            this, SLOT(xMinChanged(double)));
    connect(ui->xMaxSpin, SIGNAL(valueChanged(double)),
            this, SLOT(xMaxChanged(double)));
    connect(ui->animationsComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(animationIndexChanged(int)));
    connect(ui->xAxisComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(xAxisIndexChanged(int)));
    connect(ui->yAxisComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(yAxisIndexChanged(int)));
    connect(ui->backgroundComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(backgroundIndexChanged(int)));
    connect(ui->plotAreaComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(plotAreaIndexChanged(int)));
    connect(ui->themeComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(themeIndexChanged(int)));
    connect(ui->addSeriesButton, SIGNAL(clicked(bool)),
            this, SLOT(addSeriesClicked()));
    connect(ui->addGLSeriesButton, SIGNAL(clicked(bool)),
            this, SLOT(addGLSeriesClicked()));
    connect(ui->removeSeriesButton, SIGNAL(clicked(bool)),
            this, SLOT(removeSeriesClicked()));
    connect(ui->countComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(countIndexChanged(int)));
    connect(ui->colorsComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(colorIndexChanged(int)));
    connect(ui->widthComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(widthIndexChanged(int)));
    connect(ui->antiAliasCheckBox, SIGNAL(clicked(bool)),
            this, SLOT(antiAliasCheckBoxClicked(bool)));
    connect(ui->intervalSpinbox, SIGNAL(valueChanged(int)),
            &m_dataSource, SLOT(setInterval(int)));

    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);

    QObject::connect(m_chart->scene(), &QGraphicsScene::changed,
                     &m_dataSource, &DataSource::handleSceneChanged);

    m_dataSource.startUpdates(m_seriesList, ui->fpsLabel, ui->intervalSpinbox->value());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initXYValueChart()
{
    m_chart = new QChart();
    m_chart->setTitle("Use arrow keys to scroll and +/- to zoom");
    m_chart->setAnimationOptions(m_animationOptions);
    m_chart->setBackgroundBrush(*m_backgroundBrush);
    m_chart->setBackgroundPen(*m_backgroundPen);
    m_chart->setPlotAreaBackgroundBrush(*m_plotAreaBackgroundBrush);
    m_chart->setPlotAreaBackgroundPen(*m_plotAreaBackgroundPen);
}

void MainWindow::setXAxis(MainWindow::AxisMode mode)
{
    if (m_xAxis) {
        m_chart->removeAxis(m_xAxis);
        delete m_xAxis;
        m_xAxis = 0;
    }

    m_xAxisMode = mode;

    switch (m_xAxisMode) {
    case AxisModeNone:
        return;
    case AxisModeValue:
        m_xAxis = new QValueAxis();
        break;
    case AxisModeLogValue:
        m_xAxis = new QLogValueAxis();
        break;
    case AxisModeDateTime:
        m_xAxis = new QDateTimeAxis();
        break;
    case AxisModeCategory:
        m_xAxis = new QCategoryAxis();
        applyCategories();
        break;
    default:
        qWarning() << "Unsupported AxisMode";
        return;
    }

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);

    foreach (QAbstractSeries *series, m_seriesList)
        series->attachAxis(m_xAxis);

    applyRanges();
}

void MainWindow::setYAxis(MainWindow::AxisMode mode)
{
    if (m_yAxis) {
        m_chart->removeAxis(m_yAxis);
        delete m_yAxis;
        m_yAxis = 0;
    }

    m_yAxisMode = mode;

    switch (m_yAxisMode) {
    case AxisModeNone:
        return;
    case AxisModeValue:
        m_yAxis = new QValueAxis();
        break;
    case AxisModeLogValue:
        m_yAxis = new QLogValueAxis();
        break;
    case AxisModeDateTime:
        m_yAxis = new QDateTimeAxis();
        break;
    case AxisModeCategory:
        m_yAxis = new QCategoryAxis();
        applyCategories();
        break;
    default:
        qWarning() << "Unsupported AxisMode";
        return;
    }

    m_chart->addAxis(m_yAxis, Qt::AlignLeft);

    foreach (QAbstractSeries *series, m_seriesList)
        series->attachAxis(m_yAxis);

    applyRanges();
}

void MainWindow::applyRanges()
{
    if (m_xAxis) {
        if (m_xAxisMode == AxisModeLogValue) {
            if (m_xMin <= 0)
                m_xMin = 1.0;
            if (m_xMax <= m_xMin)
                m_xMax = m_xMin + 1.0;
        }
        if (m_xAxisMode == AxisModeDateTime) {
            QDateTime dateTimeMin;
            QDateTime dateTimeMax;
            dateTimeMin.setMSecsSinceEpoch(qint64(m_xMin));
            dateTimeMax.setMSecsSinceEpoch(qint64(m_xMax));
            m_xAxis->setRange(dateTimeMin, dateTimeMax);
        } else {
            m_xAxis->setRange(m_xMin, m_xMax);
        }
        ui->xMinSpin->setValue(m_xMin);
        ui->xMaxSpin->setValue(m_xMax);
    }
    if (m_yAxis) {
        if (m_yAxisMode == AxisModeLogValue) {
            if (m_yMin <= 0)
                m_yMin = 1.0;
            if (m_yMax <= m_yMin)
                m_yMax = m_yMin + 1.0;
        }
        if (m_yAxisMode == AxisModeDateTime) {
            QDateTime dateTimeMin;
            QDateTime dateTimeMax;
            dateTimeMin.setMSecsSinceEpoch(qint64(m_yMin));
            dateTimeMax.setMSecsSinceEpoch(qint64(m_yMax));
            m_yAxis->setRange(dateTimeMin, dateTimeMax);
        } else {
            m_yAxis->setRange(m_yMin, m_yMax);
        }
        ui->yMinSpin->setValue(m_yMin);
        ui->yMaxSpin->setValue(m_yMax);
    }
}

void MainWindow::xMinChanged(double value)
{
    m_xMin = value;
    applyRanges();
}

void MainWindow::xMaxChanged(double value)
{
    m_xMax = value;
    applyRanges();
}

void MainWindow::yMinChanged(double value)
{
    m_yMin = value;
    applyRanges();
}

void MainWindow::yMaxChanged(double value)
{
    m_yMax = value;
    applyRanges();
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

void MainWindow::xRangeChanged(qreal min, qreal max)
{
    if (!qFuzzyCompare(qreal(ui->xMinSpin->value()), min))
        ui->xMinSpin->setValue(min);
    if (!qFuzzyCompare(qreal(ui->xMaxSpin->value()), max))
        ui->xMaxSpin->setValue(max);
}

void MainWindow::yRangeChanged(qreal min, qreal max)
{
    if (!qFuzzyCompare(qreal(ui->yMinSpin->value()), min))
        ui->yMinSpin->setValue(min);
    if (!qFuzzyCompare(qreal(ui->yMaxSpin->value()), max))
        ui->yMaxSpin->setValue(max);
}

void MainWindow::xAxisIndexChanged(int index)
{
    switch (index) {
    case 0:
        setXAxis(AxisModeNone);
        break;
    case 1:
        setXAxis(AxisModeValue);
        break;
    case 2:
        setXAxis(AxisModeLogValue);
        break;
    case 3:
        setXAxis(AxisModeDateTime);
        break;
    case 4:
        setXAxis(AxisModeCategory);
        break;
    default:
        qWarning("Invalid Index!");
    }
}

void MainWindow::yAxisIndexChanged(int index)
{
    switch (index) {
    case 0:
        setYAxis(AxisModeNone);
        break;
    case 1:
        setYAxis(AxisModeValue);
        break;
    case 2:
        setYAxis(AxisModeLogValue);
        break;
    case 3:
        setYAxis(AxisModeDateTime);
        break;
    case 4:
        setYAxis(AxisModeCategory);
        break;
    default:
        qWarning("Invalid Index!");
    }
}

void MainWindow::themeIndexChanged(int index)
{
    m_chart->setTheme(QChart::ChartTheme(index));
}

void MainWindow::addSeriesClicked()
{
    addSeries(false);
}

void MainWindow::removeSeriesClicked()
{
    if (m_seriesList.size()) {
        QXYSeries *series = m_seriesList.takeAt(m_seriesList.size() - 1);
        m_chart->removeSeries(series);
        delete series;
    }
}

void MainWindow::addGLSeriesClicked()
{
    addSeries(true);
}

void MainWindow::countIndexChanged(int index)
{
    m_pointCount = ui->countComboBox->itemText(index).toInt();
    for (int i = 0; i < m_seriesList.size(); i++) {
        m_dataSource.generateData(i, 2, m_pointCount);
    }
}

void MainWindow::colorIndexChanged(int index)
{
    QColor color = QColor(ui->colorsComboBox->itemText(index).toLower());
    foreach (QXYSeries *series, m_seriesList) {
        if (series->type() == QAbstractSeries::SeriesTypeScatter) {
            QScatterSeries *scatterSeries = static_cast<QScatterSeries *>(series);
            scatterSeries->setBorderColor(color);
            scatterSeries->setColor(color);
        } else {
            series->setColor(color);
        }
    }
}

void MainWindow::widthIndexChanged(int index)
{
    int width = ui->widthComboBox->itemText(index).toInt();
    foreach (QXYSeries *series, m_seriesList) {
        if (series->type() == QAbstractSeries::SeriesTypeScatter) {
            QScatterSeries *scatterSeries = static_cast<QScatterSeries *>(series);
            scatterSeries->setMarkerSize(width);
        } else {
            QColor color = QColor(ui->colorsComboBox->itemText(
                                      ui->colorsComboBox->currentIndex()).toLower());
            series->setPen(QPen(QBrush(color), width));
        }
    }
}

void MainWindow::antiAliasCheckBoxClicked(bool checked)
{
    ui->chartView->setRenderHint(QPainter::Antialiasing, checked);
}

void MainWindow::handleHovered(const QPointF &point, bool state)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    if (series) {
        qDebug() << __FUNCTION__ << series->name() << point << state;
        const QString labelTemplate = QStringLiteral("%3: %1 x %2 - %4");
        ui->coordinatesLabel->setText(
                    labelTemplate.arg(point.x()).arg(point.y()).arg(series->name())
                    .arg(state ? QStringLiteral("enter") : QStringLiteral("leave")));
    }
}

void MainWindow::handleClicked(const QPointF &point)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    qDebug() << __FUNCTION__ << series->name() << point;
}

void MainWindow::handlePressed(const QPointF &point)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    qDebug() << __FUNCTION__ << series->name() << point;
}

void MainWindow::handleReleased(const QPointF &point)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    qDebug() << __FUNCTION__ << series->name() << point;
}

void MainWindow::handleDoubleClicked(const QPointF &point)
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *>(sender());
    qDebug() << __FUNCTION__ << series->name() << point;
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
    if (m_xAxisMode == AxisModeCategory) {
        QCategoryAxis *angCatAxis = static_cast<QCategoryAxis *>(m_xAxis);
        if (angCatAxis->count() == 0) {
            angCatAxis->setStartValue(2);
            angCatAxis->append("Category A", 6);
            angCatAxis->append("Category B", 12);
            angCatAxis->append("Category C", 18);
        }
    }

    if (m_yAxisMode == AxisModeCategory) {
        QCategoryAxis *radCatAxis = static_cast<QCategoryAxis *>(m_yAxis);
        if (radCatAxis->count() == 0) {
            radCatAxis->setStartValue(1);
            radCatAxis->append("Category 1", 3);
            radCatAxis->append("Category 2", 4);
            radCatAxis->append("Category 3", 8);
        }
    }
}

void MainWindow::addSeries(bool gl)
{
    QColor color = QColor(ui->colorsComboBox->itemText(ui->colorsComboBox->currentIndex()).toLower());
    int width = ui->widthComboBox->itemText(ui->widthComboBox->currentIndex()).toInt();

    if (m_seriesList.size() < maxSeriesCount) {
        QXYSeries *series;
        if (qrand() % 2) {
            series = new QLineSeries;
            series->setPen(QPen(QBrush(color), width));
        } else {
            QScatterSeries *scatterSeries = new QScatterSeries;
            scatterSeries->setMarkerSize(width);
            scatterSeries->setBorderColor(color);
            scatterSeries->setBrush(QBrush(color));
            series = scatterSeries;
        }
        series->setUseOpenGL(gl);
        const QString glNameTemplate = QStringLiteral("GL_%1");
        const QString rasterNameTemplate = QStringLiteral("Raster_%1");
        if (gl)
            series->setName(glNameTemplate.arg(m_seriesList.size()));
        else
            series->setName(rasterNameTemplate.arg(m_seriesList.size()));
        m_dataSource.generateData(m_seriesList.size(), 2, m_pointCount);
        m_seriesList.append(series);
        m_chart->addSeries(series);
        if (m_xAxis)
            series->attachAxis(m_xAxis);
        if (m_yAxis)
            series->attachAxis(m_yAxis);
        applyRanges();

        QObject::connect(series, &QXYSeries::hovered, this, &MainWindow::handleHovered);
        QObject::connect(series, &QXYSeries::clicked, this, &MainWindow::handleClicked);
        QObject::connect(series, &QXYSeries::pressed, this, &MainWindow::handlePressed);
        QObject::connect(series, &QXYSeries::released, this, &MainWindow::handleReleased);
        QObject::connect(series, &QXYSeries::doubleClicked, this, &MainWindow::handleDoubleClicked);
    }
}
