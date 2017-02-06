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
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QChartView>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

class tst_QBarSet : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qbarset_data();
    void qbarset();
    void label_data();
    void label();
    void append_data();
    void append();
    void appendOperator_data();
    void appendOperator();
    void insert_data();
    void insert();
    void remove_data();
    void remove();
    void replace_data();
    void replace();
    void at_data();
    void at();
    void atOperator_data();
    void atOperator();
    void count_data();
    void count();
    void sum_data();
    void sum();
    void customize();

private:
    QBarSet* m_barset;
};

void tst_QBarSet::initTestCase()
{
}

void tst_QBarSet::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QBarSet::init()
{
     m_barset = new QBarSet(QString("label"));
}

void tst_QBarSet::cleanup()
{
    delete m_barset;
    m_barset = 0;
}

void tst_QBarSet::qbarset_data()
{
}

void tst_QBarSet::qbarset()
{
    QBarSet barset(QString("label"));
    QCOMPARE(barset.label(), QString("label"));
    QCOMPARE(barset.count(), 0);
    QVERIFY(qFuzzyCompare(barset.sum(), 0));
}

void tst_QBarSet::label_data()
{
    QTest::addColumn<QString> ("label");
    QTest::addColumn<QString> ("result");
    QTest::newRow("label0") << QString("label0") << QString("label0");
    QTest::newRow("label1") << QString("label1") << QString("label1");
}

void tst_QBarSet::label()
{
    QFETCH(QString, label);
    QFETCH(QString, result);

    QSignalSpy labelSpy(m_barset,SIGNAL(labelChanged()));
    m_barset->setLabel(label);
    QCOMPARE(m_barset->label(), result);
    QVERIFY(labelSpy.count() == 1);
}

void tst_QBarSet::append_data()
{
    QTest::addColumn<int> ("count");
    QTest::newRow("0") << 0;
    QTest::newRow("5") << 5;
    QTest::newRow("100") << 100;
    QTest::newRow("1000") << 1000;
}

void tst_QBarSet::append()
{
    QFETCH(int, count);

    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    QSignalSpy valueSpy(m_barset, SIGNAL(valuesAdded(int,int)));

    qreal sum(0.0);
    qreal value(0.0);

    for (int i=0; i<count; i++) {
        m_barset->append(value);
        QCOMPARE(m_barset->at(i), value);
        sum += value;
        value += 1.0;
    }

    QCOMPARE(m_barset->count(), count);
    QVERIFY(qFuzzyCompare(m_barset->sum(), sum));

    QCOMPARE(valueSpy.count(), count);
}

void tst_QBarSet::appendOperator_data()
{
    append_data();
}

void tst_QBarSet::appendOperator()
{
    QFETCH(int, count);

    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    QSignalSpy valueSpy(m_barset,SIGNAL(valuesAdded(int,int)));

    qreal sum(0.0);
    qreal value(0.0);

    for (int i=0; i<count; i++) {
        *m_barset << value;
        QCOMPARE(m_barset->at(i), value);
        sum += value;
        value += 1.0;
    }

    QCOMPARE(m_barset->count(), count);
    QVERIFY(qFuzzyCompare(m_barset->sum(), sum));
    QCOMPARE(valueSpy.count(), count);
}

void tst_QBarSet::insert_data()
{
}

void tst_QBarSet::insert()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));
    QSignalSpy valueSpy(m_barset,SIGNAL(valuesAdded(int,int)));

    m_barset->insert(0, 1.0);       // 1.0
    QCOMPARE(m_barset->at(0), 1.0);
    QCOMPARE(m_barset->count(), 1);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)1.0));

    m_barset->insert(0, 2.0);       // 2.0 1.0
    QCOMPARE(m_barset->at(0), 2.0);
    QCOMPARE(m_barset->at(1), 1.0);
    QCOMPARE(m_barset->count(), 2);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)3.0));

    m_barset->insert(1, 3.0);       // 2.0 3.0 1.0
    QCOMPARE(m_barset->at(1), 3.0);
    QCOMPARE(m_barset->at(0), 2.0);
    QCOMPARE(m_barset->at(2), 1.0);
    QCOMPARE(m_barset->count(), 3);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)6.0));
    QCOMPARE(valueSpy.count(), 3);
}

void tst_QBarSet::remove_data()
{
}

void tst_QBarSet::remove()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    QSignalSpy valueSpy(m_barset,SIGNAL(valuesRemoved(int,int)));

    m_barset->append(1.0);
    m_barset->append(2.0);
    m_barset->append(3.0);
    m_barset->append(4.0);

    QCOMPARE(m_barset->count(), 4);
    QCOMPARE(m_barset->sum(), 10.0);

    // Remove middle
    m_barset->remove(2);                // 1.0 2.0 4.0
    QCOMPARE(m_barset->at(0), 1.0);
    QCOMPARE(m_barset->at(1), 2.0);
    QCOMPARE(m_barset->at(2), 4.0);
    QCOMPARE(m_barset->count(), 3);
    QCOMPARE(m_barset->sum(), 7.0);
    QCOMPARE(valueSpy.count(), 1);

    QList<QVariant> valueSpyArg = valueSpy.takeFirst();
    // Verify index of removed signal
    QVERIFY(valueSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(0).toInt() == 2);
    // Verify count of removed signal
    QVERIFY(valueSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(1).toInt() == 1);

    // Remove first
    m_barset->remove(0);                // 2.0 4.0
    QCOMPARE(m_barset->at(0), 2.0);
    QCOMPARE(m_barset->at(1), 4.0);
    QCOMPARE(m_barset->count(), 2);
    QCOMPARE(m_barset->sum(), 6.0);

    QCOMPARE(valueSpy.count(), 1);
    valueSpyArg = valueSpy.takeFirst();
    // Verify index of removed signal
    QVERIFY(valueSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(0).toInt() == 0);
    // Verify count of removed signal
    QVERIFY(valueSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(1).toInt() == 1);


    // Illegal indexes
    m_barset->remove(4);
    QCOMPARE(m_barset->count(), 2);
    QCOMPARE(m_barset->sum(), 6.0);
    m_barset->remove(-1);
    QCOMPARE(m_barset->count(), 2);
    QCOMPARE(m_barset->sum(), 6.0);

    // nothing removed, no signals should be emitted
    QCOMPARE(valueSpy.count(), 0);

    // Remove more items than list has
    m_barset->remove(0,312);
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    QCOMPARE(valueSpy.count(), 1);
    valueSpyArg = valueSpy.takeFirst();

    // Verify index of removed signal
    QVERIFY(valueSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(0).toInt() == 0);
    // Verify count of removed signal (expect 2 values removed, because list had only 2 items)
    QVERIFY(valueSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(valueSpyArg.at(1).toInt() == 2);
}

void tst_QBarSet::replace_data()
{

}

void tst_QBarSet::replace()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));
    QSignalSpy valueSpy(m_barset,SIGNAL(valueChanged(int)));

    m_barset->append(1.0);
    m_barset->append(2.0);
    m_barset->append(3.0);
    m_barset->append(4.0);

    QCOMPARE(m_barset->count(), 4);
    QCOMPARE(m_barset->sum(), 10.0);

    // Replace first
    m_barset->replace(0, 5.0);          // 5.0 2.0 3.0 4.0
    QCOMPARE(m_barset->count(), 4);
    QCOMPARE(m_barset->sum(), 14.0);
    QCOMPARE(m_barset->at(0), 5.0);

    // Replace last
    m_barset->replace(3, 6.0);
    QCOMPARE(m_barset->count(), 4);     // 5.0 2.0 3.0 6.0
    QCOMPARE(m_barset->sum(), 16.0);
    QCOMPARE(m_barset->at(0), 5.0);
    QCOMPARE(m_barset->at(1), 2.0);
    QCOMPARE(m_barset->at(2), 3.0);
    QCOMPARE(m_barset->at(3), 6.0);

    // Illegal indexes
    m_barset->replace(4, 6.0);
    QCOMPARE(m_barset->count(), 4);     // 5.0 2.0 3.0 6.0
    QCOMPARE(m_barset->sum(), 16.0);
    m_barset->replace(-1, 6.0);
    QCOMPARE(m_barset->count(), 4);     // 5.0 2.0 3.0 6.0
    QCOMPARE(m_barset->sum(), 16.0);
    m_barset->replace(4, 1.0);
    QCOMPARE(m_barset->count(), 4);     // 5.0 2.0 3.0 6.0
    QCOMPARE(m_barset->sum(), 16.0);
    m_barset->replace(-1, 1.0);
    QCOMPARE(m_barset->count(), 4);     // 5.0 2.0 3.0 6.0
    QCOMPARE(m_barset->sum(), 16.0);

    QVERIFY(valueSpy.count() == 2);
}

void tst_QBarSet::at_data()
{

}

void tst_QBarSet::at()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    m_barset->append(1.0);
    m_barset->append(2.0);
    m_barset->append(3.0);
    m_barset->append(4.0);

    QCOMPARE(m_barset->at(0), 1.0);
    QCOMPARE(m_barset->at(1), 2.0);
    QCOMPARE(m_barset->at(2), 3.0);
    QCOMPARE(m_barset->at(3), 4.0);
}

void tst_QBarSet::atOperator_data()
{

}

void tst_QBarSet::atOperator()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    m_barset->append(1.0);
    m_barset->append(2.0);
    m_barset->append(3.0);
    m_barset->append(4.0);

    QCOMPARE(m_barset->operator [](0), 1.0);
    QCOMPARE(m_barset->operator [](1), 2.0);
    QCOMPARE(m_barset->operator [](2), 3.0);
    QCOMPARE(m_barset->operator [](3), 4.0);
}

void tst_QBarSet::count_data()
{

}

void tst_QBarSet::count()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    m_barset->append(1.0);
    QCOMPARE(m_barset->count(),1);
    m_barset->append(2.0);
    QCOMPARE(m_barset->count(),2);
    m_barset->append(3.0);
    QCOMPARE(m_barset->count(),3);
    m_barset->append(4.0);
    QCOMPARE(m_barset->count(),4);
}

void tst_QBarSet::sum_data()
{

}

void tst_QBarSet::sum()
{
    QCOMPARE(m_barset->count(), 0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), 0));

    m_barset->append(1.0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)1.0));
    m_barset->append(2.0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)3.0));
    m_barset->append(3.0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)6.0));
    m_barset->append(4.0);
    QVERIFY(qFuzzyCompare(m_barset->sum(), (qreal)10.0));
}

void tst_QBarSet::customize()
{
    // Create sets
    QBarSet *set1 = new QBarSet("set1");
    QBarSet *set2 = new QBarSet("set2");

    // Append set1 to series
    QBarSeries *series = new QBarSeries();
    bool success = series->append(set1);
    QVERIFY(success);

    // Add series to the chart
    QChartView view(new QChart());
    view.resize(200, 200);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Test adding data to the sets
    *set1 << 1 << 2 << 1 << 3;
    *set2 << 2 << 1 << 3 << 1;

    // Remove sets from series
    series->take(set1);
    series->take(set2);

    // Test pen
    QVERIFY(set1->pen() != QPen());
    QVERIFY(set2->pen() == QPen());
    QPen pen(QColor(128,128,128,128));
    set1->setPen(pen);

    // Add sets back to series
    series->append(set1);
    series->append(set2);

    QVERIFY(set1->pen() == pen);      // Should be customized
    QVERIFY(set2->pen() != QPen());   // Should be decorated by theme

    // Remove sets from series
    series->take(set1);
    series->take(set2);

    // Test brush
    set2->setBrush(QBrush());
    QVERIFY(set1->brush() != QBrush());
    QVERIFY(set2->brush() == QBrush());
    QBrush brush(QColor(128,128,128,128));
    set1->setBrush(brush);

    // Add sets back to series
    series->append(set1);
    series->append(set2);

    QVERIFY(set1->brush() == brush);    // Should be customized
    QVERIFY(set2->brush() == QBrush()); // Setting empty brush doesn't reset to theme brush

    // Remove sets from series
    series->take(set1);
    series->take(set2);

    // Test label brush
    set2->setLabelBrush(QBrush());
    QVERIFY(set1->labelBrush() != QBrush());
    QVERIFY(set2->labelBrush() == QBrush());
    set1->setLabelBrush(brush);

    series->append(set1);
    series->append(set2);
    QVERIFY(set1->labelBrush() == brush);    // Should be customized
    QVERIFY(set2->labelBrush() == QBrush()); // Setting empty brush doesn't reset to theme brush

    // Test label font
    // Note: QFont empty constructor creates font with application's default font, so the font may or may not be the
    // same for the set added to the series  (depending on the QChart's theme configuration)
    QVERIFY(set1->labelFont() != QFont() || set1->labelFont() == QFont());
    QVERIFY(set2->labelFont() == QFont());
    QFont font;
    font.setBold(true);
    font.setItalic(true);
    set1->setLabelFont(font);
    QVERIFY(set1->labelFont() == font);
    QVERIFY(set2->labelFont() == QFont());

    // Test adding data to the sets
    *set1 << 1 << 2 << 1 << 3;
    *set2 << 2 << 1 << 3 << 1;

}




QTEST_MAIN(tst_QBarSet)

#include "tst_qbarset.moc"

