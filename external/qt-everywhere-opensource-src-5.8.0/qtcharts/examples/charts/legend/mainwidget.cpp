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
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtCore/QDebug>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QLegend>
#include <QtWidgets/QFormLayout>

QT_CHARTS_USE_NAMESPACE

MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent)
{
    // Create buttons for ui
    m_buttonLayout = new QGridLayout();
    QPushButton *detachLegendButton = new QPushButton("Toggle attached");
    connect(detachLegendButton, SIGNAL(clicked()), this, SLOT(toggleAttached()));
    m_buttonLayout->addWidget(detachLegendButton, 0, 0);

    QPushButton *addSetButton = new QPushButton("add barset");
    connect(addSetButton, SIGNAL(clicked()), this, SLOT(addBarset()));
    m_buttonLayout->addWidget(addSetButton, 2, 0);
    QPushButton *removeBarsetButton = new QPushButton("remove barset");
    connect(removeBarsetButton, SIGNAL(clicked()), this, SLOT(removeBarset()));
    m_buttonLayout->addWidget(removeBarsetButton, 3, 0);

    QPushButton *alignButton = new QPushButton("Align (Bottom)");
    connect(alignButton, SIGNAL(clicked()), this, SLOT(setLegendAlignment()));
    m_buttonLayout->addWidget(alignButton, 4, 0);

    QPushButton *boldButton = new QPushButton("Toggle bold");
    connect(boldButton, SIGNAL(clicked()), this, SLOT(toggleBold()));
    m_buttonLayout->addWidget(boldButton, 8, 0);

    QPushButton *italicButton = new QPushButton("Toggle italic");
    connect(italicButton, SIGNAL(clicked()), this, SLOT(toggleItalic()));
    m_buttonLayout->addWidget(italicButton, 9, 0);

    m_legendPosX = new QDoubleSpinBox();
    m_legendPosY = new QDoubleSpinBox();
    m_legendWidth = new QDoubleSpinBox();
    m_legendHeight = new QDoubleSpinBox();

    connect(m_legendPosX, SIGNAL(valueChanged(double)), this, SLOT(updateLegendLayout()));
    connect(m_legendPosY, SIGNAL(valueChanged(double)), this, SLOT(updateLegendLayout()));
    connect(m_legendWidth, SIGNAL(valueChanged(double)), this, SLOT(updateLegendLayout()));
    connect(m_legendHeight, SIGNAL(valueChanged(double)), this, SLOT(updateLegendLayout()));

    QFormLayout *legendLayout = new QFormLayout();
    legendLayout->addRow("HPos", m_legendPosX);
    legendLayout->addRow("VPos", m_legendPosY);
    legendLayout->addRow("Width", m_legendWidth);
    legendLayout->addRow("Height", m_legendHeight);
    m_legendSettings = new QGroupBox("Detached legend");
    m_legendSettings->setLayout(legendLayout);
    m_buttonLayout->addWidget(m_legendSettings);
    m_legendSettings->setVisible(false);

    // Create chart view with the chart
    m_chart = new QChart();
    m_chartView = new QChartView(m_chart, this);

    // Create spinbox to modify font size
    m_fontSize = new QDoubleSpinBox();
    m_fontSize->setValue(m_chart->legend()->font().pointSizeF());
    connect(m_fontSize, SIGNAL(valueChanged(double)), this, SLOT(fontSizeChanged()));

    QFormLayout *fontLayout = new QFormLayout();
    fontLayout->addRow("Legend font size", m_fontSize);

    // Create layout for grid and detached legend
    m_mainLayout = new QGridLayout();
    m_mainLayout->addLayout(m_buttonLayout, 0, 0);
    m_mainLayout->addLayout(fontLayout, 1, 0);
    m_mainLayout->addWidget(m_chartView, 0, 1, 3, 1);
    setLayout(m_mainLayout);

    createSeries();
}

void MainWidget::createSeries()
{
    m_series = new QBarSeries();
    addBarset();
    addBarset();
    addBarset();
    addBarset();

    m_chart->addSeries(m_series);
    m_chart->setTitle("Legend detach example");
    m_chart->createDefaultAxes();
//![1]
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
//![1]

    m_chartView->setRenderHint(QPainter::Antialiasing);
}

void MainWidget::showLegendSpinbox()
{
    m_legendSettings->setVisible(true);
    QRectF chartViewRect = m_chartView->rect();

    m_legendPosX->setMinimum(0);
    m_legendPosX->setMaximum(chartViewRect.width());
    m_legendPosX->setValue(150);

    m_legendPosY->setMinimum(0);
    m_legendPosY->setMaximum(chartViewRect.height());
    m_legendPosY->setValue(150);

    m_legendWidth->setMinimum(0);
    m_legendWidth->setMaximum(chartViewRect.width());
    m_legendWidth->setValue(150);

    m_legendHeight->setMinimum(0);
    m_legendHeight->setMaximum(chartViewRect.height());
    m_legendHeight->setValue(75);
}

void MainWidget::hideLegendSpinbox()
{
    m_legendSettings->setVisible(false);
}


void MainWidget::toggleAttached()
{
    QLegend *legend = m_chart->legend();
    if (legend->isAttachedToChart()) {
        //![2]
        legend->detachFromChart();
        m_chart->legend()->setBackgroundVisible(true);
        m_chart->legend()->setBrush(QBrush(QColor(128, 128, 128, 128)));
        m_chart->legend()->setPen(QPen(QColor(192, 192, 192, 192)));
        //![2]
        showLegendSpinbox();
        updateLegendLayout();
    } else {
        //![3]
        legend->attachToChart();
        legend->setBackgroundVisible(false);
        //![3]
        hideLegendSpinbox();
    }
    update();
}

void MainWidget::addBarset()
{
    QBarSet *barSet = new QBarSet(QString("set ") + QString::number(m_series->count()));
    qreal delta = m_series->count() * 0.1;
    *barSet << 1 + delta << 2 + delta << 3 + delta << 4 + delta;
    m_series->append(barSet);
}

void MainWidget::removeBarset()
{
    QList<QBarSet *> sets = m_series->barSets();
    if (sets.count() > 0) {
        m_series->remove(sets.at(sets.count() - 1));
    }
}

void MainWidget::setLegendAlignment()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());

    switch (m_chart->legend()->alignment()) {
    case Qt::AlignTop:
        m_chart->legend()->setAlignment(Qt::AlignLeft);
        if (button)
            button->setText("Align (Left)");
        break;
    case Qt::AlignLeft:
        m_chart->legend()->setAlignment(Qt::AlignBottom);
        if (button)
            button->setText("Align (Bottom)");
        break;
    case Qt::AlignBottom:
        m_chart->legend()->setAlignment(Qt::AlignRight);
        if (button)
            button->setText("Align (Right)");
        break;
    default:
        if (button)
            button->setText("Align (Top)");
        m_chart->legend()->setAlignment(Qt::AlignTop);
        break;
    }
}

void MainWidget::toggleBold()
{
    QFont font = m_chart->legend()->font();
    font.setBold(!font.bold());
    m_chart->legend()->setFont(font);
}

void MainWidget::toggleItalic()
{
    QFont font = m_chart->legend()->font();
    font.setItalic(!font.italic());
    m_chart->legend()->setFont(font);
}

void MainWidget::fontSizeChanged()
{
    QFont font = m_chart->legend()->font();
    font.setPointSizeF(m_fontSize->value());
    m_chart->legend()->setFont(font);
}

void MainWidget::updateLegendLayout()
{
//![4]
    m_chart->legend()->setGeometry(QRectF(m_legendPosX->value(),
                                          m_legendPosY->value(),
                                          m_legendWidth->value(),
                                          m_legendHeight->value()));
    m_chart->legend()->update();
//![4]
}
