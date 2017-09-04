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

#include "grid.h"
#include "charts.h"
#include <QtCharts/QChart>
#include <QtWidgets/QGraphicsGridLayout>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtCore/QDebug>

Grid::Grid(int size, QGraphicsItem *parent)
    : QGraphicsWidget(parent),
      m_listCount(3),
      m_valueMax(10),
      m_valueCount(7),
      m_size(size),
      m_dataTable(Model::generateRandomData(m_listCount, m_valueMax, m_valueCount)),
      m_state(NoState),
      m_currentState(NoState),
      m_rubberBand(new QGraphicsRectItem()),
      m_gridLayout(new QGraphicsGridLayout())
{
    setLayout(m_gridLayout);
    m_rubberBand->setParentItem(this);
    m_rubberBand->setVisible(false);
    m_rubberBand->setZValue(2);
}

Grid::~Grid()
{

}

void Grid::createCharts(const QString &category)
{
    clear();

    QChart *qchart(0);
    Charts::ChartList list = Charts::chartList();

    if (category.isEmpty()) {
        for (int i = 0; i < m_size * m_size; ++i) {
            QChart *chart = new QChart();
            chart->setTitle(QObject::tr("Empty"));
            m_gridLayout->addItem(chart, i / m_size, i % m_size);
            m_chartHash[chart] = i;
            m_chartHashRev[i] = chart;
        }
    } else {
        int j = 0;
        for (int i = 0; i < list.size(); ++i) {
            Chart *chart = list.at(i);
            if (chart->category() == category && j < m_size * m_size) {
                qchart = list.at(i)->createChart(m_dataTable);
                m_gridLayout->addItem(qchart, j / m_size, j % m_size);
                m_chartHash[qchart] = j;
                m_chartHashRev[j] = qchart;
                j++;
            }
        }
        for (; j < m_size * m_size; ++j) {
            qchart = new QChart();
            qchart->setTitle(QObject::tr("Empty"));
            m_gridLayout->addItem(qchart, j / m_size, j % m_size);
            m_chartHash[qchart] = j;
            m_chartHashRev[j] = qchart;
        }
    }
    m_gridLayout->activate();
}

void Grid::createCharts(const QString &category, const QString &subcategory, const QString &name)
{
    clear();

    QChart *qchart(0);
    Charts::ChartList list = Charts::chartList();
    Chart *chart;

    //find chart
    for (int i = 0; i < list.size(); ++i) {

        chart = list.at(i);
        if (chart->category() == category &&
            chart->subCategory() == subcategory &&
            chart->name() == name) {
            break;
        }
        chart = 0;
    }

    //create charts
    for (int j = 0; j < m_size * m_size; ++j) {

        if(!chart){
            qchart = new QChart();
        }else{
            qchart = chart->createChart(m_dataTable);
        }
        qchart->setTitle(QObject::tr("Empty"));
        m_gridLayout->addItem(qchart, j / m_size, j % m_size);
        m_chartHash[qchart] = j;
        m_chartHashRev[j] = qchart;
    }

    m_gridLayout->activate();
}

void Grid::clear()
{
    int count = m_gridLayout->count();
    for (int i = 0; i < count; ++i)
        m_gridLayout->removeAt(0);

    qDeleteAll(m_chartHash.keys());
    m_chartHash.clear();
    m_chartHashRev.clear();
}

QList<QChart *> Grid::charts()
{
    return m_chartHash.keys();
}

void Grid::setState(State state)
{
    m_state = state;
}

void Grid::setSize(int size)
{
    if (m_size != size) {

        //remove old;
        int count = m_gridLayout->count();
        for (int i = 0; i < count; ++i) {
            m_gridLayout->removeAt(0);
        }


        QChart* qchart = 0;
        int j = 0;

        for (; j < size * size; ++j) {

            qchart = m_chartHashRev[j];

            if (!qchart){
                qchart = new QChart();
                qchart->setTitle(QObject::tr("Empty"));
            }

            m_chartHash[qchart] = j;
            m_chartHashRev[j] = qchart;
            m_gridLayout->addItem(qchart, j / size, j % size);
        }

        //delete rest
        while (j < m_size * m_size) {
            QChart* qchart = m_chartHashRev.take(j);
            delete(qchart);
            m_chartHash.remove(qchart);
            j++;
        }

        m_size = size;
    }
}

void Grid::setRubberPen(const QPen &pen)
{
    m_rubberBand->setPen(pen);
}

void Grid::replaceChart(QChart *oldChart, Chart *newChart)
{
    int index = m_chartHash[oldChart];
    //not in 4.7.2 m_baseLayout->removeItem(qchart);
    for (int i = 0; i < m_gridLayout->count(); ++i) {
        if (m_gridLayout->itemAt(i) == oldChart) {
            m_gridLayout->removeAt(i);
            break;
        }
    }
    m_chartHash.remove(oldChart);
    m_chartHashRev.remove(index);
    QChart *chart = newChart->createChart(m_dataTable);
    m_gridLayout->addItem(chart, index / m_size, index % m_size);
    m_chartHash[chart] = index;
    m_chartHashRev[index] = chart;
    delete oldChart;
}

void Grid::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {

        m_origin = event->pos();
        m_currentState = NoState;

        foreach (QChart *chart, charts()) {
            QRectF geometryRect = chart->geometry();
            QRectF plotArea = chart->plotArea();
            plotArea.translate(geometryRect.topLeft());
            if (plotArea.contains(m_origin)) {
                m_currentState = m_state;
                if (m_currentState == NoState) emit chartSelected(chart);
                break;
            }
        }
        if (m_currentState == ZoomState) {
            m_rubberBand->setRect(QRectF(m_origin, QSize()));
            m_rubberBand->setVisible(true);
        }

        event->accept();
    }

    if (event->button() == Qt::RightButton) {
        m_origin = event->pos();
        m_currentState = m_state;
    }
}

void Grid::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_currentState != NoState) {

        foreach (QChart *chart, charts()) {
            QRectF geometryRect = chart->geometry();
            QRectF plotArea = chart->plotArea();
            plotArea.translate(geometryRect.topLeft());
            if (plotArea.contains(m_origin)) {
                if (m_currentState == ScrollState) {
                    QPointF delta = m_origin - event->pos();
                    chart->scroll(delta.x(), -delta.y());
                }
                if (m_currentState == ZoomState && plotArea.contains(event->pos()))
                    m_rubberBand->setRect(QRectF(m_origin, event->pos()).normalized());
                break;
            }
        }
        if (m_currentState == ScrollState)
            m_origin = event->pos();
        event->accept();
    }
}

void Grid::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_currentState == ZoomState) {
            m_rubberBand->setVisible(false);

            foreach (QChart *chart, charts()) {
                QRectF geometryRect = chart->geometry();
                QRectF plotArea = chart->plotArea();
                plotArea.translate(geometryRect.topLeft());
                if (plotArea.contains(m_origin)) {
                    QRectF rect = m_rubberBand->rect();
                    rect.translate(-geometryRect.topLeft());
                    chart->zoomIn(rect);
                    break;
                }
            }
        }
        m_currentState = NoState;
        event->accept();
    }

    if (event->button() == Qt::RightButton) {
        if (m_currentState == ZoomState) {
            foreach (QChart *chart, charts()) {
                QRectF geometryRect = chart->geometry();
                QRectF plotArea = chart->plotArea();
                plotArea.translate(geometryRect.topLeft());
                if (plotArea.contains(m_origin)) {
                    chart->zoomOut();
                    break;
                }
            }
        }
    }
}
