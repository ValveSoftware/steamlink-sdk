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

#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QChartView>
#include <QtCharts/QHCandlestickModelMapper>
#include <QtCharts/QVCandlestickModelMapper>
#include <QtCore/QString>
#include <QtGui/QStandardItemModel>
#include <QtTest/QtTest>

QT_CHARTS_USE_NAMESPACE

class tst_qcandlestickmodelmapper : public QObject
{
    Q_OBJECT

public:
    tst_qcandlestickmodelmapper();

    void createVerticalMapper();
    void createHorizontalMapper();

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
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

    QCandlestickSeries *m_series;
    QChart *m_chart;
    QChartView *m_chartView;

    QHCandlestickModelMapper *m_hMapper;
    QVCandlestickModelMapper *m_vMapper;
};

tst_qcandlestickmodelmapper::tst_qcandlestickmodelmapper()
    : m_model(nullptr),
      m_modelRowCount(10),
      m_modelColumnCount(8),
      m_series(nullptr),
      m_chart(nullptr),
      m_chartView(nullptr),
      m_hMapper(nullptr),
      m_vMapper(nullptr)
{
}

void tst_qcandlestickmodelmapper::createHorizontalMapper()
{
    m_hMapper = new QHCandlestickModelMapper;
    QVERIFY(m_hMapper->model() == nullptr);
    m_hMapper->setTimestampColumn(0);
    m_hMapper->setOpenColumn(1);
    m_hMapper->setHighColumn(3);
    m_hMapper->setLowColumn(5);
    m_hMapper->setCloseColumn(6);
    m_hMapper->setFirstSetRow(0);
    m_hMapper->setLastSetRow(4);
    m_hMapper->setModel(m_model);
    m_hMapper->setSeries(m_series);
}

void tst_qcandlestickmodelmapper::createVerticalMapper()
{
    m_vMapper = new QVCandlestickModelMapper;
    QVERIFY(m_vMapper->model() == nullptr);
    m_vMapper->setTimestampRow(0);
    m_vMapper->setOpenRow(1);
    m_vMapper->setHighRow(3);
    m_vMapper->setLowRow(5);
    m_vMapper->setCloseRow(6);
    m_vMapper->setFirstSetColumn(0);
    m_vMapper->setLastSetColumn(4);
    m_vMapper->setModel(m_model);
    m_vMapper->setSeries(m_series);
}

void tst_qcandlestickmodelmapper::initTestCase()
{
    m_chart = new QChart();
    m_chartView = new QChartView(m_chart);
    m_chartView->show();
}

void tst_qcandlestickmodelmapper::cleanupTestCase()
{
    delete m_chartView;
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_qcandlestickmodelmapper::init()
{
    m_series = new QCandlestickSeries();
    m_chart->addSeries(m_series);

    m_model = new QStandardItemModel(m_modelRowCount, m_modelColumnCount, this);
    for (int row = 0; row < m_modelRowCount; ++row) {
        for (int column = 0; column < m_modelColumnCount; ++column)
            m_model->setData(m_model->index(row, column), row * column);
    }
}

void tst_qcandlestickmodelmapper::cleanup()
{
    m_chart->removeSeries(m_series);
    delete m_series;
    m_series = nullptr;

    m_model->clear();
    m_model->deleteLater();
    m_model = nullptr;

    if (m_vMapper) {
        m_vMapper->deleteLater();
        m_vMapper = nullptr;
    }

    if (m_hMapper) {
        m_hMapper->deleteLater();
        m_hMapper = nullptr;
    }
}

void tst_qcandlestickmodelmapper::verticalMapper_data()
{
    QTest::addColumn<int>("firstSetColumn");
    QTest::addColumn<int>("lastSetColumn");
    QTest::addColumn<int>("expectedSetCount");

    QTest::newRow("last column greater than first column") << 0 << 1 << 2;
    QTest::newRow("last column equal to first column") << 1 << 1 << 1;
    QTest::newRow("last column lesser than first column") << 1 << 0 << 0;
    QTest::newRow("invalid first column and correct last column") << -3 << 1 << 0;
    QTest::newRow("first column beyond the size of model and correct last column") << m_modelColumnCount << 1 << 0;
    QTest::newRow("first column beyond the size of model and invalid last column") << m_modelColumnCount << -1 << 0;
}

void tst_qcandlestickmodelmapper::verticalMapper()
{
    QFETCH(int, firstSetColumn);
    QFETCH(int, lastSetColumn);
    QFETCH(int, expectedSetCount);

    QCandlestickSeries *series = new QCandlestickSeries();
    m_chart->addSeries(series);

    createVerticalMapper();
    m_vMapper->setFirstSetColumn(firstSetColumn);
    m_vMapper->setLastSetColumn(lastSetColumn);
    m_vMapper->setSeries(series);

    QCOMPARE(m_vMapper->firstSetColumn(), qMax(firstSetColumn, -1));
    QCOMPARE(m_vMapper->lastSetColumn(), qMax(lastSetColumn, -1));
    QCOMPARE(series->count(), expectedSetCount);

    m_chart->removeSeries(series);
    delete series;
}

void tst_qcandlestickmodelmapper::verticalMapperCustomMapping_data()
{
    QTest::addColumn<int>("timestampRow");
    QTest::addColumn<int>("openRow");
    QTest::addColumn<int>("highRow");
    QTest::addColumn<int>("lowRow");
    QTest::addColumn<int>("closeRow");

    QTest::newRow("all rows are correct") << 0 << 1 << 2 << 3 << 4;
    QTest::newRow("all rows are invalid") << -3 << -3 << -3 << -3 << -3;
    QTest::newRow("timestamp: -1 (invalid)") << -1 << 1 << 2 << 3 << 4;
    QTest::newRow("timestamp: -3 (invalid - should default to -1)") << -3 << 1 << 2 << 3 << 4;
    QTest::newRow("timestamp: +1 greater than the number of rows in the model") << m_modelRowCount + 1 << 1 << 2 << 3 << 4;
    QTest::newRow("open: -1 (invalid)") << 0 << -1 << 2 << 3 << 4;
    QTest::newRow("open: -3 (invalid - should default to -1)") << 0 << -3 << 2 << 3 << 4;
    QTest::newRow("open: +1 greater than the number of rows in the model") << 0 << m_modelRowCount + 1 << 2 << 3 << 4;
    QTest::newRow("high: -1 (invalid)") << 0 << 1 << -1 << 3 << 4;
    QTest::newRow("high: -3 (invalid - should default to -1)") << 0 << 1 << -3 << 3 << 4;
    QTest::newRow("high: +1 greater than the number of rows in the model") << 0 << 1 << m_modelRowCount + 1 << 3 << 4;
    QTest::newRow("low: -1 (invalid)") << 0 << 1 << 2 << -1 << 4;
    QTest::newRow("low: -3 (invalid - should default to -1)") << 0 << 1 << 2 << -3 << 4;
    QTest::newRow("low: +1 greater than the number of rows in the model") << 0 << 1 << 2 << m_modelRowCount + 1 << 4;
    QTest::newRow("close: -1 (invalid)") << 0 << 1 << 2 << 3 << -1;
    QTest::newRow("close: -3 (invalid - should default to -1)") << 0 << 1 << 2 << 3 << -3;
    QTest::newRow("close: +1 greater than the number of rows in the model") << 0 << 1 << 2 << 3 << m_modelRowCount + 1;
}

void tst_qcandlestickmodelmapper::verticalMapperCustomMapping()
{
    QFETCH(int, timestampRow);
    QFETCH(int, openRow);
    QFETCH(int, highRow);
    QFETCH(int, lowRow);
    QFETCH(int, closeRow);

    QCandlestickSeries *series = new QCandlestickSeries();
    m_chart->addSeries(series);
    QCOMPARE(series->count(), 0);

    createVerticalMapper();
    m_vMapper->setTimestampRow(timestampRow);
    m_vMapper->setOpenRow(openRow);
    m_vMapper->setHighRow(highRow);
    m_vMapper->setLowRow(lowRow);
    m_vMapper->setCloseRow(closeRow);
    m_vMapper->setSeries(series);

    QCOMPARE(m_vMapper->timestampRow(), qMax(timestampRow, -1));
    QCOMPARE(m_vMapper->openRow(), qMax(openRow, -1));
    QCOMPARE(m_vMapper->highRow(), qMax(highRow, -1));
    QCOMPARE(m_vMapper->lowRow(), qMax(lowRow, -1));
    QCOMPARE(m_vMapper->closeRow(), qMax(closeRow, -1));

    int count;
    if ((m_vMapper->timestampRow() >= 0 && m_vMapper->timestampRow() < m_modelRowCount)
        && (m_vMapper->openRow() >= 0 && m_vMapper->openRow() < m_modelRowCount)
        && (m_vMapper->highRow() >= 0 && m_vMapper->highRow() < m_modelRowCount)
        && (m_vMapper->lowRow() >= 0 && m_vMapper->lowRow() < m_modelRowCount)
        && (m_vMapper->closeRow() >= 0 && m_vMapper->closeRow() < m_modelRowCount))
        count = m_vMapper->lastSetColumn() - m_vMapper->firstSetColumn() + 1;
    else
        count = 0;
    QCOMPARE(series->count(), count);

    // change values column mapping to invalid
    m_vMapper->setFirstSetColumn(-1);
    m_vMapper->setLastSetColumn(1);
    QCOMPARE(series->count(), 0);

    m_chart->removeSeries(series);
    delete series;
}

void tst_qcandlestickmodelmapper::horizontalMapper_data()
{
    QTest::addColumn<int>("firstSetRow");
    QTest::addColumn<int>("lastSetRow");
    QTest::addColumn<int>("expectedSetCount");

    QTest::newRow("last row greater than first row") << 0 << 1 << 2;
    QTest::newRow("last row equal to first row") << 1 << 1 << 1;
    QTest::newRow("last row lesser than first row") << 1 << 0 << 0;
    QTest::newRow("invalid first row and correct last row") << -3 << 1 << 0;
    QTest::newRow("first row beyond the size of model and correct last row") << m_modelRowCount << 1 << 0;
    QTest::newRow("first row beyond the size of model and invalid last row") << m_modelRowCount << -1 << 0;
}

void tst_qcandlestickmodelmapper::horizontalMapper()
{
    QFETCH(int, firstSetRow);
    QFETCH(int, lastSetRow);
    QFETCH(int, expectedSetCount);

    QCandlestickSeries *series = new QCandlestickSeries();
    m_chart->addSeries(series);

    createHorizontalMapper();
    m_hMapper->setFirstSetRow(firstSetRow);
    m_hMapper->setLastSetRow(lastSetRow);
    m_hMapper->setSeries(series);

    QCOMPARE(m_hMapper->firstSetRow(), qMax(firstSetRow, -1));
    QCOMPARE(m_hMapper->lastSetRow(), qMax(lastSetRow, -1));
    QCOMPARE(series->count(), expectedSetCount);

    m_chart->removeSeries(series);
    delete series;
}

void tst_qcandlestickmodelmapper::horizontalMapperCustomMapping_data()
{
    QTest::addColumn<int>("timestampColumn");
    QTest::addColumn<int>("openColumn");
    QTest::addColumn<int>("highColumn");
    QTest::addColumn<int>("lowColumn");
    QTest::addColumn<int>("closeColumn");

    QTest::newRow("all columns are correct") << 0 << 1 << 2 << 3 << 4;
    QTest::newRow("all columns are invalid") << -3 << -3 << -3 << -3 << -3;
    QTest::newRow("timestamp: -1 (invalid)") << -1 << 1 << 2 << 3 << 4;
    QTest::newRow("timestamp: -3 (invalid - should default to -1)") << -3 << 1 << 2 << 3 << 4;
    QTest::newRow("timestamp: +1 greater than the number of columns in the model") << m_modelColumnCount + 1 << 1 << 2 << 3 << 4;
    QTest::newRow("open: -1 (invalid)") << 0 << -1 << 2 << 3 << 4;
    QTest::newRow("open: -3 (invalid - should default to -1)") << 0 << -3 << 2 << 3 << 4;
    QTest::newRow("open: +1 greater than the number of columns in the model") << 0 << m_modelColumnCount + 1 << 2 << 3 << 4;
    QTest::newRow("high: -1 (invalid)") << 0 << 1 << -1 << 3 << 4;
    QTest::newRow("high: -3 (invalid - should default to -1)") << 0 << 1 << -3 << 3 << 4;
    QTest::newRow("high: +1 greater than the number of columns in the model") << 0 << 1 << m_modelColumnCount + 1 << 3 << 4;
    QTest::newRow("low: -1 (invalid)") << 0 << 1 << 2 << -1 << 4;
    QTest::newRow("low: -3 (invalid - should default to -1)") << 0 << 1 << 2 << -3 << 4;
    QTest::newRow("low: +1 greater than the number of columns in the model") << 0 << 1 << 2 << m_modelColumnCount + 1 << 4;
    QTest::newRow("close: -1 (invalid)") << 0 << 1 << 2 << 3 << -1;
    QTest::newRow("close: -3 (invalid - should default to -1)") << 0 << 1 << 2 << 3 << -3;
    QTest::newRow("close: +1 greater than the number of columns in the model") << 0 << 1 << 2 << 3 << m_modelColumnCount + 1;
}

void tst_qcandlestickmodelmapper::horizontalMapperCustomMapping()
{
    QFETCH(int, timestampColumn);
    QFETCH(int, openColumn);
    QFETCH(int, highColumn);
    QFETCH(int, lowColumn);
    QFETCH(int, closeColumn);

    QCandlestickSeries *series = new QCandlestickSeries();
    m_chart->addSeries(series);
    QCOMPARE(series->count(), 0);

    createHorizontalMapper();
    m_hMapper->setTimestampColumn(timestampColumn);
    m_hMapper->setOpenColumn(openColumn);
    m_hMapper->setHighColumn(highColumn);
    m_hMapper->setLowColumn(lowColumn);
    m_hMapper->setCloseColumn(closeColumn);
    m_hMapper->setSeries(series);

    QCOMPARE(m_hMapper->timestampColumn(), qMax(timestampColumn, -1));
    QCOMPARE(m_hMapper->openColumn(), qMax(openColumn, -1));
    QCOMPARE(m_hMapper->highColumn(), qMax(highColumn, -1));
    QCOMPARE(m_hMapper->lowColumn(), qMax(lowColumn, -1));
    QCOMPARE(m_hMapper->closeColumn(), qMax(closeColumn, -1));

    int count;
    if ((m_hMapper->timestampColumn() >= 0 && m_hMapper->timestampColumn() < m_modelColumnCount)
        && (m_hMapper->openColumn() >= 0 && m_hMapper->openColumn() < m_modelColumnCount)
        && (m_hMapper->highColumn() >= 0 && m_hMapper->highColumn() < m_modelColumnCount)
        && (m_hMapper->lowColumn() >= 0 && m_hMapper->lowColumn() < m_modelColumnCount)
        && (m_hMapper->closeColumn() >= 0 && m_hMapper->closeColumn() < m_modelColumnCount))
        count = m_hMapper->lastSetRow() - m_hMapper->firstSetRow() + 1;
    else
        count = 0;
    QCOMPARE(series->count(), count);

    // change values row mapping to invalid
    m_hMapper->setFirstSetRow(-1);
    m_hMapper->setLastSetRow(1);
    QCOMPARE(series->count(), 0);

    m_chart->removeSeries(series);
    delete series;
}

void tst_qcandlestickmodelmapper::seriesUpdated()
{
    createVerticalMapper();
    QVERIFY(m_vMapper->model() != nullptr);

    QCandlestickSet *set = m_series->sets().value(0, 0);
    QVERIFY(set != nullptr);

    // update values
    QCOMPARE(m_model->data(m_model->index(m_vMapper->timestampRow(), 0)).toReal(),set->timestamp());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->openRow(), 0)).toReal(), set->open());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->highRow(), 0)).toReal(), set->high());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->lowRow(), 0)).toReal(), set->low());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->closeRow(), 0)).toReal(), set->close());
    set->setTimestamp(set->timestamp() + 5.0);
    set->setOpen(set->open() + 6.0);
    set->setHigh(set->high() + 7.0);
    set->setLow(set->low() + 8.0);
    set->setClose(set->close() + 9.0);
    QCOMPARE(m_model->data(m_model->index(m_vMapper->timestampRow(), 0)).toReal(),set->timestamp());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->openRow(), 0)).toReal(), set->open());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->highRow(), 0)).toReal(), set->high());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->lowRow(), 0)).toReal(), set->low());
    QCOMPARE(m_model->data(m_model->index(m_vMapper->closeRow(), 0)).toReal(), set->close());

    // append new sets
    QList<QCandlestickSet *> newCandlestickSets;
    newCandlestickSets << new QCandlestickSet(3.0, 5.0, 2.0, 4.0, 1234);
    newCandlestickSets << new QCandlestickSet(5.0, 7.0, 4.0, 6.0, 5678);
    m_series->append(newCandlestickSets);
    QCOMPARE(m_model->columnCount(), m_modelColumnCount + newCandlestickSets.count());

    // remove sets
    newCandlestickSets.clear();
    newCandlestickSets << m_series->sets().at(m_series->count() - 1);
    newCandlestickSets << m_series->sets().at(m_series->count() - 2);
    m_series->remove(newCandlestickSets);
    QCOMPARE(m_model->columnCount(), m_modelColumnCount);
}

void tst_qcandlestickmodelmapper::verticalModelInsertRows()
{
    createVerticalMapper();
    int count = m_vMapper->lastSetColumn() - m_vMapper->firstSetColumn() + 1;
    QVERIFY(m_vMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->insertRows(3, 4);
    QCOMPARE(m_series->count(), count);
}

void tst_qcandlestickmodelmapper::verticalModelRemoveRows()
{
    createVerticalMapper();
    int count = m_vMapper->lastSetColumn() - m_vMapper->firstSetColumn() + 1;
    QVERIFY(m_vMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->removeRows(m_modelRowCount - 1, 1);
    QCOMPARE(m_series->count(), count);

    int removeCount = m_model->rowCount() - m_vMapper->closeRow();
    m_model->removeRows(m_vMapper->closeRow(), removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qcandlestickmodelmapper::verticalModelInsertColumns()
{
    createVerticalMapper();
    int count = m_vMapper->lastSetColumn() - m_vMapper->firstSetColumn() + 1;
    QVERIFY(m_vMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->insertColumns(3, 4);
    QCOMPARE(m_series->count(), count);
}

void tst_qcandlestickmodelmapper::verticalModelRemoveColumns()
{
    createVerticalMapper();
    int count = m_vMapper->lastSetColumn() - m_vMapper->firstSetColumn() + 1;
    QVERIFY(m_vMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    int removeCount = m_modelColumnCount - 2;
    m_model->removeColumns(0, removeCount);
    QCOMPARE(m_series->count(), qMin(m_model->columnCount(), count));

    // leave all the columns
    m_model->removeColumns(0, m_modelColumnCount - removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qcandlestickmodelmapper::horizontalModelInsertRows()
{
    createHorizontalMapper();
    int count = m_hMapper->lastSetRow() - m_hMapper->firstSetRow() + 1;
    QVERIFY(m_hMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->insertRows(3, 4);
    QCOMPARE(m_series->count(), count);
}

void tst_qcandlestickmodelmapper::horizontalModelRemoveRows()
{
    createHorizontalMapper();
    int count = m_hMapper->lastSetRow() - m_hMapper->firstSetRow() + 1;
    QVERIFY(m_hMapper->model() != 0);
    QCOMPARE(m_series->count(), qMin(m_model->rowCount(), count));

    int removeCount = m_modelRowCount - 2;
    m_model->removeRows(0, removeCount);
    QCOMPARE(m_series->count(), qMin(m_model->rowCount(), count));

    // leave all the columns
    m_model->removeRows(0, m_modelRowCount - removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qcandlestickmodelmapper::horizontalModelInsertColumns()
{
    createHorizontalMapper();
    int count = m_hMapper->lastSetRow() - m_hMapper->firstSetRow() + 1;
    QVERIFY(m_hMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->insertColumns(3, 4);
    QCOMPARE(m_series->count(), count);
}

void tst_qcandlestickmodelmapper::horizontalModelRemoveColumns()
{
    createHorizontalMapper();
    int count = m_hMapper->lastSetRow() - m_hMapper->firstSetRow() + 1;
    QVERIFY(m_hMapper->model() != 0);
    QCOMPARE(m_series->count(), count);

    m_model->removeColumns(m_modelColumnCount - 1, 1);
    QCOMPARE(m_series->count(), count);

    int removeCount = m_model->columnCount() - m_hMapper->closeColumn();
    m_model->removeColumns(m_hMapper->closeColumn(), removeCount);
    QCOMPARE(m_series->count(), 0);
}

void tst_qcandlestickmodelmapper::modelUpdateCell()
{
    createVerticalMapper();
    QVERIFY(m_vMapper->model() != 0);

    QModelIndex index = m_model->index(m_vMapper->timestampRow(), 0);
    qreal newValue = 44.0;
    QVERIFY(m_model->setData(index, newValue));
    QCOMPARE(m_model->data(index).toReal(), newValue);
    QCOMPARE(m_series->sets().at(index.row())->timestamp(), newValue);
}

void tst_qcandlestickmodelmapper::verticalMapperSignals()
{
    QVCandlestickModelMapper *mapper = new QVCandlestickModelMapper();

    QSignalSpy spy0(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy1(mapper, SIGNAL(seriesReplaced()));
    QSignalSpy spy2(mapper, SIGNAL(timestampRowChanged()));
    QSignalSpy spy3(mapper, SIGNAL(openRowChanged()));
    QSignalSpy spy4(mapper, SIGNAL(highRowChanged()));
    QSignalSpy spy5(mapper, SIGNAL(lowRowChanged()));
    QSignalSpy spy6(mapper, SIGNAL(closeRowChanged()));
    QSignalSpy spy7(mapper, SIGNAL(firstSetColumnChanged()));
    QSignalSpy spy8(mapper, SIGNAL(lastSetColumnChanged()));

    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setTimestampRow(1);
    mapper->setOpenRow(2);
    mapper->setHighRow(3);
    mapper->setLowRow(4);
    mapper->setCloseRow(5);
    mapper->setFirstSetColumn(0);
    mapper->setLastSetColumn(1);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy5.count(), 1);
    QCOMPARE(spy6.count(), 1);
    QCOMPARE(spy7.count(), 1);
    QCOMPARE(spy8.count(), 1);

    delete mapper;
}

void tst_qcandlestickmodelmapper::horizontalMapperSignals()
{
    QHCandlestickModelMapper *mapper = new QHCandlestickModelMapper();

    QSignalSpy spy0(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy1(mapper, SIGNAL(seriesReplaced()));
    QSignalSpy spy2(mapper, SIGNAL(timestampColumnChanged()));
    QSignalSpy spy3(mapper, SIGNAL(openColumnChanged()));
    QSignalSpy spy4(mapper, SIGNAL(highColumnChanged()));
    QSignalSpy spy5(mapper, SIGNAL(lowColumnChanged()));
    QSignalSpy spy6(mapper, SIGNAL(closeColumnChanged()));
    QSignalSpy spy7(mapper, SIGNAL(firstSetRowChanged()));
    QSignalSpy spy8(mapper, SIGNAL(lastSetRowChanged()));

    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setTimestampColumn(1);
    mapper->setOpenColumn(2);
    mapper->setHighColumn(3);
    mapper->setLowColumn(4);
    mapper->setCloseColumn(5);
    mapper->setFirstSetRow(0);
    mapper->setLastSetRow(1);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy5.count(), 1);
    QCOMPARE(spy6.count(), 1);
    QCOMPARE(spy7.count(), 1);
    QCOMPARE(spy8.count(), 1);

    delete mapper;
}

QTEST_MAIN(tst_qcandlestickmodelmapper)

#include "tst_qcandlestickmodelmapper.moc"
