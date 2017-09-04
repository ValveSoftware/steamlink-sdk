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
#include <QtCharts/QHorizontalBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(QBarSet*)
Q_DECLARE_METATYPE(QAbstractBarSeries::LabelsPosition)

class tst_QHorizontalBarSeries : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qhorizontalbarseries_data();
    void qhorizontalbarseries();
    void type_data();
    void type();
    void append_data();
    void append();
    void remove_data();
    void remove();
    void appendList_data();
    void appendList();
    void count_data();
    void count();
    void barSets_data();
    void barSets();
    void setLabelsVisible_data();
    void setLabelsVisible();
    void setLabelsFormat();
    void setLabelsPosition();
    void setLabelsAngle();
    void mouseclicked_data();
    void mouseclicked();
    void mousehovered_data();
    void mousehovered();
    void clearWithAnimations();
    void mousePressed();
    void mouseReleased();
    void mouseDoubleClicked();

private:
    QHorizontalBarSeries* m_barseries;
    QHorizontalBarSeries* m_barseries_with_sets;

    QList<QBarSet*> m_testSets;

};

void tst_QHorizontalBarSeries::initTestCase()
{
    qRegisterMetaType<QBarSet*>("QBarSet*");
    qRegisterMetaType<QAbstractBarSeries::LabelsPosition>("QAbstractBarSeries::LabelsPosition");
}

void tst_QHorizontalBarSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QHorizontalBarSeries::init()
{
    m_barseries = new QHorizontalBarSeries();
    m_barseries_with_sets = new QHorizontalBarSeries();

    for (int i=0; i<5; i++) {
        m_testSets.append(new QBarSet("testset"));
        m_barseries_with_sets->append(m_testSets.at(i));
    }
}

void tst_QHorizontalBarSeries::cleanup()
{
    foreach (QBarSet* s, m_testSets) {
        m_barseries_with_sets->remove(s);
    }
    m_testSets.clear();

    delete m_barseries;
    m_barseries = 0;
    delete m_barseries_with_sets;
    m_barseries_with_sets = 0;
}

void tst_QHorizontalBarSeries::qhorizontalbarseries_data()
{
}

void tst_QHorizontalBarSeries::qhorizontalbarseries()
{
    QHorizontalBarSeries *barseries = new QHorizontalBarSeries();
    QVERIFY(barseries != 0);
    delete barseries;
}

void tst_QHorizontalBarSeries::type_data()
{

}

void tst_QHorizontalBarSeries::type()
{
    QVERIFY(m_barseries->type() == QAbstractSeries::SeriesTypeHorizontalBar);
}

void tst_QHorizontalBarSeries::append_data()
{
}

void tst_QHorizontalBarSeries::append()
{
    QVERIFY(m_barseries->count() == 0);

    bool ret = false;

    // Try adding barset
    QBarSet *barset = new QBarSet("testset");
    ret = m_barseries->append(barset);

    QVERIFY(ret == true);
    QVERIFY(m_barseries->count() == 1);

    // Try adding another set
    QBarSet *barset2 = new QBarSet("testset2");
    ret = m_barseries->append(barset2);

    QVERIFY(ret == true);
    QVERIFY(m_barseries->count() == 2);

    // Try adding same set again
    ret = m_barseries->append(barset2);
    QVERIFY(ret == false);
    QVERIFY(m_barseries->count() == 2);

    // Try adding null set
    ret = m_barseries->append(0);
    QVERIFY(ret == false);
    QVERIFY(m_barseries->count() == 2);

}

void tst_QHorizontalBarSeries::remove_data()
{
}

void tst_QHorizontalBarSeries::remove()
{
    int count = m_testSets.count();
    QVERIFY(m_barseries_with_sets->count() == count);

    // Try to remove null pointer (should not remove, should not crash)
    bool ret = false;
    ret = m_barseries_with_sets->remove(0);
    QVERIFY(ret == false);
    QVERIFY(m_barseries_with_sets->count() == count);

    // Try to remove invalid pointer (should not remove, should not crash)
    ret = m_barseries_with_sets->remove((QBarSet*) (m_testSets.at(0) + 1) );
    QVERIFY(ret == false);
    QVERIFY(m_barseries_with_sets->count() == count);

    // remove some sets
    ret = m_barseries_with_sets->remove(m_testSets.at(2));
    QVERIFY(ret == true);
    ret = m_barseries_with_sets->remove(m_testSets.at(3));
    QVERIFY(ret == true);
    ret = m_barseries_with_sets->remove(m_testSets.at(4));
    QVERIFY(ret == true);

    QVERIFY(m_barseries_with_sets->count() == 2);

    QList<QBarSet*> verifysets = m_barseries_with_sets->barSets();

    QVERIFY(verifysets.at(0) == m_testSets.at(0));
    QVERIFY(verifysets.at(1) == m_testSets.at(1));

    // Try removing all sets again (should be ok, even if some sets have already been removed)
    ret = false;
    for (int i=0; i<count; i++) {
        ret |= m_barseries_with_sets->remove(m_testSets.at(i));
    }

    QVERIFY(ret == true);
    QVERIFY(m_barseries_with_sets->count() == 0);
}

void tst_QHorizontalBarSeries::appendList_data()
{

}

void tst_QHorizontalBarSeries::appendList()
{
    int count = 5;
    QVERIFY(m_barseries->count() == 0);

    QList<QBarSet*> sets;
    for (int i=0; i<count; i++) {
        sets.append(new QBarSet("testset"));
    }

    // Append new sets (should succeed, count should match the count of sets)
    bool ret = false;
    ret = m_barseries->append(sets);
    QVERIFY(ret == true);
    QVERIFY(m_barseries->count() == count);

    // Append same sets again (should fail, count should remain same)
    ret = m_barseries->append(sets);
    QVERIFY(ret == false);
    QVERIFY(m_barseries->count() == count);

    // Try append empty list (should succeed, but count should remain same)
    QList<QBarSet*> invalidList;
    ret = m_barseries->append(invalidList);
    QVERIFY(ret == true);
    QVERIFY(m_barseries->count() == count);

    // Try append list with one new and one existing set (should fail, count remains same)
    invalidList.append(new QBarSet("ok set"));
    invalidList.append(sets.at(0));
    ret = m_barseries->append(invalidList);
    QVERIFY(ret == false);
    QVERIFY(m_barseries->count() == count);
    delete invalidList.at(0);

    // Try append list with null pointers (should fail, count remains same)
    QList<QBarSet*> invalidList2;
    invalidList2.append(0);
    invalidList2.append(0);
    invalidList2.append(0);
    ret = m_barseries->append(invalidList2);
    QVERIFY(ret == false);
    QVERIFY(m_barseries->count() == count);
}

void tst_QHorizontalBarSeries::count_data()
{

}

void tst_QHorizontalBarSeries::count()
{
    QVERIFY(m_barseries->count() == 0);
    QVERIFY(m_barseries_with_sets->count() == m_testSets.count());
}

void tst_QHorizontalBarSeries::barSets_data()
{

}

void tst_QHorizontalBarSeries::barSets()
{
    QVERIFY(m_barseries->barSets().count() == 0);

    QList<QBarSet*> sets = m_barseries_with_sets->barSets();
    QVERIFY(sets.count() == m_testSets.count());

    for (int i=0; i<m_testSets.count(); i++) {
        QVERIFY(sets.at(i) == m_testSets.at(i));
    }
}

void tst_QHorizontalBarSeries::setLabelsVisible_data()
{

}

void tst_QHorizontalBarSeries::setLabelsVisible()
{
    // labels should be invisible by default
    QVERIFY(m_barseries->isLabelsVisible() == false);
    QVERIFY(m_barseries_with_sets->isLabelsVisible() == false);

    // turn labels to visible
    m_barseries_with_sets->setLabelsVisible(true);
    // TODO: test the signal
    QVERIFY(m_barseries_with_sets->isLabelsVisible() == true);

    // turn labels to invisible
    m_barseries_with_sets->setLabelsVisible(false);
    // TODO: test the signal
    QVERIFY(m_barseries_with_sets->isLabelsVisible() == false);

    // without parameter, should turn labels to visible
    m_barseries_with_sets->setLabelsVisible();
    // TODO: test the signal
    QVERIFY(m_barseries_with_sets->isLabelsVisible() == true);
}

void tst_QHorizontalBarSeries::setLabelsFormat()
{
    QSignalSpy labelsFormatSpy(m_barseries, SIGNAL(labelsFormatChanged(QString)));
    QCOMPARE(m_barseries->labelsFormat(), QString());

    QString format("(@value)");
    m_barseries->setLabelsFormat(format);
    TRY_COMPARE(labelsFormatSpy.count(), 1);
    QList<QVariant> arguments = labelsFormatSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == format);
    QCOMPARE(m_barseries->labelsFormat(), format);

    m_barseries->setLabelsFormat(QString());
    TRY_COMPARE(labelsFormatSpy.count(), 1);
    arguments = labelsFormatSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == QString());
    QCOMPARE(m_barseries->labelsFormat(), QString());
}

void tst_QHorizontalBarSeries::setLabelsPosition()
{
    QSignalSpy labelsPositionSpy(m_barseries,
                             SIGNAL(labelsPositionChanged(QAbstractBarSeries::LabelsPosition)));
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalBarSeries::LabelsCenter);

    m_barseries->setLabelsPosition(QHorizontalBarSeries::LabelsInsideEnd);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    QList<QVariant> arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalBarSeries::LabelsInsideEnd);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalBarSeries::LabelsInsideEnd);

    m_barseries->setLabelsPosition(QHorizontalBarSeries::LabelsInsideBase);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalBarSeries::LabelsInsideBase);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalBarSeries::LabelsInsideBase);

    m_barseries->setLabelsPosition(QHorizontalBarSeries::LabelsOutsideEnd);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalBarSeries::LabelsOutsideEnd);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalBarSeries::LabelsOutsideEnd);

    m_barseries->setLabelsPosition(QHorizontalBarSeries::LabelsCenter);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalBarSeries::LabelsCenter);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalBarSeries::LabelsCenter);
}

void tst_QHorizontalBarSeries::setLabelsAngle()
{
    QSignalSpy labelsAngleSpy(m_barseries,
                             SIGNAL(labelsAngleChanged(qreal)));
    QCOMPARE(m_barseries->labelsAngle(), 0.0);

    m_barseries->setLabelsAngle(55.0);
    TRY_COMPARE(labelsAngleSpy.count(), 1);
    QList<QVariant> arguments = labelsAngleSpy.takeFirst();
    QVERIFY(arguments.at(0).value<qreal>() == 55.0);
    QCOMPARE(m_barseries->labelsAngle(), 55.0);
}

void tst_QHorizontalBarSeries::mouseclicked_data()
{

}

void tst_QHorizontalBarSeries::mouseclicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);

    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesSpy(series,SIGNAL(clicked(int,QBarSet*)));
    QSignalSpy setSpy1(set1, SIGNAL(clicked(int)));
    QSignalSpy setSpy2(set2, SIGNAL(clicked(int)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeX = 10;  // From 0 to 10 because of maximum value in set is 10
    qreal rangeY = 3;   // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = (scaleY / setCount) * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = plotArea.bottom() + (domainMinY - i) * scaleY;
            yPos += setCount*rectHeight/2;
            yPos -= set*rectHeight;

            qreal rectWidth = barSets.at(set)->at(i) * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
        }
    }

//====================================================================================
// barset 1, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(0).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    QList<QVariant> setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 1, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 1, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(4).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);

//====================================================================================
// barset 2, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 2, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(3).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 2, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(5).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);
}

void tst_QHorizontalBarSeries::mousehovered_data()
{

}

void tst_QHorizontalBarSeries::mousehovered()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();
    SKIP_IF_FLAKY_MOUSE_MOVE();

    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);

    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesIndexSpy(series, SIGNAL(hovered(bool, int, QBarSet*)));
    QSignalSpy setIndexSpy1(set1, SIGNAL(hovered(bool, int)));
    QSignalSpy setIndexSpy2(set2, SIGNAL(hovered(bool, int)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    //this is hack since view does not get events otherwise
    view.setMouseTracking(true);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeX = 10;  // From 0 to 10 because of maximum value in set is 10
    qreal rangeY = 3;   // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = (scaleY / setCount) * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = plotArea.bottom() + (domainMinY - i) * scaleY;
            yPos += setCount*rectHeight/2;
            yPos -= set*rectHeight;

            qreal rectWidth = barSets.at(set)->at(i) * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
        }
    }

//=======================================================================
// move mouse to bottom border
    QTest::mouseMove(view.viewport(), QPoint(layout.at(0).center().x(), 300));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10000);
    TRY_COMPARE(seriesIndexSpy.count(), 0);
    TRY_COMPARE(setIndexSpy1.count(), 0);
    TRY_COMPARE(setIndexSpy2.count(), 0);

//=======================================================================
// move mouse on top of set1
    QTest::mouseMove(view.viewport(), layout.at(0).center().toPoint());
    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 0);

    QList<QVariant> seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);

    QList<QVariant> setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);

//=======================================================================
// move mouse from top of set1 to top of set2
    QTest::mouseMove(view.viewport(), layout.at(1).center().toPoint());
    TRY_COMPARE(seriesIndexSpy.count(), 2);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should leave set1
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);

    setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);

    // should enter set2
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);

//=======================================================================
// move mouse from top of set2 to background
    QTest::mouseMove(view.viewport(), QPoint(0, layout.at(0).center().y()));
    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 0);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should leave set2
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);

//=======================================================================
// move mouse on top of set1, bar0 to check the index (hover into set1)
    QTest::mouseMove(view.viewport(), layout.at(0).center().toPoint());

    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 0);

    //should enter set1, bar0
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 0);

    setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 0);

//=======================================================================
// move mouse on top of set2, bar0 to check the index (hover out set1,
// hover in set1)
    QTest::mouseMove(view.viewport(), layout.at(1).center().toPoint());

    TRY_COMPARE(seriesIndexSpy.count(), 2);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should leave set1, bar0
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 0);

    setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 0);

    // should enter set2, bar0
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 0);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 0);

//=======================================================================
// move mouse on top of set1, bar1 to check the index (hover out set 2,
// hover in set1)
    QTest::mouseMove(view.viewport(), layout.at(2).center().toPoint());

    TRY_COMPARE(seriesIndexSpy.count(), 2);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should leave set2, bar0
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 0);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 0);

    // should enter set1, bar1
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 1);

    setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 1);

//=======================================================================
// move mouse on top of view between set1 and set2 to check the index
// (hover out set1)
    QTest::mouseMove(view.viewport(), QPoint(0, (layout.at(2).top() + layout.at(3).bottom()) / 2));

    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 0);

    // should leave set1, bar1
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set1);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 1);

    setIndexSpyArg = setIndexSpy1.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 1);

//=======================================================================
// move mouse on top of set2, bar1 to check the index (hover in set2)
    QTest::mouseMove(view.viewport(), layout.at(3).center().toPoint());

    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 0);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should enter set2, bar1
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == true);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 1);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == true);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 1);

//=======================================================================
// move mouse on top of the view between bar1 and bar2 to check the index
//(hover out set2)
    QTest::mouseMove(view.viewport(), QPoint(0, (layout.at(3).top() + layout.at(3).bottom()) / 2));

    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 0);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    // should leave set2, bar1
    seriesIndexSpyArg = seriesIndexSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesIndexSpyArg.at(2)), set2);
    QVERIFY(seriesIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(seriesIndexSpyArg.at(0).toBool() == false);
    QVERIFY(seriesIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(seriesIndexSpyArg.at(1).toInt() == 1);

    setIndexSpyArg = setIndexSpy2.takeFirst();
    QVERIFY(setIndexSpyArg.at(0).type() == QVariant::Bool);
    QVERIFY(setIndexSpyArg.at(0).toBool() == false);
    QVERIFY(setIndexSpyArg.at(1).type() == QVariant::Int);
    QVERIFY(setIndexSpyArg.at(1).toInt() == 1);
}

void tst_QHorizontalBarSeries::clearWithAnimations()
{
    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->setAnimationOptions(QChart::SeriesAnimations);
    view.chart()->addSeries(series);
    view.show();

    series->clear();
}

void tst_QHorizontalBarSeries::mousePressed()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);
    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesSpy(series,SIGNAL(pressed(int,QBarSet*)));
    QSignalSpy setSpy1(set1, SIGNAL(pressed(int)));
    QSignalSpy setSpy2(set2, SIGNAL(pressed(int)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeX = 10;  // From 0 to 10 because of maximum value in set is 10
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = (scaleY / setCount) * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = plotArea.bottom() + (domainMinY - i) * scaleY;
            yPos += setCount*rectHeight/2;
            yPos -= set*rectHeight;

            qreal rectWidth = barSets.at(set)->at(i) * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
        }
    }

//====================================================================================
// barset 1, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(0).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    QList<QVariant> setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 1, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 1, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(4).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);

//====================================================================================
// barset 2, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 2, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(3).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 2, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(5).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);
}

void tst_QHorizontalBarSeries::mouseReleased()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);
    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesSpy(series,SIGNAL(released(int,QBarSet*)));
    QSignalSpy setSpy1(set1, SIGNAL(released(int)));
    QSignalSpy setSpy2(set2, SIGNAL(released(int)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeX = 10;  // From 0 to 10 because of maximum value in set is 10
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = (scaleY / setCount) * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = plotArea.bottom() + (domainMinY - i) * scaleY;
            yPos += setCount*rectHeight/2;
            yPos -= set*rectHeight;

            qreal rectWidth = barSets.at(set)->at(i) * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
        }
    }

//====================================================================================
// barset 1, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(0).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    QList<QVariant> setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 1, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 1, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(4).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);

//====================================================================================
// barset 2, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 2, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(3).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 2, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(5).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

    setSpyArg = setSpy2.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 2);
}

void tst_QHorizontalBarSeries::mouseDoubleClicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalBarSeries* series = new QHorizontalBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);
    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesSpy(series,SIGNAL(doubleClicked(int,QBarSet*)));
    QSignalSpy setSpy1(set1, SIGNAL(doubleClicked(int)));
    QSignalSpy setSpy2(set2, SIGNAL(doubleClicked(int)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeX = 10;  // From 0 to 10 because of maximum value in set is 10
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = (scaleY / setCount) * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = plotArea.bottom() + (domainMinY - i) * scaleY;
            yPos += setCount*rectHeight/2;
            yPos -= set*rectHeight;

            qreal rectWidth = barSets.at(set)->at(i) * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
        }
    }

    // barset 1, bar 0
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, 0, layout.at(0).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

    QList<QVariant> setSpyArg = setSpy1.takeFirst();
    QVERIFY(setSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(setSpyArg.at(0).toInt() == 0);
}
QTEST_MAIN(tst_QHorizontalBarSeries)

#include "tst_qhorizontalbarseries.moc"

