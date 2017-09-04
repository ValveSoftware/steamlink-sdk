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

#include "chart-widget.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QHorizontalBarSeries>
#include <QtCharts/QHorizontalPercentBarSeries>
#include <QtCharts/QHorizontalStackedBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCore/QTime>
#include <QElapsedTimer>
#include <QDebug>

//#define VALUE_LOGGING 1

const int initialCount = 100000;
const int visibleCount = 20;
const int storeCount = 100000000;
const int interval = 500;

const int initialSeriesCount = 2;
const int maxSeriesCount = 2;
const bool removeSeries = false;
const int extraSeriesFrequency = 7;

const int initialSetCount = 3;
const int maxSetCount = 3;
const bool removeSets = false;
const int extraSetFrequency = 3;

const bool initialLabels = true;
const int labelsTrigger = -1;
const int visibleTrigger = -1;
const int appendFrequency = 1;
const int animationTrigger = -1;
const bool sameNumberOfBars = false;

const bool animation = true;
const int animationDuration = 300;

const bool horizontal = false;
const bool percent = false;
const bool stacked = true;

const bool negativeValues = false;
const bool mixedValues = false;

// Negative indexes are counted from end of the set.
const bool doReplace = false;
const bool doRemove = false;
const bool doInsert = false;
const bool singleReplace = false;
const bool singleRemove = false;
const bool singleInsert = false;
const int removeIndex = -7;
const int replaceIndex = -3;
const int insertIndex = -5;

const bool logarithmic = false;
const bool barCategories = false;

const qreal barWidth = 0.9;

// Note: reverse axes are not fully supported for bars (animation and label positioning break a bit)
const bool reverseBar = false;
const bool reverseValue = false;

static int counter = 1;
static const QString nameTemplate = QStringLiteral("Set %1/%2");

ChartWidget::ChartWidget(QWidget *parent) :
    QWidget(parent),
    m_chart(new QChart()),
    m_chartView(new QChartView(this)),
    m_setCount(initialSetCount),
    m_seriesCount(initialSeriesCount),
    m_extraScroll(0.0)
{
    m_elapsedTimer.start();

    if (logarithmic) {
        QLogValueAxis *logAxis = new QLogValueAxis;
        logAxis->setBase(2);
        m_valueAxis = logAxis;
    } else {
        m_valueAxis = new QValueAxis;
    }

    if (barCategories)
        m_barAxis = new QBarCategoryAxis;
    else
        m_barAxis = new QValueAxis;

    m_barAxis->setReverse(reverseBar);
    m_valueAxis->setReverse(reverseValue);

    for (int i = 0; i < maxSeriesCount; i++) {
        if (horizontal) {
            if (percent)
                m_series.append(new QHorizontalPercentBarSeries);
            else if (stacked)
                m_series.append(new QHorizontalStackedBarSeries);
            else
                m_series.append(new QHorizontalBarSeries);
        } else {
            if (percent)
                m_series.append(new QPercentBarSeries);
            else if (stacked)
                m_series.append(new QStackedBarSeries);
            else
                m_series.append(new QBarSeries);
        }
        QAbstractBarSeries *series =
                qobject_cast<QAbstractBarSeries *>(m_series.at(m_series.size() - 1));
        QString seriesNameTemplate = QStringLiteral("bar %1");
        series->setName(seriesNameTemplate.arg(i));
        series->setLabelsPosition(QAbstractBarSeries::LabelsInsideEnd);
        series->setLabelsVisible(initialLabels);
        series->setBarWidth(barWidth);
    }

    qsrand((uint) QTime::currentTime().msec());

    resize(800, 300);
    m_horizontalLayout = new QHBoxLayout(this);
    m_horizontalLayout->setSpacing(6);
    m_horizontalLayout->setContentsMargins(11, 11, 11, 11);
    m_horizontalLayout->addWidget(m_chartView);

    qDebug() << "UI setup time:"<< m_elapsedTimer.restart();

    m_chartView->setRenderHint(QPainter::Antialiasing);

    createChart();
    qDebug() << "Chart creation time:"<< m_elapsedTimer.restart();

    QObject::connect(&m_timer, SIGNAL(timeout()), this, SLOT(handleTimeout()));

    m_timer.setInterval(interval);
    m_timer.start();
}

ChartWidget::~ChartWidget()
{
}

void ChartWidget::handleTimeout()
{
    qDebug() << "Intervening time:" << m_elapsedTimer.restart();

    qDebug() << "----------" << counter << "----------";

    bool doScroll = false;

    if (counter % appendFrequency == 0) {
        for (int i = 0; i < maxSeriesCount; i++) {
            for (int j = 0; j < maxSetCount; j++) {
                QBarSet *set = m_sets.value(m_series.at(i)).at(j);
                qreal value = 5 * i + (counter % 100) / qreal(j+1);
                if (negativeValues)
                    set->append(-value);
                else if (mixedValues)
                    set->append(counter % 2 ? value : -value);
                else
                    set->append(value);
#ifdef VALUE_LOGGING
                qDebug() << "Appended value:" << set->at(set->count() - 1)
                         << "to series:" << i
                         << "to set:" << j
                         << "at index:" << set->count() - 1;
#endif
                if (set->count() > storeCount)
                    set->remove(0, set->count() - storeCount);
                if (set->count() > visibleCount)
                    doScroll = true;
            }
        }
        qDebug() << "Append time:" << m_elapsedTimer.restart();
    }

    if (doScroll) {
        doScroll = false;
        qreal scrollAmount = horizontal ? m_chart->plotArea().height() / visibleCount
                                        : m_chart->plotArea().width() / visibleCount;
        // Charts can't scroll without any series, so store the required scroll in those cases
        if (m_seriesCount == 0) {
            m_extraScroll += scrollAmount;
        } else {
            if (horizontal)
                m_chart->scroll(0, scrollAmount + m_extraScroll);
            else
                m_chart->scroll(scrollAmount + m_extraScroll, 0);
            m_extraScroll = 0.0;
        }
        qDebug() << "Scroll time:" << m_elapsedTimer.restart();
    }

    if (doRemove || doReplace || doInsert) {
        for (int i = 0; i < maxSeriesCount; i++) {
            for (int j = 0; j < m_setCount; j++) {
                QBarSet *set = m_sets.value(m_series.at(i)).at(j);
                qreal value = ((counter + 20) % 100) / qreal(j + 1);
                if (doReplace && (!singleReplace || j == 0)) {
                    int index = replaceIndex < 0 ? set->count() + replaceIndex : replaceIndex;
                    set->replace(index, value);
                }
                if (doRemove && (!singleRemove || j == 0)) {
                    int index = removeIndex < 0 ? set->count() + removeIndex : removeIndex;
                    set->remove(index, 1);
                }
                if (doInsert && (!singleInsert || j == 0)) {
                    int index = insertIndex < 0 ? set->count() + insertIndex : insertIndex;
                    set->insert(index, value);
                }
            }
        }
        qDebug() << "R/R    time:" << m_elapsedTimer.restart();
    }

    if (counter % extraSeriesFrequency == 0) {
        if (m_seriesCount <= maxSeriesCount) {
            qDebug() << "Adjusting series count, current count:" << m_seriesCount;
            static int seriesCountAdder = 1;
            if (m_seriesCount == maxSeriesCount) {
                if (removeSeries)
                    seriesCountAdder = -1;
                else
                    seriesCountAdder = 0;
            } else if (m_seriesCount == 0) {
                seriesCountAdder = 1;
            }
            if (seriesCountAdder < 0)
                m_chart->removeSeries(m_series.at(m_seriesCount - 1));
            else if (m_seriesCount < maxSeriesCount)
                addSeriesToChart(m_series.at(m_seriesCount));
            m_seriesCount += seriesCountAdder;
        }
    }

    if (counter % extraSetFrequency == 0) {
        if (m_setCount <= maxSetCount) {
            qDebug() << "Adjusting setcount, current count:" << m_setCount;
            static int setCountAdder = 1;
            if (m_setCount == maxSetCount) {
                if (removeSets)
                    setCountAdder = -1;
                else
                    setCountAdder = 0;
            } else if (m_setCount == 0) {
                setCountAdder = 1;
            }
            for (int i = 0; i < maxSeriesCount; i++) {
                if (setCountAdder < 0) {
                    int barCount = m_sets.value(m_series.at(i)).at(m_setCount - 1)->count();
                    m_series.at(i)->remove(m_sets.value(m_series.at(i)).at(m_setCount - 1));
                    // Since remove deletes the set, recreate it in our list
                    QBarSet *set = new QBarSet(nameTemplate.arg(i).arg(m_setCount - 1));
                    m_sets[m_series.at(i)][m_setCount - 1] = set;
                    set->setLabelBrush(QColor("black"));
                    set->setPen(QPen(QColor("black"), 0.3));
                    QList<qreal> valueList;
                    valueList.reserve(barCount);
                    for (int j = 0; j < barCount; ++j) {
                        qreal value = counter % 100;
                        if (negativeValues)
                            valueList.append(-value);
                        else if (mixedValues)
                            valueList.append(counter % 2 ? value : -value);
                        else
                            valueList.append(value);
                   }
                    set->append(valueList);

                } else if (m_setCount < maxSetCount) {
                    m_series.at(i)->append(m_sets.value(m_series.at(i)).at(m_setCount));
                }
            }
            m_setCount += setCountAdder;
        }
    }

    if (labelsTrigger > 0 && counter % labelsTrigger == 0) {
        m_series.at(0)->setLabelsVisible(!m_series.at(0)->isLabelsVisible());
        qDebug() << "Label visibility changed";
    }

    if (visibleTrigger > 0 && counter % visibleTrigger == 0) {
        m_series.at(0)->setVisible(!m_series.at(0)->isVisible());
        qDebug() << "Series visibility changed";
    }

    if (animationTrigger > 0 && counter % animationTrigger == 0) {
        if (m_chart->animationOptions() == QChart::SeriesAnimations)
            m_chart->setAnimationOptions(QChart::NoAnimation);
        else
            m_chart->setAnimationOptions(QChart::SeriesAnimations);
        qDebug() << "Series animation changed";
    }

    qDebug() << "Rest of time:" << m_elapsedTimer.restart();

    qDebug() << "GraphicsItem Count:" << m_chart->scene()->items().size();
    counter++;
}

void ChartWidget::createChart()
{
    qDebug() << "Initial bar count:" << initialCount;

    QList<qreal> valueList;
    valueList.reserve(initialCount);
    for (int j = 0; j < initialCount; ++j) {
        qreal value = counter++ % 100;
        if (negativeValues)
            valueList.append(-value);
        else if (mixedValues)
            valueList.append(counter % 2 ? value : -value);
        else
            valueList.append(value);
    }

    for (int i = 0; i < maxSeriesCount; i++) {
        for (int j = 0; j < maxSetCount; j++) {
            QBarSet *set = new QBarSet(nameTemplate.arg(i).arg(j));
            m_sets[m_series.at(i)].append(set);
            set->setLabelBrush(QColor("black"));
            set->setPen(QPen(QColor("black"), 0.3));
            if (sameNumberOfBars) {
                set->append(valueList);
            } else {
                QList<qreal> tempList = valueList;
                for (int k = 0; k < j; k++)
                    tempList.removeLast();
                set->append(tempList);
            }
            if (j < m_setCount)
                m_series.at(i)->append(set);
        }
    }
    for (int i = 0; i < initialSeriesCount; i++)
        addSeriesToChart(m_series.at(i));

    m_chart->setTitle("Chart");
    if (animation) {
        m_chart->setAnimationOptions(QChart::SeriesAnimations);
        m_chart->setAnimationDuration(animationDuration);
    }

    if (barCategories) {
        QBarCategoryAxis *barCatAxis = qobject_cast<QBarCategoryAxis *>(m_barAxis);
        QStringList categories;
        const int count = qMax(initialCount, visibleCount);
        for (int i = 0; i < count; i++)
            categories.append(QString::number(i));
        barCatAxis->setCategories(categories);
    } else {
        qobject_cast<QValueAxis *>(m_barAxis)->setTickCount(11);
    }

    if (initialCount > visibleCount)
        m_barAxis->setRange(initialCount - visibleCount, initialCount);
    else
        m_barAxis->setRange(0, visibleCount);

    qreal rangeValue = stacked ? 200.0 : 100.0;
    m_valueAxis->setRange(logarithmic ? 1.0 : (negativeValues || mixedValues) ? -rangeValue : 0.0,
                          (!negativeValues || mixedValues) ? rangeValue : 0.0);

    m_chartView->setChart(m_chart);
}

void ChartWidget::addSeriesToChart(QAbstractBarSeries *series)
{
    qDebug() << "Adding series:" << series->name();

    // HACK: Temporarily take the sets out of the series until axes are set.
    // This is done because added series defaults to a domain that displays all bars, which can
    // get extremely slow as the bar count increases.
    QList<QBarSet *> sets = series->barSets();
    for (auto set : sets)
        series->take(set);

    m_chart->addSeries(series);
    if (horizontal) {
        m_chart->setAxisX(m_valueAxis,series);
        m_chart->setAxisY(m_barAxis, series);
    } else {
        m_chart->setAxisX(m_barAxis, series);
        m_chart->setAxisY(m_valueAxis, series);
    }

    series->append(sets);
}
