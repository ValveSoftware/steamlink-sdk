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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QVBarModelMapper>
#include <QtCharts/QHBarModelMapper>
#include <QtGui/QStandardItemModel>

QT_CHARTS_USE_NAMESPACE

class tst_qbarmodelmapper : public QObject
{
    Q_OBJECT
    
    public:
    tst_qbarmodelmapper();
    void createVerticalMapper();
    void createHorizontalMapper();
    
    private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void verticalMapper_data();
    void verticalMapper();
    void verticalMapperCustomMapping_data();
    void verticalMapperCustomMapping();
    void horizontalMapper_data();
    void horizontalMapper();
    void horizontalMapperCustomMapping_data();
    void horizontalMapperCustomMapping();
    void seriesUpdated();
    void verticalModelInsertRows();
    void verticalModelRemoveRows();
    void verticalModelInsertColumns();
    void verticalModelRemoveColumns();
    void horizontalModelInsertRows();
    void horizontalModelRemoveRows();
    void horizontalModelInsertColumns();
    void horizontalModelRemoveColumns();
    void modelUpdateCell();
    void verticalMapperSignals();
    void horizontalMapperSignals();

    private:
    QStandardItemModel *m_model;
    int m_modelRowCount;
    int m_modelColumnCount;

    QVBarModelMapper *m_vMapper;
    QHBarModelMapper *m_hMapper;

    QBarSeries *m_series;
    QChart *m_chart;
    QChartView *m_chartView;
};

tst_qbarmodelmapper::tst_qbarmodelmapper():
    m_model(0),
    m_modelRowCount(10),
    m_modelColumnCount(8),
    m_vMapper(0),
    m_hMapper(0),
    m_series(0),
    m_chart(0),
    m_chartView(0)
{
}

void tst_qbarmodelmapper::createVerticalMapper()
{
    m_vMapper = new QVBarModelMapper;
    QVERIFY(m_vMapper->model() == 0);
    m_vMapper->setFirstBarSetColumn(0);
    m_vMapper->setLastBarSetColumn(4);
    m_vMapper->setModel(m_model);
    m_vMapper->setSeries(m_series);
}

void tst_qbarmodelmapper::createHorizontalMapper()
{
    m_hMapper = new QHBarModelMapper;
    QVERIFY(m_hMapper->model() == 0);
    m_hMapper->setFirstBarSetRow(0);
    m_hMapper->setLastBarSetRow(4);
    m_hMapper->setModel(m_model);
    m_hMapper->setSeries(m_series);
}

void tst_qbarmodelmapper::init()
{
    m_series = new QBarSeries;
    m_chart->addSeries(m_series);

    m_model = new QStandardItemModel(m_modelRowCount, m_modelColumnCount, this);
    for (int row = 0; row < m_modelRowCount; ++row) {
        for (int column = 0; column < m_modelColumnCount; column++) {
            m_model->setData(m_model->index(row, column), row * column);
        }
    }
}

void tst_qbarmodelmapper::cleanup()
{
    m_chart->removeSeries(m_series);
    delete m_series;
    m_series = 0;

    m_model->clear();
    m_model->deleteLater();
    m_model = 0;

    if (m_vMapper) {
        m_vMapper->deleteLater();
        m_vMapper = 0;
    }

    if (m_hMapper) {
        m_hMapper->deleteLater();
        m_hMapper = 0;
    }
}

void tst_qbarmodelmapper::initTestCase()
{
    m_chart = new QChart;
    m_chartView = new QChartView(m_chart);
    m_chartView->show();
}

void tst_qbarmodelmapper::cleanupTestCase()
{
    delete m_chartView;
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_qbarmodelmapper::verticalMapper_data()
{
    QTest::addColumn<int>("firstBarSetColumn");
    QTest::addColumn<int>("lastBarSetColumn");
    QTest::addColumn<int>("expectedBarSetCount");
    QTest::newRow("lastBarSetColumn greater than firstBarSetColumn") << 0 << 1 << 2;
    QTest::newRow("lastBarSetColumn equal to firstBarSetColumn") << 1 << 1 << 1;
    QTest::newRow("lastBarSetColumn lesser than firstBarSetColumn") << 1 << 0 << 0;
    QTest::newRow("invalid firstBarSetColumn and correct lastBarSetColumn") << -3 << 1 << 0;
    QTest::newRow("firstBarSetColumn beyond the size of model and correct lastBarSetColumn") << m_modelColumnCount << 1 << 0;
    QTest::newRow("firstBarSetColumn beyond the size of model and invalid lastBarSetColumn") << m_modelColumnCount << -1 << 0;
}

void tst_qbarmodelmapper::verticalMapper()
{
    QFETCH(int, firstBarSetColumn);
    QFETCH(int, lastBarSetColumn);
    QFETCH(int, expectedBarSetCount);

    QBarSeries *series = new QBarSeries;

    QVBarModelMapper *mapper = new QVBarModelMapper;
    mapper->setFirstBarSetColumn(firstBarSetColumn);
    mapper->setLastBarSetColumn(lastBarSetColumn);
    mapper->setModel(m_model);
    mapper->setSeries(series);

    m_chart->addSeries(series);

    QCOMPARE(series->count(), expectedBarSetCount);
    QCOMPARE(mapper->firstBarSetColumn(), qMax(-1, firstBarSetColumn));
    QCOMPARE(mapper->lastBarSetColumn(), qMax(-1, lastBarSetColumn));

    delete mapper;
    m_chart->removeSeries(series);
    delete series;
}

void tst_qbarmodelmapper::verticalMapperCustomMapping_data()
{
    QTest::addColumn<int>("first");
    QTest::addColumn<int>("countLimit");
    QTest::addColumn<int>("expectedBarSetCount");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("first: 0, unlimited count") << 0 << -1 << 2 << m_modelRowCount;
    QTest::newRow("first: 3, unlimited count") << 3 << -1 << 2 << m_modelRowCount - 3;
    QTest::newRow("first: 0, count: 5") << 0 << 5 << 2 << qMin(5, m_modelRowCount);
    QTest::newRow("first: 3, count: 5") << 3 << 5 << 2 << qMin(5, m_modelRowCount - 3);
    QTest::newRow("first: +1 greater then the number of rows in the model, unlimited count") << m_modelRowCount + 1 << -1 << 0 << 0;
    QTest::newRow("first: +1 greater then the number of rows in the model, count: 5") << m_modelRowCount + 1 << 5 << 0 << 0;
    QTest::newRow("first: 0, count: +3 greater than the number of rows in the model (should limit to the size of model)") << 0 << m_modelRowCount + 3 << 2 << m_modelRowCount;
    QTest::newRow("first: -3(invalid - should default to 0), unlimited count") << -3 << -1 << 2 << m_modelRowCount;
    QTest::newRow("first: 0, count: -3 (invalid - shlould default to -1)") << 0 << -3 << 2 << m_modelRowCount;
    QTest::newRow("first: -3(invalid - should default to 0), count: -3 (invalid - shlould default to -1)") << -3 << -3 << 2 << m_modelRowCount;
}

void tst_qbarmodelmapper::verticalMapperCustomMapping()
{
    QFETCH(int, first);
    QFETCH(int, countLimit);
    QFETCH(int, expectedBarSetCount);
    QFETCH(int, expectedCount);

    QBarSeries *series = new QBarSeries;

    QCOMPARE(series->count(), 0);

    QVBarModelMapper *mapper = new QVBarModelMapper;
    mapper->setFirstBarSetColumn(0);
    mapper->setLastBarSetColumn(1);
    mapper->setModel(m_model);
    mapper->setSeries(series);
    mapper->setFirstRow(first);
    mapper->setRowCount(countLimit);
    m_chart->addSeries(series);

    QCOMPARE(series->count(), expectedBarSetCount);

    if (expectedBarSetCount > 0)
        QCOMPARE(series->barSets().first()->count(), expectedCount);

    // change values column mapping to invalid
    mapper->setFirstBarSetColumn(-1);
    mapper->setLastBarSetColumn(1);

    QCOMPARE(series->count(), 0);

    delete mapper;
    m_chart->removeSeries(series);
    delete series;
}

void tst_qbarmodelmapper::horizontalMapper_data()
{
    QTest::addColumn<int>("firstBarSetRow");
    QTest::addColumn<int>("lastBarSetRow");
    QTest::addColumn<int>("expectedBarSetCount");
    QTest::newRow("lastBarSetRow greater than firstBarSetRow") << 0 << 1 << 2;
    QTest::newRow("lastBarSetRow equal to firstBarSetRow") << 1 << 1 << 1;
    QTest::newRow("lastBarSetRow lesser than firstBarSetRow") << 1 << 0 << 0;
    QTest::newRow("invalid firstBarSetRow and correct lastBarSetRow") << -3 << 1 << 0;
    QTest::newRow("firstBarSetRow beyond the size of model and correct lastBarSetRow") << m_modelRowCount << 1 << 0;
    QTest::newRow("firstBarSetRow beyond the size of model and invalid lastBarSetRow") << m_modelRowCount << -1 << 0;
}

void tst_qbarmodelmapper::horizontalMapper()
{
    QFETCH(int, firstBarSetRow);
    QFETCH(int, lastBarSetRow);
    QFETCH(int, expectedBarSetCount);

    QBarSeries *series = new QBarSeries;

    QHBarModelMapper *mapper = new QHBarModelMapper;
    mapper->setFirstBarSetRow(firstBarSetRow);
    mapper->setLastBarSetRow(lastBarSetRow);
    mapper->setModel(m_model);
    mapper->setSeries(series);

    m_chart->addSeries(series);

    QCOMPARE(series->count(), expectedBarSetCount);
    QCOMPARE(mapper->firstBarSetRow(), qMax(-1, firstBarSetRow));
    QCOMPARE(mapper->lastBarSetRow(), qMax(-1, lastBarSetRow));

    delete mapper;
    m_chart->removeSeries(series);
    delete series;
}

void tst_qbarmodelmapper::horizontalMapperCustomMapping_data()
{
    QTest::addColumn<int>("first");
    QTest::addColumn<int>("countLimit");
    QTest::addColumn<int>("expectedBarSetCount");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("first: 0, unlimited count") << 0 << -1 << 2 << m_modelColumnCount;
    QTest::newRow("first: 3, unlimited count") << 3 << -1 << 2 << m_modelColumnCount - 3;
    QTest::newRow("first: 0, count: 5") << 0 << 5 << 2 << qMin(5, m_modelColumnCount);
    QTest::newRow("first: 3, count: 5") << 3 << 5 << 2 << qMin(5, m_modelColumnCount - 3);
    QTest::newRow("first: +1 greater then the number of rows in the model, unlimited count") << m_modelColumnCount + 1 << -1 << 0 << 0;
    QTest::newRow("first: +1 greater then the number of rows in the model, count: 5") << m_modelColumnCount + 1 << 5 << 0 << 0;
    QTest::newRow("first: 0, count: +3 greater than the number of rows in the model (should limit to the size of model)") << 0 << m_modelColumnCount + 3 << 2 << m_modelColumnCount;
    QTest::newRow("first: -3(invalid - should default to 0), unlimited count") << -3 << -1 << 2 << m_modelColumnCount;
    QTest::newRow("first: 0, count: -3 (invalid - shlould default to -1)") << 0 << -3 << 2 << m_modelColumnCount;
    QTest::newRow("first: -3(invalid - should default to 0), count: -3 (invalid - shlould default to -1)") << -3 << -3 << 2 << m_modelColumnCount;
}

void tst_qbarmodelmapper::horizontalMapperCustomMapping()
{
    QFETCH(int, first);
    QFETCH(int, countLimit);
    QFETCH(int, expectedBarSetCount);
    QFETCH(int, expectedCount);

    QBarSeries *series = new QBarSeries;

    QCOMPARE(series->count(), 0);

    QHBarModelMapper *mapper = new QHBarModelMapper;
    mapper->setFirstBarSetRow(0);
    mapper->setLastBarSetRow(1);
    mapper->setModel(m_model);
    mapper->setSeries(series);
    mapper->setFirstColumn(first);
    mapper->setColumnCount(countLimit);
    m_chart->addSeries(series);

    QCOMPARE(series->count(), expectedBarSetCount);

    if (expectedBarSetCount > 0)
        QCOMPARE(series->barSets().first()->count(), expectedCount);

    // change values column mapping to invalid
    mapper->setFirstBarSetRow(-1);
    mapper->setLastBarSetRow(1);

    QCOMPARE(series->count(), 0);

    delete mapper;
    m_chart->removeSeries(series);
    delete series;
}

void tst_qbarmodelmapper::seriesUpdated()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
    QCOMPARE(m_vMapper->rowCount(), -1);

    m_series->barSets().first()->append(123);
    QCOMPARE(m_model->rowCount(), m_modelRowCount + 1);
    QCOMPARE(m_vMapper->rowCount(), -1); // the value should not change as it indicates 'all' items there are in the model

    m_series->barSets().last()->remove(0, m_modelRowCount);
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_vMapper->rowCount(), -1); // the value should not change as it indicates 'all' items there are in the model

    m_series->barSets().first()->replace(0, 444.0);
    QCOMPARE(m_model->data(m_model->index(0, 0)).toReal(), 444.0);

    m_series->barSets().first()->setLabel("Hello");
    QCOMPARE(m_model->headerData(0, Qt::Horizontal).toString(), QString("Hello"));

    QList<qreal> newValues;
    newValues << 15 << 27 << 35 << 49;
    m_series->barSets().first()->append(newValues);
    QCOMPARE(m_model->rowCount(), 1 + newValues.count());

    QList<QBarSet* > newBarSets;
    QBarSet* newBarSet_1 = new QBarSet("New_1");
    newBarSet_1->append(101);
    newBarSet_1->append(102);
    newBarSet_1->append(103);
    newBarSets.append(newBarSet_1);

    QBarSet* newBarSet_2 = new QBarSet("New_2");
    newBarSet_2->append(201);
    newBarSet_2->append(202);
    newBarSet_2->append(203);
    newBarSets.append(newBarSet_2);

    m_series->append(newBarSets);
    QCOMPARE(m_model->columnCount(), m_modelColumnCount + newBarSets.count());
}

void tst_qbarmodelmapper::verticalModelInsertRows()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int insertCount = 4;
    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount + insertCount);

    int first = 3;
    m_vMapper->setFirstRow(3);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount + insertCount - first);

    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount +  2 * insertCount - first);

    int countLimit = 6;
    m_vMapper->setRowCount(countLimit);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount + 2 * insertCount - first));

    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount + 3 * insertCount - first));

    m_vMapper->setFirstRow(0);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount + 3 * insertCount));

    m_vMapper->setRowCount(-1);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount + 3 * insertCount);
}

void tst_qbarmodelmapper::verticalModelRemoveRows()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int removeCount = 2;
    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount - removeCount);

    int first = 1;
    m_vMapper->setFirstRow(first);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount - removeCount - first);

    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount -  2 * removeCount - first);

    int countLimit = 3;
    m_vMapper->setRowCount(countLimit);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount -  2 * removeCount - first));

    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount -  3 * removeCount - first));

    m_vMapper->setFirstRow(0);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelRowCount -  3 * removeCount));

    m_vMapper->setRowCount(-1);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount -  3 * removeCount);
}

void tst_qbarmodelmapper::verticalModelInsertColumns()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int insertCount = 4;
    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
}

void tst_qbarmodelmapper::verticalModelRemoveColumns()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), qMin(m_model->columnCount(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1));
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int removeCount = m_modelColumnCount - 2;
    m_model->removeColumns(0, removeCount);
    QCOMPARE(m_series->count(), qMin(m_model->columnCount(), m_vMapper->lastBarSetColumn() - m_vMapper->firstBarSetColumn() + 1));
    QCOMPARE(m_series->barSets().first()->count(), m_modelRowCount);

    // leave all the columns
    m_model->removeColumns(0, m_modelColumnCount - removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qbarmodelmapper::horizontalModelInsertRows()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int insertCount = 4;
    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);
}

void tst_qbarmodelmapper::horizontalModelRemoveRows()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), qMin(m_model->rowCount(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1));
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int removeCount = m_modelRowCount - 2;
    m_model->removeRows(0, removeCount);
    QCOMPARE(m_series->count(), qMin(m_model->rowCount(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1));
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);

    // leave all the columns
    m_model->removeRows(0, m_modelRowCount - removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qbarmodelmapper::horizontalModelInsertColumns()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int insertCount = 4;
    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount + insertCount);

    int first = 3;
    m_hMapper->setFirstColumn(3);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount + insertCount - first);

    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount +  2 * insertCount - first);

    int countLimit = 6;
    m_hMapper->setColumnCount(countLimit);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount + 2 * insertCount - first));

    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount + 3 * insertCount - first));

    m_hMapper->setFirstColumn(0);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount + 3 * insertCount));

    m_hMapper->setColumnCount(-1);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount + 3 * insertCount);
}

void tst_qbarmodelmapper::horizontalModelRemoveColumns()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int removeCount = 2;
    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount - removeCount);

    int first = 1;
    m_hMapper->setFirstColumn(first);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount - removeCount - first);

    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount -  2 * removeCount - first);

    int countLimit = 3;
    m_hMapper->setColumnCount(countLimit);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount -  2 * removeCount - first));

    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount -  3 * removeCount - first));

    m_hMapper->setFirstColumn(0);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), qMin(countLimit, m_modelColumnCount -  3 * removeCount));

    m_hMapper->setColumnCount(-1);
    QCOMPARE(m_series->count(), m_hMapper->lastBarSetRow() - m_hMapper->firstBarSetRow() + 1);
    QCOMPARE(m_series->barSets().first()->count(), m_modelColumnCount -  3 * removeCount);
}

void tst_qbarmodelmapper::modelUpdateCell()
{
    // setup the mapper
    createVerticalMapper();

    QVERIFY(m_model->setData(m_model->index(1, 0), 44));
    QCOMPARE(m_series->barSets().at(0)->at(1), 44.0);
    QCOMPARE(m_model->data(m_model->index(1, 0)).toReal(), 44.0);
}

void tst_qbarmodelmapper::verticalMapperSignals()
{
    QVBarModelMapper *mapper = new QVBarModelMapper;

    QSignalSpy spy0(mapper, SIGNAL(firstRowChanged()));
    QSignalSpy spy1(mapper, SIGNAL(rowCountChanged()));
    QSignalSpy spy2(mapper, SIGNAL(firstBarSetColumnChanged()));
    QSignalSpy spy3(mapper, SIGNAL(lastBarSetColumnChanged()));
    QSignalSpy spy4(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy5(mapper, SIGNAL(seriesReplaced()));

    mapper->setFirstBarSetColumn(0);
    mapper->setLastBarSetColumn(1);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setFirstRow(1);
    mapper->setRowCount(5);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy5.count(), 1);

    delete mapper;
}

void tst_qbarmodelmapper::horizontalMapperSignals()
{
    QHBarModelMapper *mapper = new QHBarModelMapper;

    QSignalSpy spy0(mapper, SIGNAL(firstColumnChanged()));
    QSignalSpy spy1(mapper, SIGNAL(columnCountChanged()));
    QSignalSpy spy2(mapper, SIGNAL(firstBarSetRowChanged()));
    QSignalSpy spy3(mapper, SIGNAL(lastBarSetRowChanged()));
    QSignalSpy spy4(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy5(mapper, SIGNAL(seriesReplaced()));

    mapper->setFirstBarSetRow(0);
    mapper->setLastBarSetRow(1);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setFirstColumn(1);
    mapper->setColumnCount(5);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy5.count(), 1);

    delete mapper;
}

QTEST_MAIN(tst_qbarmodelmapper)

#include "tst_qbarmodelmapper.moc"
