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
#include <QtCharts/QHorizontalPercentBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(QBarSet*)
Q_DECLARE_METATYPE(QAbstractBarSeries::LabelsPosition)

class tst_QHorizontalPercentBarSeries : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qhorizontalpercentbarseries_data();
    void qhorizontalpercentbarseries();
    void type_data();
    void type();
    void setLabelsFormat();
    void setLabelsPosition();
    void setLabelsAngle();
    void mouseclicked_data();
    void mouseclicked();
    void mousehovered_data();
    void mousehovered();
    void zeroValuesInSeries();
    void mousePressed();
    void mouseReleased();
    void mouseDoubleClicked();

private:
    QHorizontalPercentBarSeries* m_barseries;
};

void tst_QHorizontalPercentBarSeries::initTestCase()
{
    qRegisterMetaType<QBarSet*>("QBarSet*");
    qRegisterMetaType<QAbstractBarSeries::LabelsPosition>("QAbstractBarSeries::LabelsPosition");
}

void tst_QHorizontalPercentBarSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QHorizontalPercentBarSeries::init()
{
    m_barseries = new QHorizontalPercentBarSeries();
}

void tst_QHorizontalPercentBarSeries::cleanup()
{
    delete m_barseries;
    m_barseries = 0;
}

void tst_QHorizontalPercentBarSeries::qhorizontalpercentbarseries_data()
{
}

void tst_QHorizontalPercentBarSeries::qhorizontalpercentbarseries()
{
    QHorizontalPercentBarSeries *barseries = new QHorizontalPercentBarSeries();
    QVERIFY(barseries != 0);
    delete barseries;
}

void tst_QHorizontalPercentBarSeries::type_data()
{

}

void tst_QHorizontalPercentBarSeries::type()
{
    QVERIFY(m_barseries->type() == QAbstractSeries::SeriesTypeHorizontalPercentBar);
}

void tst_QHorizontalPercentBarSeries::setLabelsFormat()
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

void tst_QHorizontalPercentBarSeries::setLabelsPosition()
{
    QSignalSpy labelsPositionSpy(m_barseries,
                             SIGNAL(labelsPositionChanged(QAbstractBarSeries::LabelsPosition)));
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalPercentBarSeries::LabelsCenter);

    m_barseries->setLabelsPosition(QHorizontalPercentBarSeries::LabelsInsideEnd);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    QList<QVariant> arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalPercentBarSeries::LabelsInsideEnd);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalPercentBarSeries::LabelsInsideEnd);

    m_barseries->setLabelsPosition(QHorizontalPercentBarSeries::LabelsInsideBase);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalPercentBarSeries::LabelsInsideBase);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalPercentBarSeries::LabelsInsideBase);

    m_barseries->setLabelsPosition(QHorizontalPercentBarSeries::LabelsOutsideEnd);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalPercentBarSeries::LabelsOutsideEnd);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalPercentBarSeries::LabelsOutsideEnd);

    m_barseries->setLabelsPosition(QHorizontalPercentBarSeries::LabelsCenter);
    TRY_COMPARE(labelsPositionSpy.count(), 1);
    arguments = labelsPositionSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QAbstractBarSeries::LabelsPosition>()
            == QHorizontalPercentBarSeries::LabelsCenter);
    QCOMPARE(m_barseries->labelsPosition(), QHorizontalPercentBarSeries::LabelsCenter);
}

void tst_QHorizontalPercentBarSeries::setLabelsAngle()
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

void tst_QHorizontalPercentBarSeries::mouseclicked_data()
{

}

void tst_QHorizontalPercentBarSeries::mouseclicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalPercentBarSeries* series = new QHorizontalPercentBarSeries();

    QBarSet* set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    series->append(set1);

    QBarSet* set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    series->append(set2);

    QList<QBarSet*> barSets = series->barSets();

    QSignalSpy seriesSpy(series,SIGNAL(clicked(int,QBarSet*)));

    QChartView view(new QChart());
    view.resize(400,300);
    view.chart()->addSeries(series);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for bars
    QRectF plotArea = view.chart()->plotArea();
    qreal width = plotArea.width();
    qreal height = plotArea.height();
    qreal rangeY = 3;   // 3 values per set
    qreal rangeX = 100;  // From 0 to 100 because of scaling to 100%
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = scaleY * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal colSum = 20; // Sum of values in column (10 + 10 in our test case)
        qreal percentage = (100 / colSum);
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = (domainMinY +0.5 -i) * scaleY + plotArea.bottom() - rectHeight/2;
            qreal rectWidth = barSets.at(set)->at(i) * percentage * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
            xPos += rectWidth;
        }
    }

//====================================================================================
// barset 1, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(0).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 1, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 1, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(4).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set1);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);

//====================================================================================
// barset 2, bar 0
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 0);

//====================================================================================
// barset 2, bar 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(3).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 1);

//====================================================================================
// barset 2, bar 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.at(5).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);

    seriesSpyArg = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QBarSet*>(seriesSpyArg.at(1)), set2);
    QVERIFY(seriesSpyArg.at(0).type() == QVariant::Int);
    QVERIFY(seriesSpyArg.at(0).toInt() == 2);
}

void tst_QHorizontalPercentBarSeries::mousehovered_data()
{

}

void tst_QHorizontalPercentBarSeries::mousehovered()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();
    SKIP_IF_FLAKY_MOUSE_MOVE();

    QHorizontalPercentBarSeries* series = new QHorizontalPercentBarSeries();

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
    qreal rangeY = 3;   // 3 values per set
    qreal rangeX = 100;  // From 0 to 100 because of scaling to 100%
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = scaleY * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal colSum = 20; // Sum of values in column (10 + 10 in our test case)
        qreal percentage = (100 / colSum);
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = (domainMinY +0.5 -i) * scaleY + plotArea.bottom() - rectHeight/2;
            qreal rectWidth = barSets.at(set)->at(i) * percentage * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
            xPos += rectWidth;
        }
    }

//=======================================================================
// move mouse to left border
    QTest::mouseMove(view.viewport(), QPoint(0, layout.at(4).center().y()));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10000);
    TRY_COMPARE(seriesIndexSpy.count(), 0);

//=======================================================================
// move mouse on top of set1
    QTest::mouseMove(view.viewport(), layout.at(4).center().toPoint());
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
    QTest::mouseMove(view.viewport(), layout.at(5).center().toPoint());
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
    QTest::mouseMove(view.viewport(), QPoint(layout.at(5).center().x(), 0));
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
// hover in set2)
    QTest::mouseMove(view.viewport(), layout.at(1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10000);

    TRY_COMPARE(seriesIndexSpy.count(), 2);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    //should leave set1, bar0
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

    //should enter set2, bar0
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
// move mouse on top of set1, bar1 to check the index (hover out set2,
// hover in set1)
    QTest::mouseMove(view.viewport(), layout.at(2).center().toPoint());

    TRY_COMPARE(seriesIndexSpy.count(), 2);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    //should leave set2, bar0
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

    //should enter set1, bar1
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
// move mouse between set1 and set2 (hover out set1)
    QTest::mouseMove(view.viewport(), QPoint(layout.at(3).left(),
                                             (layout.at(3).top() + layout.at(4).bottom()) / 2));

    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 1);
    TRY_COMPARE(setIndexSpy2.count(), 0);

    //should leave set1, bar1
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

    //should enter set2, bar1
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
// move mouse between set1 and set2 (hover out set2)
    QTest::mouseMove(view.viewport(), QPoint(layout.at(3).left(),
                                             (layout.at(3).top() + layout.at(4).bottom()) / 2));

    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);
    TRY_COMPARE(seriesIndexSpy.count(), 1);
    TRY_COMPARE(setIndexSpy1.count(), 0);
    TRY_COMPARE(setIndexSpy2.count(), 1);

    //should leave set1, bar1
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

void tst_QHorizontalPercentBarSeries::zeroValuesInSeries()
{
    QHorizontalPercentBarSeries *series = new QHorizontalPercentBarSeries();
    QBarSet *set1 = new QBarSet(QString("set 1"));
    *set1 << 100 << 0.0 << 10;
    series->append(set1);

    QBarSet *set2 = new QBarSet(QString("set 2"));
    *set2 << 0.0 << 0.0 << 70;
    series->append(set2);

    QChartView view(new QChart());
    view.chart()->addSeries(series);
    view.chart()->createDefaultAxes();
    view.show();

    QTest::qWaitForWindowShown(&view);
}


void tst_QHorizontalPercentBarSeries::mousePressed()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalPercentBarSeries* series = new QHorizontalPercentBarSeries();

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
    qreal rangeX = 100;  // From 0 to 100 because of scaling to 100%
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = scaleY * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal colSum = 20; // Sum of values in column (10 + 10 in our test case)
        qreal percentage = (100 / colSum);
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = (domainMinY +0.5 -i) * scaleY + plotArea.bottom() - rectHeight/2;
            qreal rectWidth = barSets.at(set)->at(i) * percentage * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
            xPos += rectWidth;
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

void tst_QHorizontalPercentBarSeries::mouseReleased()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalPercentBarSeries* series = new QHorizontalPercentBarSeries();

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
    qreal rangeX = 100;  // From 0 to 100 because of scaling to 100%
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = scaleY * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal colSum = 20; // Sum of values in column (10 + 10 in our test case)
        qreal percentage = (100 / colSum);
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = (domainMinY +0.5 -i) * scaleY + plotArea.bottom() - rectHeight/2;
            qreal rectWidth = barSets.at(set)->at(i) * percentage * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
            xPos += rectWidth;
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

void tst_QHorizontalPercentBarSeries::mouseDoubleClicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QHorizontalPercentBarSeries* series = new QHorizontalPercentBarSeries();

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
    qreal rangeX = 100;  // From 0 to 100 because of scaling to 100%
    qreal rangeY = 3; // 3 values per set
    qreal scaleY = (height / rangeY);
    qreal scaleX = (width / rangeX);

    qreal setCount = series->count();
    qreal domainMinY = -0.5;    // These come from internal domain used by barseries.
    qreal domainMinX = 0;       // No access to domain from outside, so use hard coded values.
    qreal rectHeight = scaleY * series->barWidth(); // On horizontal chart barWidth of the barseries means height of the rect.

    QVector<QRectF> layout;

    // 3 = count of values in set
    // Note that rects in this vector will be interleaved (set1 bar0, set2 bar0, set1 bar1, set2 bar1, etc.)
    for (int i = 0; i < 3; i++) {
        qreal colSum = 20; // Sum of values in column (10 + 10 in our test case)
        qreal percentage = (100 / colSum);
        qreal xPos = -scaleX * domainMinX + plotArea.left();
        for (int set = 0; set < setCount; set++) {
            qreal yPos = (domainMinY +0.5 -i) * scaleY + plotArea.bottom() - rectHeight/2;
            qreal rectWidth = barSets.at(set)->at(i) * percentage * scaleX;
            QRectF rect(xPos, yPos - rectHeight, rectWidth, rectHeight);
            layout.append(rect);
            xPos += rectWidth;
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

QTEST_MAIN(tst_QHorizontalPercentBarSeries)

#include "tst_qhorizontalpercentbarseries.moc"

