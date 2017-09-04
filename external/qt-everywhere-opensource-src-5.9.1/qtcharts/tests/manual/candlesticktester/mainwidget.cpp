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

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QHCandlestickModelMapper>
#include <QtCharts/QValueAxis>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include "customtablemodel.h"
#include "mainwidget.h"

QT_CHARTS_USE_NAMESPACE

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      m_chart(new QChart()),
      m_chartView(new QChartView(m_chart, this)),
      m_axisX(nullptr),
      m_axisY(nullptr),
      m_maximumColumnWidth(-1.0),
      m_minimumColumnWidth(5.0),
      m_bodyOutlineVisible(true),
      m_capsVisible(false),
      m_bodyWidth(0.5),
      m_capsWidth(0.5),
      m_customIncreasingColor(false),
      m_customDecreasingColor(false),
      m_hModelMapper(new QHCandlestickModelMapper(this))
{
    qsrand(QDateTime::currentDateTime().toTime_t());

    m_chartView->setRenderHint(QPainter::Antialiasing, false);

    m_hModelMapper->setModel(new CustomTableModel(this));
    m_hModelMapper->setTimestampColumn(0);
    m_hModelMapper->setOpenColumn(1);
    m_hModelMapper->setHighColumn(2);
    m_hModelMapper->setLowColumn(3);
    m_hModelMapper->setCloseColumn(4);

    QGridLayout *mainLayout = new QGridLayout();
    mainLayout->addLayout(createSeriesControlsLayout(), 0, 0);
    mainLayout->addLayout(createSetsControlsLayout(), 1, 0);
    mainLayout->addLayout(createCandlestickControlsLayout(), 2, 0);
    mainLayout->addLayout(createMiscellaneousControlsLayout(), 3, 0);
    mainLayout->addWidget(m_chartView, 0, 1, mainLayout->rowCount() + 1, 1);
    mainLayout->addLayout(createModelMapperControlsLayout(), 0, 2, mainLayout->rowCount(), 1);
    setLayout(mainLayout);

    addSeries();
}

MainWidget::~MainWidget()
{
}

QGridLayout *MainWidget::createSeriesControlsLayout()
{
    QGridLayout *layout = new QGridLayout();
    int row = 0;

    layout->addWidget(new QLabel(QStringLiteral("Series controls:")), row, 0, Qt::AlignLeft);

    QPushButton *addSeriesButton = new QPushButton(QStringLiteral("Add a series"));
    connect(addSeriesButton, SIGNAL(clicked(bool)), this, SLOT(addSeries()));
    layout->addWidget(addSeriesButton, row++, 1, Qt::AlignLeft);

    QPushButton *removeSeriesButton = new QPushButton(QStringLiteral("Remove a series"));
    connect(removeSeriesButton, SIGNAL(clicked(bool)), this, SLOT(removeSeries()));
    layout->addWidget(removeSeriesButton, row++, 1, Qt::AlignLeft);

    QPushButton *removeAllSeriesButton = new QPushButton(QStringLiteral("Remove all series"));
    connect(removeAllSeriesButton, SIGNAL(clicked(bool)), this, SLOT(removeAllSeries()));
    layout->addWidget(removeAllSeriesButton, row++, 1, Qt::AlignLeft);

    return layout;
}

QGridLayout *MainWidget::createSetsControlsLayout()
{
    QGridLayout *layout = new QGridLayout();
    int row = 0;

    layout->addWidget(new QLabel(QStringLiteral("Sets controls:")), row, 0, Qt::AlignLeft);

    QPushButton *addSetButton = new QPushButton(QStringLiteral("Add a set"));
    connect(addSetButton, SIGNAL(clicked(bool)), this, SLOT(addSet()));
    layout->addWidget(addSetButton, row++, 1, Qt::AlignLeft);

    QPushButton *insertSetButton = new QPushButton(QStringLiteral("Insert a set"));
    connect(insertSetButton, SIGNAL(clicked(bool)), this, SLOT(insertSet()));
    layout->addWidget(insertSetButton, row++, 1, Qt::AlignLeft);

    QPushButton *removeSetButton = new QPushButton(QStringLiteral("Remove a set"));
    connect(removeSetButton, SIGNAL(clicked(bool)), this, SLOT(removeSet()));
    layout->addWidget(removeSetButton, row++, 1, Qt::AlignLeft);

    QPushButton *removeAllSetsButton = new QPushButton(QStringLiteral("Remove all sets"));
    connect(removeAllSetsButton, SIGNAL(clicked(bool)), this, SLOT(removeAllSets()));
    layout->addWidget(removeAllSetsButton, row++, 1, Qt::AlignLeft);

    return layout;
}

QGridLayout *MainWidget::createCandlestickControlsLayout()
{
    QGridLayout *layout = new QGridLayout();
    int row = 0;

    layout->addWidget(new QLabel(QStringLiteral("Maximum column width:")), row, 0, Qt::AlignLeft);
    QDoubleSpinBox *maximumColumnWidthSpinBox = new QDoubleSpinBox();
    maximumColumnWidthSpinBox->setRange(-1.0, 1024.0);
    maximumColumnWidthSpinBox->setDecimals(0);
    maximumColumnWidthSpinBox->setValue(m_maximumColumnWidth);
    maximumColumnWidthSpinBox->setSingleStep(1.0);
    connect(maximumColumnWidthSpinBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeMaximumColumnWidth(double)));
    layout->addWidget(maximumColumnWidthSpinBox, row++, 1, Qt::AlignLeft);

    layout->addWidget(new QLabel(QStringLiteral("Minimum column width:")), row, 0, Qt::AlignLeft);
    QDoubleSpinBox *minimumColumnWidthSpinBox = new QDoubleSpinBox();
    minimumColumnWidthSpinBox->setRange(-1.0, 1024.0);
    minimumColumnWidthSpinBox->setDecimals(0);
    minimumColumnWidthSpinBox->setValue(m_minimumColumnWidth);
    minimumColumnWidthSpinBox->setSingleStep(1.0);
    connect(minimumColumnWidthSpinBox, SIGNAL(valueChanged(double)),
            this, SLOT(changeMinimumColumnWidth(double)));
    layout->addWidget(minimumColumnWidthSpinBox, row++, 1, Qt::AlignLeft);

    QCheckBox *bodyOutlineVisible = new QCheckBox(QStringLiteral("Body outline visible"));
    connect(bodyOutlineVisible, SIGNAL(toggled(bool)), this, SLOT(bodyOutlineVisibleToggled(bool)));
    bodyOutlineVisible->setChecked(m_bodyOutlineVisible);
    layout->addWidget(bodyOutlineVisible, row++, 0, Qt::AlignLeft);

    QCheckBox *capsVisible = new QCheckBox(QStringLiteral("Caps visible"));
    connect(capsVisible, SIGNAL(toggled(bool)), this, SLOT(capsVisibleToggled(bool)));
    capsVisible->setChecked(m_capsVisible);
    layout->addWidget(capsVisible, row++, 0, Qt::AlignLeft);

    layout->addWidget(new QLabel(QStringLiteral("Candlestick body width:")), row, 0, Qt::AlignLeft);
    QDoubleSpinBox *bodyWidthSpinBox = new QDoubleSpinBox();
    bodyWidthSpinBox->setRange(-1.0, 2.0);
    bodyWidthSpinBox->setValue(m_bodyWidth);
    bodyWidthSpinBox->setSingleStep(0.1);
    connect(bodyWidthSpinBox, SIGNAL(valueChanged(double)), this, SLOT(changeBodyWidth(double)));
    layout->addWidget(bodyWidthSpinBox, row++, 1, Qt::AlignLeft);

    layout->addWidget(new QLabel(QStringLiteral("Candlestick caps width:")), row, 0, Qt::AlignLeft);
    QDoubleSpinBox *capsWidthSpinBox = new QDoubleSpinBox();
    capsWidthSpinBox->setRange(-1.0, 2.0);
    capsWidthSpinBox->setValue(m_capsWidth);
    capsWidthSpinBox->setSingleStep(0.1);
    connect(capsWidthSpinBox, SIGNAL(valueChanged(double)), this, SLOT(changeCapsWidth(double)));
    layout->addWidget(capsWidthSpinBox, row++, 1, Qt::AlignLeft);

    QCheckBox *increasingColor = new QCheckBox(QStringLiteral("Custom increasing color (only S1)"));
    connect(increasingColor, SIGNAL(toggled(bool)), this, SLOT(customIncreasingColorToggled(bool)));
    increasingColor->setChecked(m_customIncreasingColor);
    layout->addWidget(increasingColor, row++, 0, 1, 2, Qt::AlignLeft);

    QCheckBox *decreasingColor = new QCheckBox(QStringLiteral("Custom decreasing color (only S1)"));
    connect(decreasingColor, SIGNAL(toggled(bool)), this, SLOT(customDecreasingColorToggled(bool)));
    decreasingColor->setChecked(m_customDecreasingColor);
    layout->addWidget(decreasingColor, row++, 0, 1, 2, Qt::AlignLeft);

    return layout;
}

QGridLayout *MainWidget::createMiscellaneousControlsLayout()
{
    QGridLayout *layout = new QGridLayout();
    int row = 0;

    layout->addWidget(new QLabel(QStringLiteral("Miscellaneous:")), row, 0, Qt::AlignLeft);

    QCheckBox *antialiasingCheckBox = new QCheckBox(QStringLiteral("Antialiasing"));
    connect(antialiasingCheckBox, SIGNAL(toggled(bool)), this, SLOT(antialiasingToggled(bool)));
    antialiasingCheckBox->setChecked(false);
    layout->addWidget(antialiasingCheckBox, row++, 1, Qt::AlignLeft);

    QCheckBox *animationCheckBox = new QCheckBox(QStringLiteral("Animation"));
    connect(animationCheckBox, SIGNAL(toggled(bool)), this, SLOT(animationToggled(bool)));
    animationCheckBox->setChecked(false);
    layout->addWidget(animationCheckBox, row++, 1, Qt::AlignLeft);

    QCheckBox *legendCheckBox = new QCheckBox(QStringLiteral("Legend"));
    connect(legendCheckBox, SIGNAL(toggled(bool)), this, SLOT(legendToggled(bool)));
    legendCheckBox->setChecked(true);
    layout->addWidget(legendCheckBox, row++, 1, Qt::AlignLeft);

    QCheckBox *titleCheckBox = new QCheckBox(QStringLiteral("Title"));
    connect(titleCheckBox, SIGNAL(toggled(bool)), this, SLOT(titleToggled(bool)));
    titleCheckBox->setChecked(true);
    layout->addWidget(titleCheckBox, row++, 1, Qt::AlignLeft);

    layout->addWidget(new QLabel(QStringLiteral("Chart theme:")), row, 0, Qt::AlignLeft);
    QComboBox *chartThemeComboBox = new QComboBox();
    chartThemeComboBox->addItem(QStringLiteral("Light"));
    chartThemeComboBox->addItem(QStringLiteral("Blue Cerulean"));
    chartThemeComboBox->addItem(QStringLiteral("Dark"));
    chartThemeComboBox->addItem(QStringLiteral("Brown Sand"));
    chartThemeComboBox->addItem(QStringLiteral("Blue Ncs"));
    chartThemeComboBox->addItem(QStringLiteral("High Contrast"));
    chartThemeComboBox->addItem(QStringLiteral("Blue Icy"));
    chartThemeComboBox->addItem(QStringLiteral("Qt"));
    connect(chartThemeComboBox,SIGNAL(currentIndexChanged(int)),this,SLOT(changeChartTheme(int)));
    layout->addWidget(chartThemeComboBox, row++, 1, Qt::AlignLeft);

    layout->addWidget(new QLabel(QStringLiteral("Axis X:")), row, 0, Qt::AlignLeft);
    QComboBox *axisXComboBox = new QComboBox();
    axisXComboBox->addItem(QStringLiteral("BarCategory"));
    axisXComboBox->addItem(QStringLiteral("DateTime"));
    axisXComboBox->addItem(QStringLiteral("Value"));
    connect(axisXComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeAxisX(int)));
    layout->addWidget(axisXComboBox, row++, 1, Qt::AlignLeft);

    return layout;
}

QGridLayout *MainWidget::createModelMapperControlsLayout()
{
    QGridLayout *layout = new QGridLayout();
    int row = 0;

    layout->addWidget(new QLabel(QStringLiteral("First series:")), row, 0, Qt::AlignLeft);

    QPushButton *attachModelMapperButton = new QPushButton(QStringLiteral("Attach model mapper"));
    connect(attachModelMapperButton, SIGNAL(clicked(bool)), this, SLOT(attachModelMapper()));
    layout->addWidget(attachModelMapperButton, row++, 1, Qt::AlignLeft);

    QPushButton *detachModelMappeButton = new QPushButton(QStringLiteral("Detach model mapper"));
    connect(detachModelMappeButton, SIGNAL(clicked(bool)), this, SLOT(detachModelMapper()));
    layout->addWidget(detachModelMappeButton, row++, 1, Qt::AlignLeft);

    QTableView *tableView = new QTableView();
    tableView->setMinimumSize(320, 480);
    tableView->setMaximumSize(320, 480);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    Q_ASSERT_X(m_hModelMapper->model(), Q_FUNC_INFO, "Model is not initialized");
    tableView->setModel(m_hModelMapper->model());
    layout->addWidget(tableView, row++, 0, 1, 2, Qt::AlignLeft);

    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding), row++, 0);

    return layout;
}

qreal MainWidget::randomValue(int min, int max) const
{
    return (qrand() / (qreal(RAND_MAX) + 1)) * ((qMax(min, max) - qMin(min, max)) + qMin(min, max));
}

QCandlestickSet *MainWidget::randomSet(qreal timestamp)
{
    QCandlestickSet *set = new QCandlestickSet(timestamp);
    set->setOpen(randomValue(4, 11));
    set->setHigh(randomValue(12, 15));
    set->setLow(randomValue(0, 3));
    set->setClose(randomValue(4, 11));

    return set;
}

void MainWidget::updateAxes()
{
    if (m_chart->axes().isEmpty())
        m_chart->createDefaultAxes();

    QCandlestickSeries *series;
    if (!m_chart->series().isEmpty())
        series = qobject_cast<QCandlestickSeries *>(m_chart->series().at(0));
    else
        series = nullptr;

    m_axisX = m_chart->axes(Qt::Horizontal).first();
    if (series && !series->sets().isEmpty()) {
        if (m_axisX->type() == QAbstractAxis::AxisTypeBarCategory) {
            QBarCategoryAxis *axisX = qobject_cast<QBarCategoryAxis *>(m_axisX);
            QStringList categories;
            for (int i = 0; i < series->sets().count(); ++i)
                categories.append(QString::number(i));
            axisX->setCategories(categories);
        } else { // QAbstractAxis::AxisTypeDateTime || QAbstractAxis::AxisTypeValue
            qreal msInMonth = 31.0 * 24.0 * 60.0 * 60.0 * 1000.0;
            qreal min = series->sets().first()->timestamp() - msInMonth;
            qreal max = series->sets().last()->timestamp() + msInMonth;
            QDateTime minDateTime = QDateTime::fromMSecsSinceEpoch(min);
            QDateTime maxDateTime = QDateTime::fromMSecsSinceEpoch(max);

            if (m_axisX->type() == QAbstractAxis::AxisTypeDateTime)
                m_axisX->setRange(minDateTime, maxDateTime);
            else
                m_axisX->setRange(min, max);
        }
    }

    m_axisY = m_chart->axes(Qt::Vertical).first();
    m_axisY->setMax(15);
    m_axisY->setMin(0);
}

void MainWidget::addSeries()
{
    if (m_chart->series().count() > 9) {
        qDebug() << "Maximum series count is 10";
        return;
    }

    QCandlestickSeries *series = new QCandlestickSeries();
    series->setName(QStringLiteral("S%1").arg(m_chart->series().count() + 1));
    series->setMaximumColumnWidth(m_maximumColumnWidth);
    series->setMinimumColumnWidth(m_minimumColumnWidth);
    series->setBodyOutlineVisible(m_bodyOutlineVisible);
    series->setBodyWidth(m_bodyWidth);
    series->setCapsVisible(m_capsVisible);
    series->setCapsWidth(m_capsWidth);

    if (m_chart->series().isEmpty()) {
        if (m_customIncreasingColor)
            series->setIncreasingColor(QColor(Qt::green));
        if (m_customDecreasingColor)
            series->setDecreasingColor(QColor(Qt::red));

        for (int month = 1; month <= 12; ++month) {
            QDateTime dateTime;
            dateTime.setDate(QDate(QDateTime::currentDateTime().date().year(), month, 1));
            dateTime.setTime(QTime(12, 34, 56, 789));

            QCandlestickSet *set = randomSet(dateTime.toMSecsSinceEpoch());
            series->append(set);
        }
    } else {
        QCandlestickSeries *s = qobject_cast<QCandlestickSeries *>(m_chart->series().at(0));
        for (int i = 0; i < s->sets().count(); ++i) {
            QCandlestickSet *set = randomSet(s->sets().at(i)->timestamp());
            series->append(set);
        }
    }

    m_chart->addSeries(series);

    updateAxes();
    if (!series->attachedAxes().contains(m_axisX))
        series->attachAxis(m_axisX);
    if (!series->attachedAxes().contains(m_axisY))
        series->attachAxis(m_axisY);
}

void MainWidget::removeSeries()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    if (m_chart->series().count() == 1)
        detachModelMapper();

    QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(m_chart->series().last());
    m_chart->removeSeries(series);
    delete series;
    series = nullptr;
}

void MainWidget::removeAllSeries()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    detachModelMapper();

    m_chart->removeAllSeries();
}

void MainWidget::addSet()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);

        QDateTime dateTime;
        if (series->count()) {
            dateTime.setMSecsSinceEpoch(series->sets().last()->timestamp());
            dateTime = dateTime.addMonths(1);
        } else {
            dateTime.setDate(QDate(QDateTime::currentDateTime().date().year(), 1, 1));
            dateTime.setTime(QTime(12, 34, 56, 789));
        }

        QCandlestickSet *set = randomSet(dateTime.toMSecsSinceEpoch());
        series->append(set);
    }

    updateAxes();
}

void MainWidget::insertSet()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);

        QDateTime dateTime;
        if (series->count()) {
            dateTime.setMSecsSinceEpoch(series->sets().first()->timestamp());
            dateTime = dateTime.addMonths(-1);
        } else {
            dateTime.setDate(QDate(QDateTime::currentDateTime().date().year(), 1, 1));
            dateTime.setTime(QTime(12, 34, 56, 789));
        }

        QCandlestickSet *set = randomSet(dateTime.toMSecsSinceEpoch());
        series->insert(0, set);
    }

    updateAxes();
}

void MainWidget::removeSet()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        if (series->sets().isEmpty())
            qDebug() << "Create a set first";
        else
            series->remove(series->sets().last());
    }

    updateAxes();
}

void MainWidget::removeAllSets()
{
    if (m_chart->series().isEmpty()) {
        qDebug() << "Create a series first";
        return;
    }

    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        if (series->sets().isEmpty())
            qDebug() << "Create a set first";
        else
            series->clear();
    }

    updateAxes();
}

void MainWidget::changeMaximumColumnWidth(double width)
{
    m_maximumColumnWidth = width;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setMaximumColumnWidth(m_maximumColumnWidth);
    }
}

void MainWidget::changeMinimumColumnWidth(double width)
{
    m_minimumColumnWidth = width;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setMinimumColumnWidth(m_minimumColumnWidth);
    }
}

void MainWidget::bodyOutlineVisibleToggled(bool visible)
{
    m_bodyOutlineVisible = visible;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setBodyOutlineVisible(m_bodyOutlineVisible);
    }
}

void MainWidget::capsVisibleToggled(bool visible)
{
    m_capsVisible = visible;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setCapsVisible(m_capsVisible);
    }
}

void MainWidget::changeBodyWidth(double width)
{
    m_bodyWidth = width;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setBodyWidth(m_bodyWidth);
    }
}

void MainWidget::changeCapsWidth(double width)
{
    m_capsWidth = width;
    foreach (QAbstractSeries *s, m_chart->series()) {
        QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(s);
        series->setCapsWidth(m_capsWidth);
    }
}

void MainWidget::customIncreasingColorToggled(bool custom)
{
    m_customIncreasingColor = custom;

    if (m_chart->series().isEmpty())
        return;

    QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(m_chart->series().at(0));
    if (series) {
        QColor color = m_customIncreasingColor ? QColor(Qt::green) : QColor();
        series->setIncreasingColor(color);
    }
}

void MainWidget::customDecreasingColorToggled(bool custom)
{
    m_customDecreasingColor = custom;

    if (m_chart->series().isEmpty())
        return;

    QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(m_chart->series().at(0));
    if (series) {
        QColor color = m_customDecreasingColor ? QColor(Qt::red) : QColor();
        series->setDecreasingColor(color);
    }
}

void MainWidget::antialiasingToggled(bool enabled)
{
    m_chartView->setRenderHint(QPainter::Antialiasing, enabled);
}

void MainWidget::animationToggled(bool enabled)
{
    if (enabled)
        m_chart->setAnimationOptions(QChart::SeriesAnimations);
    else
        m_chart->setAnimationOptions(QChart::NoAnimation);
}

void MainWidget::legendToggled(bool visible)
{
    m_chart->legend()->setVisible(visible);
    if (visible)
        m_chart->legend()->setAlignment(Qt::AlignBottom);
}

void MainWidget::titleToggled(bool visible)
{
    if (visible)
        m_chart->setTitle(QStringLiteral("Candlestick Chart"));
    else
        m_chart->setTitle(QString());
}

void MainWidget::changeChartTheme(int themeIndex)
{
    if (themeIndex < QChart::ChartThemeLight || themeIndex > QChart::ChartThemeQt) {
        qDebug() << "Invalid chart theme index:" << themeIndex;
        return;
    }

    m_chart->setTheme((QChart::ChartTheme)(themeIndex));
}

void MainWidget::changeAxisX(int axisXIndex)
{
    if (m_axisX) {
        m_chart->removeAxis(m_axisX);
        delete m_axisX;
    }

    switch (axisXIndex) {
    case 0:
        m_axisX = new QBarCategoryAxis();
        break;
    case 1:
        m_axisX = new QDateTimeAxis();
        break;
    case 2:
        m_axisX = new QValueAxis();
        break;
    default:
        qDebug() << "Invalid axis x index:" << axisXIndex;
        return;
    }

    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    updateAxes();

    foreach (QAbstractSeries *series, m_chart->series())
        series->attachAxis(m_axisX);
}

void MainWidget::attachModelMapper()
{
    if (m_hModelMapper->series()) {
        qDebug() << "Model mapper is already attached";
        return;
    }

    if (m_chart->series().isEmpty())
        addSeries();

    QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(m_chart->series().at(0));
    Q_ASSERT(series);
    series->setName(QStringLiteral("SWMM")); // Series With Model Mapper

    CustomTableModel *model = qobject_cast<CustomTableModel *>(m_hModelMapper->model());
    foreach (QCandlestickSet *set, series->sets())
        model->addRow(set);

    m_hModelMapper->setFirstSetRow(0);
    m_hModelMapper->setLastSetRow(model->rowCount() - 1);
    m_hModelMapper->setSeries(series);
}

void MainWidget::detachModelMapper()
{
    if (!m_hModelMapper->series())
        return;

    QCandlestickSeries *series = qobject_cast<QCandlestickSeries *>(m_hModelMapper->series());
    Q_ASSERT(series);
    series->setName(QStringLiteral("S1"));

    m_hModelMapper->setSeries(nullptr);
    m_hModelMapper->setFirstSetRow(-1);
    m_hModelMapper->setLastSetRow(-1);

    CustomTableModel *model = qobject_cast<CustomTableModel *>(m_hModelMapper->model());
    model->clearRows();
}
