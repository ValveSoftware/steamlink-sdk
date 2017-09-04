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

#include "engine.h"
#include <QtCore/QItemSelectionModel>
#include <QtGui/QStandardItemModel>
#include <QtCharts/QVXYModelMapper>
#include <QtCharts/QHBarModelMapper>
#include <QtCharts/QVPieModelMapper>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>
#include <QtCharts/QBarSet>


const qint32 MAGIC_NUMBER = 0x66666666;

Engine::Engine(QObject* parent) :
    QObject(parent), m_count(10), m_chart(new QChart()), m_model(0), m_selection(0)
{
    createModels();
}

Engine::~Engine()
{
    delete m_chart;
    delete m_selection;
    delete m_model;
}

void Engine::createModels()
{
    m_model = new QStandardItemModel(m_count, m_count);
    m_model->setHorizontalHeaderLabels(
        QStringList() << "A" << "B" << "C" << "D" << "E" << "F" << "G" << "H" << "I" << "J");
    m_model->setVerticalHeaderLabels(
        QStringList() << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9" << "10");
    m_selection = new QItemSelectionModel(m_model);

}

QList<QAbstractSeries*> Engine::addSeries(QAbstractSeries::SeriesType type)
{
    const QModelIndexList& list = m_selection->selectedIndexes();

    QMap<int, QModelIndex> columns;

    foreach (const QModelIndex& index, list) {
        columns.insertMulti(index.column(), index);
    }

    QList<int> keys = columns.uniqueKeys();

    QModelIndexList rows = columns.values(keys.first());

    int minRow = m_count + 1;
    int maxRow = -1;

    foreach (const QModelIndex& index, rows) {
        minRow = qMin(index.row(), minRow);
        maxRow = qMax(index.row(), maxRow);
    }

    QList<QAbstractSeries*> result;
    QColor color;

    switch (type) {

    case QAbstractSeries::SeriesTypeLine:
    {
        for (int i = 1; i < keys.count(); ++i) {
            QLineSeries *line = new QLineSeries();
            setupXYSeries(line, keys, i, minRow, maxRow);
            result << line;
        }
        break;
    }
    case QAbstractSeries::SeriesTypeSpline:
    {
        for (int i = 1; i < keys.count(); ++i) {
            QSplineSeries *line = new QSplineSeries();
            setupXYSeries(line, keys, i, minRow, maxRow);
            result << line;
        }
        break;
    }
    case QAbstractSeries::SeriesTypeScatter:
    {
        for (int i = 1; i < keys.count(); ++i) {
            QScatterSeries *line = new QScatterSeries();
            setupXYSeries(line, keys, i, minRow, maxRow);
            result << line;
        }
        break;
    }
    case QAbstractSeries::SeriesTypeBar:
    {
        QBarSeries *bar = new QBarSeries();
        setupBarSeries(bar,keys,minRow,maxRow);
        result << bar;
        break;
    }
    case QAbstractSeries::SeriesTypePercentBar:
    {
        QPercentBarSeries *bar = new QPercentBarSeries();
        setupBarSeries(bar,keys,minRow,maxRow);
        result << bar;
        break;
    }
    case QAbstractSeries::SeriesTypeStackedBar:
    {
        QStackedBarSeries *bar = new QStackedBarSeries();
        setupBarSeries(bar,keys,minRow,maxRow);
        result << bar;
        break;
    }
    case QAbstractSeries::SeriesTypePie:
    {

        QPieSeries *pie = new QPieSeries();
        setupPieSeries(pie,keys,minRow,maxRow);
        result << pie;
        break;
    }
    case QAbstractSeries::SeriesTypeArea:
    {
         QAreaSeries *area = new QAreaSeries( new QLineSeries(), new QLineSeries());
         setupAreaSeries(area,keys,minRow,maxRow);
         result << area;
         break;
    }
    }

    m_chart->createDefaultAxes();
    return result;
}

void Engine::removeSeries(QAbstractSeries* series)
{
    m_chart->removeSeries(series);

    foreach (const QModelIndex& index, m_seriesModelIndex.value(series)) {
        m_model->setData(index, QColor(Qt::white), Qt::BackgroundRole);
    }
}

void Engine::clearModels()
{
    delete m_selection;
    m_selection = 0;
    delete m_model;
    m_model = 0;
    createModels();
}

bool Engine::save(const QString &filename) const
{
    if (filename.isEmpty())
        return false;

    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QDataStream out(&file);
    out << MAGIC_NUMBER;
    out.setVersion(QDataStream::Qt_4_8);
    out << m_model->rowCount();
    out << m_model->columnCount();

    for (int row = 0; row < m_model->rowCount(); ++row) {
        for (int column = 0; column < m_model->columnCount(); ++column) {
            QStandardItem *item = m_model->item(row, column);
            if (item) {
                    out << row;
                    out << column;
                    out << item->data(Qt::EditRole).toString();
            }
        }
    }
    return true;
}

bool Engine::load(const QString &filename)
{
    clearModels();

    if (filename.isEmpty())
        return false;

    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream in(&file);

    qint32 magicNumber;
    in >> magicNumber;

    if (magicNumber != MAGIC_NUMBER)
        return false;

    in.setVersion(QDataStream::Qt_4_8);

    int rowCount;
    in >> rowCount;

    int columnCount;
    in >> columnCount;

    while (!in.atEnd()) {
        int row;
        int column;
        QString value;
        in >> row >> column >> value;
        QStandardItem *item = new QStandardItem();
        bool ok;
        double result = value.toDouble(&ok);
        if(ok)
            item->setData(result, Qt::EditRole);
        else
            item->setData(value, Qt::EditRole);
        m_model->setItem(row, column, item);
    }

    return true;
}

void Engine::setupXYSeries(QXYSeries *xyseries, const QList<int>& columns, int column, int minRow, int maxRow)
{
    QVXYModelMapper* mapper = new QVXYModelMapper(xyseries);
    mapper->setSeries(xyseries);
    mapper->setModel(m_model);
    mapper->setXColumn(columns.first());
    mapper->setYColumn(columns.at(column));
    mapper->setFirstRow(minRow);
    mapper->setRowCount(maxRow - minRow + 1);
    m_chart->addSeries(xyseries);
    xyseries->setName(QString("Series %1").arg(m_chart->series().count()));
    QObject::connect(xyseries,SIGNAL(clicked(QPointF)),this,SIGNAL(selected()));
    const QModelIndexList& list = m_selection->selectedIndexes();
    QModelIndexList result;
    foreach (const QModelIndex& index, list) {
        if (index.column() ==columns.at(column)){
            m_model->setData(index, xyseries->pen().color(), Qt::BackgroundRole);
            result << index;
        }
    }
    m_seriesModelIndex.insert(xyseries,result);
}

void Engine::setupBarSeries(QAbstractBarSeries *bar, const QList<int>& columns, int minRow, int maxRow)
{
    QHBarModelMapper* mapper = new QHBarModelMapper(bar);
    mapper->setSeries(bar);
    mapper->setModel(m_model);
    mapper->setFirstColumn(minRow);
    mapper->setColumnCount(maxRow - minRow + 1);
    mapper->setFirstBarSetRow(columns.at(1));
    mapper->setLastBarSetRow(columns.last());
    m_chart->addSeries(bar);
    bar->setName(QString("Series %1").arg(m_chart->series().count()));

    const QModelIndexList& list = m_selection->selectedIndexes();
    foreach (const QModelIndex& index, list) {
        if (index.column() >= columns.at(1) && index.column()<= columns.last()) {
            //m_model->setData(index, bar->barSets().at(index.column())->brush().color(), Qt::BackgroundRole);
        }
    }
}

void Engine::setupPieSeries(QPieSeries *pie, const QList<int>& columns, int minRow, int maxRow)
{
    QVPieModelMapper* mapper = new QVPieModelMapper(pie);
    mapper->setSeries(pie);
    mapper->setModel(m_model);
    mapper->setValuesColumn(columns.at(1));
    mapper->setLabelsColumn(columns.first());
    mapper->setFirstRow(minRow);
    mapper->setRowCount(maxRow - minRow + 1);
    m_chart->addSeries(pie);
    pie->setName(QString("Series %1").arg(m_chart->series().count()));

    const QModelIndexList& list = m_selection->selectedIndexes();
    foreach (const QModelIndex& index, list) {
       // m_model->setData(index, bar->barSets()pen().color(), Qt::BackgroundRole);
    }
}

void Engine::setupAreaSeries(QAreaSeries *series, const QList<int>& columns, int minRow, int maxRow)
{
    QVXYModelMapper* umapper = new QVXYModelMapper(series);
    umapper->setSeries(series->upperSeries());
    umapper->setModel(m_model);
    umapper->setXColumn(columns.first());
    umapper->setYColumn(columns.at(1));
    umapper->setFirstRow(minRow);
    umapper->setRowCount(maxRow - minRow + 1);

    QVXYModelMapper* lmapper = new QVXYModelMapper(series);
    lmapper->setSeries(series->lowerSeries());
    lmapper->setModel(m_model);
    lmapper->setXColumn(columns.first());
    lmapper->setYColumn(columns.at(2));
    lmapper->setFirstRow(minRow);
    lmapper->setRowCount(maxRow - minRow + 1);

    m_chart->addSeries(series);
    series->setName(QString("Series %1").arg(m_chart->series().count()));

    const QModelIndexList& list = m_selection->selectedIndexes();
    foreach (const QModelIndex& index, list) {
        //if (index.column() ==columns.at(column))
          //  m_model->setData(index, xyseries->pen().color(), Qt::BackgroundRole);
    }
}
