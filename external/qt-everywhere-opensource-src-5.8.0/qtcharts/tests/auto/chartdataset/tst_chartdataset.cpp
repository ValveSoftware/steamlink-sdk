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

#include <QtTest/QtTest>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QCategoryAxis>
#ifndef QT_QREAL_IS_FLOAT
#include <QtCharts/QDateTimeAxis>
#endif
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <private/chartdataset_p.h>
#include <private/abstractdomain_p.h>
#include <tst_definitions.h>

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(AbstractDomain *)
Q_DECLARE_METATYPE(QAbstractAxis *)
Q_DECLARE_METATYPE(QAbstractSeries *)
Q_DECLARE_METATYPE(QList<QAbstractSeries *>)
Q_DECLARE_METATYPE(QList<QAbstractAxis *>)
Q_DECLARE_METATYPE(Qt::Alignment)
Q_DECLARE_METATYPE(QList<Qt::Alignment>)
Q_DECLARE_METATYPE(QLineSeries *)

class tst_ChartDataSet: public QObject {

    Q_OBJECT
public:
    tst_ChartDataSet():m_dataset(0){};

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void chartdataset_data();
    void chartdataset();
    void addSeries_data();
    void addSeries();
    void removeSeries_data();
    void removeSeries();
    void addAxis_data();
    void addAxis();
    void removeAxis_data();
    void removeAxis();
    void attachAxis_data();
    void attachAxis();
    void detachAxis_data();
    void detachAxis();

private:
    ChartDataSet* m_dataset;
};

void tst_ChartDataSet::initTestCase()
{
    qRegisterMetaType<AbstractDomain*>();
    qRegisterMetaType<QAbstractAxis*>();
    qRegisterMetaType<QAbstractSeries*>();
}

void tst_ChartDataSet::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_ChartDataSet::init()
{
    Q_ASSERT(!m_dataset);
    m_dataset = new ChartDataSet(0);
}


void tst_ChartDataSet::cleanup()
{
    delete m_dataset;
    m_dataset = 0;
}

void tst_ChartDataSet::chartdataset_data()
{
}

void tst_ChartDataSet::chartdataset()
{
	QVERIFY(m_dataset->axes().isEmpty());
	QVERIFY(m_dataset->series().isEmpty());
	m_dataset->createDefaultAxes();
}


void tst_ChartDataSet::addSeries_data()
{
    QTest::addColumn<QAbstractSeries*>("series");

    QAbstractSeries* line = new QLineSeries(this);
    QTest::newRow("line") << line;

    QAbstractSeries* area = new QAreaSeries(static_cast<QLineSeries*>(new QLineSeries(this)));
    QTest::newRow("area") << area;

    QAbstractSeries* scatter = new QScatterSeries(this);
    QTest::newRow("scatter") << scatter;

    QAbstractSeries* spline = new QSplineSeries(this);
    QTest::newRow("spline") << spline;

    QAbstractSeries* pie = new QPieSeries(this);
    QTest::newRow("pie") << pie;

    QAbstractSeries* bar = new QBarSeries(this);
    QTest::newRow("bar") << bar;

    QAbstractSeries* percent = new QPercentBarSeries(this);
    QTest::newRow("percent") << percent;

    QAbstractSeries* stacked = new QStackedBarSeries(this);
    QTest::newRow("stacked") << stacked;
}

void tst_ChartDataSet::addSeries()
{
    QFETCH(QAbstractSeries*, series);
    QVERIFY(m_dataset->series().isEmpty());

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    m_dataset->addSeries(series);

    QCOMPARE(m_dataset->series().count(),1);
    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 1);
    TRY_COMPARE(spy3.count(), 0);
}

void tst_ChartDataSet::removeSeries_data()
{
    addSeries_data();
}

void tst_ChartDataSet::removeSeries()
{
    QFETCH(QAbstractSeries*, series);
    QVERIFY(m_dataset->series().isEmpty());
    m_dataset->addSeries(series);

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    m_dataset->removeSeries(series);

    QCOMPARE(m_dataset->series().count(),0);
    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 0);
    TRY_COMPARE(spy3.count(), 1);

    delete series;
}

void tst_ChartDataSet::addAxis_data()
{
    QTest::addColumn<QAbstractAxis*>("axis");
    QAbstractAxis* value = new QValueAxis(this);
    QAbstractAxis* category = new QCategoryAxis(this);
    QAbstractAxis* barcategory = new QBarCategoryAxis(this);
#ifndef QT_QREAL_IS_FLOAT
    QAbstractAxis* datetime = new QDateTimeAxis(this);
#endif

    QTest::newRow("value") << value;
    QTest::newRow("category") << category;
    QTest::newRow("barcategory") << barcategory;
#ifndef QT_QREAL_IS_FLOAT
    QTest::newRow("datetime") << datetime;
#endif
}

void tst_ChartDataSet::addAxis()
{
    QFETCH(QAbstractAxis*, axis);
    QVERIFY(m_dataset->axes().isEmpty());

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    m_dataset->addAxis(axis,Qt::AlignBottom);

    QCOMPARE(m_dataset->axes().count(),1);
    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 0);
    TRY_COMPARE(spy3.count(), 0);
}

void tst_ChartDataSet::removeAxis_data()
{
    addAxis_data();
}

void tst_ChartDataSet::removeAxis()
{
    QFETCH(QAbstractAxis*, axis);
    QVERIFY(m_dataset->series().isEmpty());
    m_dataset->addAxis(axis,Qt::AlignBottom);

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    m_dataset->removeAxis(axis);

    QCOMPARE(m_dataset->axes().count(),0);
    QCOMPARE(m_dataset->series().count(),0);
    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 0);
    TRY_COMPARE(spy3.count(), 0);

    delete axis;
}

void tst_ChartDataSet::attachAxis_data()
{

    QTest::addColumn<QList<QAbstractSeries*> >("series");
    QTest::addColumn<QList<QAbstractAxis*> >("axis");
    QTest::addColumn<QList<Qt::Alignment> >("alignment");
    QTest::addColumn<QAbstractSeries*>("attachSeries");
    QTest::addColumn<QAbstractAxis*>("attachAxis");
    QTest::addColumn<bool>("success");

    {
     QList<QAbstractSeries*> series;
     QList<QAbstractAxis*> axes;
     QList<Qt::Alignment> alignment;
     QAbstractSeries* line = new QLineSeries(this);
     QAbstractAxis* value1 = new QValueAxis(this);
     QAbstractAxis* value2 = new QValueAxis(this);
     series << line << 0;
     axes << value1 << value2;
     alignment << Qt::AlignBottom << Qt::AlignLeft;
     QTest::newRow("first") << series << axes << alignment << line << value2 << true ;
    }

    {
      QList<QAbstractSeries*> series;
      QList<QAbstractAxis*> axes;
      QList<Qt::Alignment> alignment;
      QAbstractSeries* line = new QLineSeries(this);
      QAbstractAxis* value1 = new QValueAxis(this);
      QAbstractAxis* value2 = new QValueAxis(this);
      series << 0 << line;
      axes << value1 << value2;
      alignment << Qt::AlignBottom << Qt::AlignLeft;
      QTest::newRow("second") << series << axes << alignment << line << value1 << true;
    }

}

void tst_ChartDataSet::attachAxis()
{
    QFETCH(QList<QAbstractSeries*>, series);
    QFETCH(QList<QAbstractAxis*>, axis);
    QFETCH(QList<Qt::Alignment>, alignment);
    QFETCH(QAbstractSeries*, attachSeries);
    QFETCH(QAbstractAxis*, attachAxis);
    QFETCH(bool, success);

    Q_ASSERT(series.count() == axis.count());
    Q_ASSERT(series.count() == alignment.count());

    QVERIFY(m_dataset->series().isEmpty());
    QVERIFY(m_dataset->axes().isEmpty());

    for(int i = 0 ; i < series.count() ; i++){
        if(series[i]) m_dataset->addSeries(series[i]);
        if(axis[i]) m_dataset->addAxis(axis[i],alignment[i]);
        if(series[i] && axis[i]) m_dataset->attachAxis(series[i],axis[i]);
    }

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    QCOMPARE(m_dataset->attachAxis(attachSeries,attachAxis),success);

}

void tst_ChartDataSet::detachAxis_data()
{
    QTest::addColumn<QList<QAbstractSeries*> >("series");
    QTest::addColumn<QList<QAbstractAxis*> >("axis");
    QTest::addColumn<QAbstractSeries*>("detachSeries");
    QTest::addColumn<QAbstractAxis*>("detachAxis");
    QTest::addColumn<bool>("success");

    {
    QList<QAbstractSeries*> series;
    QList<QAbstractAxis*> axes;
    QAbstractSeries* line = new QLineSeries(this);
    QAbstractAxis* value = new QValueAxis(this);
    series << line;
    axes << value;
    QTest::newRow("first") << series << axes << line << value << true;
    }
}

void tst_ChartDataSet::detachAxis()
{
    QFETCH(QList<QAbstractSeries*>, series);
    QFETCH(QList<QAbstractAxis*>, axis);
    QFETCH(QAbstractSeries*, detachSeries);
    QFETCH(QAbstractAxis*, detachAxis);
    QFETCH(bool, success);

    Q_ASSERT(series.count() == axis.count());

    QVERIFY(m_dataset->series().isEmpty());
    QVERIFY(m_dataset->axes().isEmpty());

    for(int i = 0; i < series.count(); i++) {
        if(series[i]) m_dataset->addSeries(series[i]);
        if(axis[i]) m_dataset->addAxis(axis[i],Qt::AlignBottom);
        if(series[i] && axis[i]) m_dataset->attachAxis(series[i],axis[i]);
    }

    QSignalSpy spy0(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)));
    QSignalSpy spy1(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)));
    QSignalSpy spy2(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)));
    QSignalSpy spy3(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)));

    QCOMPARE(m_dataset->detachAxis(detachSeries,detachAxis),success);
}

QTEST_MAIN(tst_ChartDataSet)
#include "tst_chartdataset.moc"
