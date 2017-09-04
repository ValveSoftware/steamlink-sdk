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
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QVPieModelMapper>
#include <QtCharts/QHPieModelMapper>
#include <QtGui/QStandardItemModel>

QT_CHARTS_USE_NAMESPACE

class tst_qpiemodelmapper : public QObject
{
    Q_OBJECT
    
    public:
    tst_qpiemodelmapper();
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
    QVPieModelMapper *m_vMapper;
    QHPieModelMapper *m_hMapper;

    QPieSeries *m_series;
    QChart *m_chart;
    QChartView *m_chartView;
};

tst_qpiemodelmapper::tst_qpiemodelmapper():
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

void tst_qpiemodelmapper::createVerticalMapper()
{
    m_vMapper = new QVPieModelMapper;
    QVERIFY(m_vMapper->model() == 0);
    m_vMapper->setValuesColumn(0);
    m_vMapper->setLabelsColumn(1);
    m_vMapper->setModel(m_model);
    m_vMapper->setSeries(m_series);
}

void tst_qpiemodelmapper::createHorizontalMapper()
{
    m_hMapper = new QHPieModelMapper;
    QVERIFY(m_hMapper->model() == 0);
    m_hMapper->setValuesRow(0);
    m_hMapper->setLabelsRow(1);
    m_hMapper->setModel(m_model);
    m_hMapper->setSeries(m_series);
}

void tst_qpiemodelmapper::init()
{
    m_series = new QPieSeries;
    m_chart->addSeries(m_series);

    m_model = new QStandardItemModel(m_modelRowCount, m_modelColumnCount, this);
    for (int row = 0; row < m_modelRowCount; ++row) {
        for (int column = 0; column < m_modelColumnCount; column++) {
            m_model->setData(m_model->index(row, column), row * column);
        }
    }
}

void tst_qpiemodelmapper::cleanup()
{
    m_chart->removeSeries(m_series);
    m_series->deleteLater();
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

void tst_qpiemodelmapper::initTestCase()
{
    m_chart = new QChart;
    m_chartView = new QChartView(m_chart);
    m_chartView->resize(200, 200);
    m_chartView->show();
}

void tst_qpiemodelmapper::cleanupTestCase()
{
    delete m_chartView;
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_qpiemodelmapper::verticalMapper_data()
{
    QTest::addColumn<int>("valuesColumn");
    QTest::addColumn<int>("labelsColumn");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("different values and labels columns") << 0 << 1 << m_modelRowCount;
    QTest::newRow("same values and labels columns") << 1 << 1 << m_modelRowCount;
    QTest::newRow("invalid values column and correct labels column") << -3 << 1 << 0;
    QTest::newRow("values column beyond the size of model and correct labels column") << m_modelColumnCount << 1 << 0;
    QTest::newRow("values column beyond the size of model and invalid labels column") << m_modelColumnCount << -1 << 0;
}

void tst_qpiemodelmapper::verticalMapper()
{
    QFETCH(int, valuesColumn);
    QFETCH(int, labelsColumn);
    QFETCH(int, expectedCount);

    QVPieModelMapper *mapper = new QVPieModelMapper;
    mapper->setValuesColumn(valuesColumn);
    mapper->setLabelsColumn(labelsColumn);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);

    QCOMPARE(m_series->count(), expectedCount);
    QCOMPARE(mapper->valuesColumn(), qMax(-1, valuesColumn));
    QCOMPARE(mapper->labelsColumn(), qMax(-1, labelsColumn));

    delete mapper;
    mapper = 0;
}

void tst_qpiemodelmapper::verticalMapperCustomMapping_data()
{
    QTest::addColumn<int>("first");
    QTest::addColumn<int>("countLimit");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("first: 0, unlimited count") << 0 << -1 << m_modelRowCount;
    QTest::newRow("first: 3, unlimited count") << 3 << -1 << m_modelRowCount - 3;
    QTest::newRow("first: 0, count: 5") << 0 << 5 << qMin(5, m_modelRowCount);
    QTest::newRow("first: 3, count: 5") << 3 << 5 << qMin(5, m_modelRowCount - 3);
    QTest::newRow("first: +1 greater then the number of rows in the model, unlimited count") << m_modelRowCount + 1 << -1 << 0;
    QTest::newRow("first: +1 greater then the number of rows in the model, count: 5") << m_modelRowCount + 1 << 5 << 0;
    QTest::newRow("first: 0, count: +3 greater than the number of rows in the model (should limit to the size of model)") << 0 << m_modelRowCount + 3 << m_modelRowCount;
    QTest::newRow("first: -3(invalid - should default to 0), unlimited count") << -3 << -1 << m_modelRowCount;
    QTest::newRow("first: 0, count: -3 (invalid - shlould default to -1)") << 0 << -3 << m_modelRowCount;
    QTest::newRow("first: -3(invalid - should default to 0), count: -3 (invalid - shlould default to -1)") << -3 << -3 << m_modelRowCount;

}

void tst_qpiemodelmapper::verticalMapperCustomMapping()
{
    QFETCH(int, first);
    QFETCH(int, countLimit);
    QFETCH(int, expectedCount);

    QCOMPARE(m_series->count(), 0);

    QVPieModelMapper *mapper = new QVPieModelMapper;
    mapper->setValuesColumn(0);
    mapper->setLabelsColumn(1);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setFirstRow(first);
    mapper->setRowCount(countLimit);

    QCOMPARE(m_series->count(), expectedCount);

    // change values column mapping to invalid
    mapper->setValuesColumn(-1);
    mapper->setLabelsColumn(1);

    QCOMPARE(m_series->count(), 0);

    delete mapper;
    mapper = 0;
}

void tst_qpiemodelmapper::horizontalMapper_data()
{
    QTest::addColumn<int>("valuesRow");
    QTest::addColumn<int>("labelsRow");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("different values and labels rows") << 0 << 1 << m_modelColumnCount;
    QTest::newRow("same values and labels rows") << 1 << 1 << m_modelColumnCount;
    QTest::newRow("invalid values row and correct labels row") << -3 << 1 << 0;
    QTest::newRow("values row beyond the size of model and correct labels row") << m_modelRowCount << 1 << 0;
    QTest::newRow("values row beyond the size of model and invalid labels row") << m_modelRowCount << -1 << 0;
}

void tst_qpiemodelmapper::horizontalMapper()
{
    QFETCH(int, valuesRow);
    QFETCH(int, labelsRow);
    QFETCH(int, expectedCount);

    QHPieModelMapper *mapper = new QHPieModelMapper;
    mapper->setValuesRow(valuesRow);
    mapper->setLabelsRow(labelsRow);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);

    QCOMPARE(m_series->count(), expectedCount);
    QCOMPARE(mapper->valuesRow(), qMax(-1, valuesRow));
    QCOMPARE(mapper->labelsRow(), qMax(-1, labelsRow));

    delete mapper;
    mapper = 0;
}

void tst_qpiemodelmapper::horizontalMapperCustomMapping_data()
{
    QTest::addColumn<int>("first");
    QTest::addColumn<int>("countLimit");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("first: 0, unlimited count") << 0 << -1 << m_modelColumnCount;
    QTest::newRow("first: 3, unlimited count") << 3 << -1 << m_modelColumnCount - 3;
    QTest::newRow("first: 0, count: 5") << 0 << 5 << qMin(5, m_modelColumnCount);
    QTest::newRow("first: 3, count: 5") << 3 << 5 << qMin(5, m_modelColumnCount - 3);
    QTest::newRow("first: +1 greater then the number of columns in the model, unlimited count") << m_modelColumnCount + 1 << -1 << 0;
    QTest::newRow("first: +1 greater then the number of columns in the model, count: 5") << m_modelColumnCount + 1 << 5 << 0;
    QTest::newRow("first: 0, count: +3 greater than the number of columns in the model (should limit to the size of model)") << 0 << m_modelColumnCount + 3 << m_modelColumnCount;
    QTest::newRow("first: -3(invalid - should default to 0), unlimited count") << -3 << -1 << m_modelColumnCount;
    QTest::newRow("first: 0, count: -3 (invalid - shlould default to -1)") << 0 << -3 << m_modelColumnCount;
    QTest::newRow("first: -3(invalid - should default to 0), count: -3 (invalid - shlould default to -1)") << -3 << -3 << m_modelColumnCount;
}

void tst_qpiemodelmapper::horizontalMapperCustomMapping()
{
    QFETCH(int, first);
    QFETCH(int, countLimit);
    QFETCH(int, expectedCount);

    QCOMPARE(m_series->count(), 0);

    QHPieModelMapper *mapper = new QHPieModelMapper;
    mapper->setValuesRow(0);
    mapper->setLabelsRow(1);
    mapper->setModel(m_model);
    mapper->setSeries(m_series);
    mapper->setFirstColumn(first);
    mapper->setColumnCount(countLimit);

    QCOMPARE(m_series->count(), expectedCount);

    // change values row mapping to invalid
    mapper->setValuesRow(-1);
    mapper->setLabelsRow(1);

    QCOMPARE(m_series->count(), 0);

    delete mapper;
    mapper = 0;
}

void tst_qpiemodelmapper::seriesUpdated()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_modelRowCount);
    QCOMPARE(m_vMapper->rowCount(), -1);

    m_series->append("1000", 1000);
    QCOMPARE(m_series->count(), m_modelRowCount + 1);
    QCOMPARE(m_vMapper->rowCount(), -1); // the value should not change as it indicates 'all' items there are in the model

    m_series->remove(m_series->slices().last());
    QCOMPARE(m_series->count(), m_modelRowCount);
    QCOMPARE(m_vMapper->rowCount(), -1); // the value should not change as it indicates 'all' items there are in the model

    QPieSlice *slice = m_series->slices().first();
    slice->setValue(25.0);
    slice->setLabel(QString("25.0"));
    QCOMPARE(m_model->data(m_model->index(0, 0)).toReal(), 25.0);
    QCOMPARE(m_model->data(m_model->index(0, 1)).toString(), QString("25.0"));
}

void tst_qpiemodelmapper::verticalModelInsertRows()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int insertCount = 4;
    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_modelRowCount + insertCount);

    int first = 3;
    m_vMapper->setFirstRow(3);
    QCOMPARE(m_series->count(), m_modelRowCount + insertCount - first);

    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_modelRowCount +  2 * insertCount - first);

    int countLimit = 6;
    m_vMapper->setRowCount(countLimit);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount + 2 * insertCount - first));

    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount + 3 * insertCount - first));

    m_vMapper->setFirstRow(0);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount + 3 * insertCount));

    m_vMapper->setRowCount(-1);
    QCOMPARE(m_series->count(), m_modelRowCount + 3 * insertCount);
}

void tst_qpiemodelmapper::verticalModelRemoveRows()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int removeCount = 2;
    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), m_modelRowCount - removeCount);

    int first = 1;
    m_vMapper->setFirstRow(first);
    QCOMPARE(m_series->count(), m_modelRowCount - removeCount - first);

    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), m_modelRowCount -  2 * removeCount - first);

    int countLimit = 3;
    m_vMapper->setRowCount(countLimit);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount -  2 * removeCount - first));

    m_model->removeRows(1, removeCount);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount -  3 * removeCount - first));

    m_vMapper->setFirstRow(0);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelRowCount -  3 * removeCount));

    m_vMapper->setRowCount(-1);
    QCOMPARE(m_series->count(), m_modelRowCount -  3 * removeCount);
}

void tst_qpiemodelmapper::verticalModelInsertColumns()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int insertCount = 4;
    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_modelRowCount);
}

void tst_qpiemodelmapper::verticalModelRemoveColumns()
{
    // setup the mapper
    createVerticalMapper();
    QCOMPARE(m_series->count(), m_modelRowCount);
    QVERIFY(m_vMapper->model() != 0);

    int removeCount = m_modelColumnCount - 2;
    m_model->removeColumns(0, removeCount);
    QCOMPARE(m_series->count(), m_modelRowCount);

    // leave only one column
    m_model->removeColumns(0, m_modelColumnCount - removeCount - 1);
    QCOMPARE(m_series->count(), 0);
}

void tst_qpiemodelmapper::horizontalModelInsertRows()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int insertCount = 4;
    m_model->insertRows(3, insertCount);
    QCOMPARE(m_series->count(), m_modelColumnCount);
}

void tst_qpiemodelmapper::horizontalModelRemoveRows()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int removeCount = m_modelRowCount - 2;
    m_model->removeRows(0, removeCount);
    QCOMPARE(m_series->count(), m_modelColumnCount);

    // leave only one column
    m_model->removeRows(0, m_modelRowCount - removeCount - 1);
    QCOMPARE(m_series->count(), 0);
}

void tst_qpiemodelmapper::horizontalModelInsertColumns()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int insertCount = 4;
    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_modelColumnCount + insertCount);

    int first = 3;
    m_hMapper->setFirstColumn(3);
    QCOMPARE(m_series->count(), m_modelColumnCount + insertCount - first);

    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), m_modelColumnCount +  2 * insertCount - first);

    int countLimit = 6;
    m_hMapper->setColumnCount(countLimit);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount + 2 * insertCount - first));

    m_model->insertColumns(3, insertCount);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount + 3 * insertCount - first));

    m_hMapper->setFirstColumn(0);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount + 3 * insertCount));

    m_hMapper->setColumnCount(-1);
    QCOMPARE(m_series->count(), m_modelColumnCount + 3 * insertCount);
}

void tst_qpiemodelmapper::horizontalModelRemoveColumns()
{
    // setup the mapper
    createHorizontalMapper();
    QCOMPARE(m_series->count(), m_modelColumnCount);
    QVERIFY(m_hMapper->model() != 0);

    int removeCount = 2;
    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), m_modelColumnCount - removeCount);

    int first = 1;
    m_hMapper->setFirstColumn(first);
    QCOMPARE(m_series->count(), m_modelColumnCount - removeCount - first);

    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), m_modelColumnCount -  2 * removeCount - first);

    int countLimit = 3;
    m_hMapper->setColumnCount(countLimit);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount -  2 * removeCount - first));

    m_model->removeColumns(1, removeCount);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount -  3 * removeCount - first));

    m_hMapper->setFirstColumn(0);
    QCOMPARE(m_series->count(), qMin(countLimit, m_modelColumnCount -  3 * removeCount));

    m_hMapper->setColumnCount(-1);
    QCOMPARE(m_series->count(), m_modelColumnCount -  3 * removeCount);
}

void tst_qpiemodelmapper::modelUpdateCell()
{
    // setup the mapper
    createVerticalMapper();

    QVERIFY(m_model->setData(m_model->index(1, 0), 44));
    QCOMPARE(m_series->slices().at(1)->value(), 44.0);
    QCOMPARE(m_model->data(m_model->index(1, 0)).toReal(), 44.0);
}

void tst_qpiemodelmapper::verticalMapperSignals()
{
    QVPieModelMapper *mapper = new QVPieModelMapper;

    QSignalSpy spy0(mapper, SIGNAL(firstRowChanged()));
    QSignalSpy spy1(mapper, SIGNAL(rowCountChanged()));
    QSignalSpy spy2(mapper, SIGNAL(valuesColumnChanged()));
    QSignalSpy spy3(mapper, SIGNAL(labelsColumnChanged()));
    QSignalSpy spy4(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy5(mapper, SIGNAL(seriesReplaced()));

    mapper->setValuesColumn(0);
    mapper->setLabelsColumn(1);
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

void tst_qpiemodelmapper::horizontalMapperSignals()
{
    QHPieModelMapper *mapper = new QHPieModelMapper;

    QSignalSpy spy0(mapper, SIGNAL(firstColumnChanged()));
    QSignalSpy spy1(mapper, SIGNAL(columnCountChanged()));
    QSignalSpy spy2(mapper, SIGNAL(valuesRowChanged()));
    QSignalSpy spy3(mapper, SIGNAL(labelsRowChanged()));
    QSignalSpy spy4(mapper, SIGNAL(modelReplaced()));
    QSignalSpy spy5(mapper, SIGNAL(seriesReplaced()));

    mapper->setValuesRow(0);
    mapper->setLabelsRow(1);
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
}

QTEST_MAIN(tst_qpiemodelmapper)

#include "tst_qpiemodelmapper.moc"
