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

#include "mainwidget.h"
#include "customtablemodel.h"
#include "pentool.h"
#include <QtCharts/QVBoxPlotModelMapper>
#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtCharts/QChartView>
#include <QtCharts/QBoxPlotSeries>
#include <QtCharts/QBoxSet>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QMessageBox>
#include <cmath>
#include <QtCore/QDebug>
#include <QtGui/QStandardItemModel>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QLogValueAxis>

QT_CHARTS_USE_NAMESPACE

static const QString allCategories[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const int maxCategories = 12;

MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    m_chart(0),
    m_axis(0),
    m_rowPos(0),
    m_seriesCount(0)
{
    m_chart = new QChart();

    m_penTool = new PenTool("Whiskers pen", this);

    // Grid layout for the controls for configuring the chart widget
    QGridLayout *grid = new QGridLayout();

    // Create add a series button
    QPushButton *addSeriesButton = new QPushButton("Add a series");
    connect(addSeriesButton, SIGNAL(clicked()), this, SLOT(addSeries()));
    grid->addWidget(addSeriesButton, m_rowPos++, 1);

    // Create remove a series button
    QPushButton *removeSeriesButton = new QPushButton("Remove a series");
    connect(removeSeriesButton, SIGNAL(clicked()), this, SLOT(removeSeries()));
    grid->addWidget(removeSeriesButton, m_rowPos++, 1);

    // Create add a single box button
    QPushButton *addBoxButton = new QPushButton("Add a box");
    connect(addBoxButton, SIGNAL(clicked()), this, SLOT(addBox()));
    grid->addWidget(addBoxButton, m_rowPos++, 1);

    // Create insert a box button
    QPushButton *insertBoxButton = new QPushButton("Insert a box");
    connect(insertBoxButton, SIGNAL(clicked()), this, SLOT(insertBox()));
    grid->addWidget(insertBoxButton, m_rowPos++, 1);

    // Create add a single box button
    QPushButton *removeBoxButton = new QPushButton("Remove a box");
    connect(removeBoxButton, SIGNAL(clicked()), this, SLOT(removeBox()));
    grid->addWidget(removeBoxButton, m_rowPos++, 1);

    // Create clear button
    QPushButton *clearButton = new QPushButton("Clear");
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    grid->addWidget(clearButton, m_rowPos++, 1);

    // Create clear button
    QPushButton *clearBoxButton = new QPushButton("ClearBox");
    connect(clearBoxButton, SIGNAL(clicked()), this, SLOT(clearBox()));
    grid->addWidget(clearBoxButton, m_rowPos++, 1);

    // Create set brush button
    QPushButton *setBrushButton = new QPushButton("Set brush");
    connect(setBrushButton, SIGNAL(clicked()), this, SLOT(setBrush()));
    grid->addWidget(setBrushButton, m_rowPos++, 1);

    // Create set whiskers pen button
    QPushButton *setWhiskersButton = new QPushButton("Whiskers pen");
    connect(setWhiskersButton, SIGNAL(clicked()), m_penTool, SLOT(show()));
    connect(m_penTool, SIGNAL(changed()), this, SLOT(changePen()));
    grid->addWidget(setWhiskersButton, m_rowPos++, 1);

    // Box width setting
    m_boxWidthSB = new QDoubleSpinBox();
    m_boxWidthSB->setMinimum(-1.0);
    m_boxWidthSB->setMaximum(2.0);
    m_boxWidthSB->setSingleStep(0.1);
    m_boxWidthSB->setValue(0.5);
    grid->addWidget(new QLabel("Box width:"), m_rowPos, 0);
    grid->addWidget(m_boxWidthSB, m_rowPos++, 1);
    connect(m_boxWidthSB, SIGNAL(valueChanged(double)), this, SLOT(setBoxWidth(double)));

    initThemeCombo(grid);
    initCheckboxes(grid);

    QTableView *tableView = new QTableView;
    m_model = new CustomTableModel(tableView);
    tableView->setModel(m_model);
    tableView->setMaximumWidth(200);
    grid->addWidget(tableView, m_rowPos++, 0, 3, 2, Qt::AlignLeft);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // add row with empty label to make all the other rows static
    grid->addWidget(new QLabel(""), grid->rowCount(), 0);
    grid->setRowStretch(grid->rowCount() - 1, 1);

    // Create chart view with the chart
    m_chartView = new QChartView(m_chart, this);

    // As a default antialiasing is off
    m_chartView->setRenderHint(QPainter::Antialiasing, false);

    // Another grid layout as a main layout
    QGridLayout *mainLayout = new QGridLayout();
    mainLayout->addLayout(grid, 0, 0);
    mainLayout->addWidget(m_chartView, 0, 1, 3, 1);
    setLayout(mainLayout);

    legendToggled(false);
    animationToggled(false);
}

// Combo box for selecting theme
void MainWidget::initThemeCombo(QGridLayout *grid)
{
    QComboBox *chartTheme = new QComboBox();
    chartTheme->addItem("Default");
    chartTheme->addItem("Light");
    chartTheme->addItem("Blue Cerulean");
    chartTheme->addItem("Dark");
    chartTheme->addItem("Brown Sand");
    chartTheme->addItem("Blue NCS");
    chartTheme->addItem("High Contrast");
    chartTheme->addItem("Blue Icy");
    chartTheme->addItem("Qt");
    connect(chartTheme, SIGNAL(currentIndexChanged(int)),
            this, SLOT(changeChartTheme(int)));
    grid->addWidget(new QLabel("Chart theme:"), m_rowPos, 0);
    grid->addWidget(chartTheme, m_rowPos++, 1);
}

// Different check boxes for customizing chart
void MainWidget::initCheckboxes(QGridLayout *grid)
{
    QCheckBox *animationCheckBox = new QCheckBox("Animation");
    connect(animationCheckBox, SIGNAL(toggled(bool)), this, SLOT(animationToggled(bool)));
    animationCheckBox->setChecked(false);
    grid->addWidget(animationCheckBox, m_rowPos++, 0);

    QCheckBox *legendCheckBox = new QCheckBox("Legend");
    connect(legendCheckBox, SIGNAL(toggled(bool)), this, SLOT(legendToggled(bool)));
    legendCheckBox->setChecked(false);
    grid->addWidget(legendCheckBox, m_rowPos++, 0);

    QCheckBox *titleCheckBox = new QCheckBox("Title");
    connect(titleCheckBox, SIGNAL(toggled(bool)), this, SLOT(titleToggled(bool)));
    titleCheckBox->setChecked(false);
    grid->addWidget(titleCheckBox, m_rowPos++, 0);

    QCheckBox *antialiasingCheckBox = new QCheckBox("Antialiasing");
    connect(antialiasingCheckBox, SIGNAL(toggled(bool)), this, SLOT(antialiasingToggled(bool)));
    antialiasingCheckBox->setChecked(false);
    grid->addWidget(antialiasingCheckBox, m_rowPos++, 0);

    QCheckBox *modelMapperCheckBox = new QCheckBox("Use model mapper");
    connect(modelMapperCheckBox, SIGNAL(toggled(bool)), this, SLOT(modelMapperToggled(bool)));
    modelMapperCheckBox->setChecked(false);
    grid->addWidget(modelMapperCheckBox, m_rowPos++, 0);

    m_boxOutlined = new QCheckBox("Box outlined");
    connect(m_boxOutlined, SIGNAL(toggled(bool)), this, SLOT(boxOutlineToggled(bool)));
    m_boxOutlined->setChecked(true);
    grid->addWidget(m_boxOutlined, m_rowPos++, 0);
}

void MainWidget::updateAxis(int categoryCount)
{
    if (!m_axis) {
        m_chart->createDefaultAxes();
        m_axis = new QBarCategoryAxis();
    }
    QStringList categories;
    for (int i = 0; i < categoryCount; i++)
        categories << allCategories[i];
    m_axis->setCategories(categories);
}

void MainWidget::addSeries()
{
    qDebug() << "BoxPlotTester::MainWidget::addSeries()";

    if (m_seriesCount > 9)
        return;

    // Initial data
    QBoxSet *set0 = new QBoxSet();
    QBoxSet *set1 = new QBoxSet();
    QBoxSet *set2 = new QBoxSet();
    QBoxSet *set3 = new QBoxSet();
    QBoxSet *set4 = new QBoxSet();
    QBoxSet *set5 = new QBoxSet();

    //      low  bot   med   top  upp
    *set0 << -1 << 2 << 4 << 13 << 15;
    *set1 << 5 << 6 << 7.5 << 8 << 12;
    *set2 << 3 << 5 << 5.7 << 8 << 9;
    *set3 << 5 << 6 << 6.8 << 7 << 8;
    *set4 << 4 << 5 << 5.2 << 6 << 7;
    *set5 << 4 << 7 << 8.2 << 9 << 10;

    m_series[m_seriesCount] = new QBoxPlotSeries();
    m_series[m_seriesCount]->append(set0);
    m_series[m_seriesCount]->append(set1);
    m_series[m_seriesCount]->append(set2);
    m_series[m_seriesCount]->append(set3);
    m_series[m_seriesCount]->append(set4);
    m_series[m_seriesCount]->append(set5);
    m_series[m_seriesCount]->setName("Box & Whiskers");

    connect(m_series[m_seriesCount], SIGNAL(clicked(QBoxSet*)), this, SLOT(boxClicked(QBoxSet*)));
    connect(m_series[m_seriesCount], SIGNAL(pressed(QBoxSet*)), this, SLOT(boxPressed(QBoxSet*)));
    connect(m_series[m_seriesCount], SIGNAL(released(QBoxSet*)), this, SLOT(boxReleased(QBoxSet*)));
    connect(m_series[m_seriesCount], SIGNAL(doubleClicked(QBoxSet*)),
            this, SLOT(boxDoubleClicked(QBoxSet*)));
    connect(m_series[m_seriesCount], SIGNAL(hovered(bool, QBoxSet*)), this, SLOT(boxHovered(bool, QBoxSet*)));
    connect(set1, SIGNAL(clicked()), this, SLOT(singleBoxClicked()));
    connect(set1, SIGNAL(pressed()), this, SLOT(singleBoxPressed()));
    connect(set1, SIGNAL(released()), this, SLOT(singleBoxReleased()));
    connect(set1, SIGNAL(doubleClicked()), this, SLOT(singleBoxDoubleClicked()));
    connect(set2, SIGNAL(hovered(bool)), this, SLOT(singleBoxHovered(bool)));

    m_series[m_seriesCount]->setBoxOutlineVisible(m_boxOutlined->checkState());
    m_series[m_seriesCount]->setBoxWidth(m_boxWidthSB->value());

    m_chart->addSeries(m_series[m_seriesCount]);

    updateAxis(m_series[0]->count());
    m_chart->setAxisX(m_axis, m_series[m_seriesCount]);

    m_seriesCount++;
}

void MainWidget::removeSeries()
{
    qDebug() << "BoxPlotTester::MainWidget::removeSeries()";

    if (m_seriesCount > 0) {
        m_seriesCount--;
        m_chart->removeSeries(m_series[m_seriesCount]);
        delete m_series[m_seriesCount];
        m_series[m_seriesCount] = 0;
    } else {
        qDebug() << "Create a series first";
    }
}

void MainWidget::addBox()
{
    qDebug() << "BoxPlotTester::MainWidget::addBox()";

    if (m_seriesCount > 0 && m_series[0]->count() < maxCategories) {
        QBoxSet *newSet = new QBoxSet();
        newSet->setValue(QBoxSet::LowerExtreme, 5.0);
        newSet->setValue(QBoxSet::LowerQuartile, 6.0);
        newSet->setValue(QBoxSet::Median, 6.8);
        newSet->setValue(QBoxSet::UpperQuartile, 7.0);
        newSet->setValue(QBoxSet::UpperExtreme, 8.0);

        updateAxis(m_series[0]->count() + 1);

        m_series[0]->append(newSet);
    }
}

void MainWidget::insertBox()
{
    qDebug() << "BoxPlotTester::MainWidget::insertBox()";

    if (m_seriesCount > 0 && m_series[0]->count() < maxCategories) {
        updateAxis(m_series[0]->count() + 1);
        for (int i = 0; i < m_seriesCount; i++) {
            QBoxSet *newSet = new QBoxSet();
            *newSet << 2 << 6 << 6.8 << 7 << 10;
            m_series[i]->insert(1, newSet);
        }
    }
}

void MainWidget::removeBox()
{
    qDebug() << "BoxPlotTester::MainWidget::removeBox";

    if (m_seriesCount > 0) {
        for (int i = 0; i < m_seriesCount; i++) {
            qDebug() << "m_series[i]->count() = " << m_series[i]->count();
            if (m_series[i]->count()) {
                QList<QBoxSet *> sets = m_series[i]->boxSets();
                m_series[i]->remove(sets.at(m_series[i]->count() - 1));
            }
        }

        updateAxis(m_series[0]->count());
    } else {
        qDebug() << "Create a series first";
    }
}

void MainWidget::clear()
{
    qDebug() << "BoxPlotTester::MainWidget::clear";

    if (m_seriesCount > 0)
        m_series[0]->clear();
    else
        qDebug() << "Create a series first";
}

void MainWidget::clearBox()
{
    qDebug() << "BoxPlotTester::MainWidget::clearBox";

    if (m_seriesCount > 0) {
        QList<QBoxSet *> sets = m_series[0]->boxSets();
        if (sets.count() > 1)
            sets.at(1)->clear();
        else
            qDebug() << "Create a series with at least two items first";
    } else {
        qDebug() << "Create a series first";
    }
}

void MainWidget::setBrush()
{
    qDebug() << "BoxPlotTester::MainWidget::setBrush";

    if (m_seriesCount > 0) {
        QList<QBoxSet *> sets = m_series[0]->boxSets();
        if (sets.count() > 1)
            sets.at(1)->setBrush(QBrush(QColor(Qt::yellow)));
        else
            qDebug() << "Create a series with at least two items first";
    } else {
        qDebug() << "Create a series first";
    }
}

void MainWidget::animationToggled(bool enabled)
{
    qDebug() << "BoxPlotTester::Animation toggled to " << enabled;
    if (enabled)
        m_chart->setAnimationOptions(QChart::SeriesAnimations);
    else
        m_chart->setAnimationOptions(QChart::NoAnimation);
}

void MainWidget::legendToggled(bool enabled)
{
    qDebug() << "BoxPlotTester::Legend toggled to " << enabled;
    m_chart->legend()->setVisible(enabled);
    if (enabled)
        m_chart->legend()->setAlignment(Qt::AlignBottom);
}

void MainWidget::titleToggled(bool enabled)
{
    qDebug() << "BoxPlotTester::Title toggled to " << enabled;
    if (enabled)
        m_chart->setTitle("Simple boxplotchart example");
    else
        m_chart->setTitle("");
}

void MainWidget::antialiasingToggled(bool enabled)
{
    qDebug() << "BoxPlotTester::antialiasingToggled toggled to " << enabled;
    m_chartView->setRenderHint(QPainter::Antialiasing, enabled);
}

void MainWidget::boxOutlineToggled(bool visible)
{
    qDebug() << "BoxPlotTester::boxOutlineToggled toggled to " << visible;
    for (int i = 0; i <  m_seriesCount; i++)
        m_series[i]->setBoxOutlineVisible(visible);
}

void MainWidget::modelMapperToggled(bool enabled)
{
    if (enabled) {
        m_series[m_seriesCount] = new QBoxPlotSeries();

        int first = 0;
        int count = 5;
        QVBoxPlotModelMapper *mapper = new QVBoxPlotModelMapper(this);
        mapper->setFirstBoxSetColumn(0);
        mapper->setLastBoxSetColumn(5);
        mapper->setFirstRow(first);
        mapper->setRowCount(count);
        mapper->setSeries(m_series[m_seriesCount]);
        mapper->setModel(m_model);
        m_chart->addSeries(m_series[m_seriesCount]);

        m_seriesCount++;
    } else {
        removeSeries();
    }
}

void MainWidget::changeChartTheme(int themeIndex)
{
    qDebug() << "BoxPlotTester::changeChartTheme: " << themeIndex;
    if (themeIndex == 0)
        m_chart->setTheme(QChart::ChartThemeLight);
    else
        m_chart->setTheme((QChart::ChartTheme) (themeIndex - 1));
}

void MainWidget::boxClicked(QBoxSet *set)
{
    qDebug() << "boxClicked, median = " << set->at(QBoxSet::Median);
}

void MainWidget::boxHovered(bool state, QBoxSet *set)
{
    if (state)
        qDebug() << "box median " << set->at(QBoxSet::Median) << " hover started";
    else
        qDebug() << "box median " << set->at(QBoxSet::Median) << " hover ended";
}

void MainWidget::boxPressed(QBoxSet *set)
{
    qDebug() << "boxPressed, median = " << set->at(QBoxSet::Median);
}

void MainWidget::boxReleased(QBoxSet *set)
{
    qDebug() << "boxReleased, median = " << set->at(QBoxSet::Median);
}

void MainWidget::boxDoubleClicked(QBoxSet *set)
{
    qDebug() << "boxDoubleClicked, median = " << set->at(QBoxSet::Median);
}

void MainWidget::singleBoxClicked()
{
    qDebug() << "singleBoxClicked";
}

void MainWidget::singleBoxPressed()
{
    qDebug() << "singleBoxPressed";
}

void MainWidget::singleBoxReleased()
{
    qDebug() << "singleBoxReleased";
}

void MainWidget::singleBoxDoubleClicked()
{
    qDebug() << "singleBoxDoubleClicked";
}

void MainWidget::singleBoxHovered(bool state)
{
    if (state)
        qDebug() << "single box hover started";
    else
        qDebug() << "single box hover ended";
}

void MainWidget::changePen()
{
    qDebug() << "changePen() = " << m_penTool->pen();
    for (int i = 0; i <  m_seriesCount; i++)
        m_series[i]->setPen(m_penTool->pen());
}

void MainWidget::setBoxWidth(double width)
{
    qDebug() << "setBoxWidth to " << width;

    for (int i = 0; i <  m_seriesCount; i++)
        m_series[i]->setBoxWidth(qreal(width));
}
