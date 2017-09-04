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
#include <QtTest/QtTest>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(QCandlestickSet *)
Q_DECLARE_METATYPE(QList<QCandlestickSet *>)

class tst_QCandlestickSeries : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void qCandlestickSeries();
    void append();
    void remove();
    void appendList();
    void removeList();
    void insert();
    void take();
    void clear();
    void sets();
    void count();
    void type();
    void maximumColumnWidth_data();
    void maximumColumnWidth();
    void minimumColumnWidth_data();
    void minimumColumnWidth();
    void bodyWidth_data();
    void bodyWidth();
    void bodyOutlineVisible();
    void capsWidth_data();
    void capsWidth();
    void capsVisible();
    void increasingColor();
    void decreasingColor();
    void brush();
    void pen();
    void mouseClicked();
    void mouseHovered();
    void mousePressed();
    void mouseReleased();
    void mouseDoubleClicked();

private:
    QCandlestickSeries *m_series;
    QList<QCandlestickSet *> m_sets;
};

void tst_QCandlestickSeries::initTestCase()
{
    qRegisterMetaType<QCandlestickSet *>("QCandlestickSet *");
    qRegisterMetaType<QList<QCandlestickSet *>>("QList<QCandlestickSet *>");
}

void tst_QCandlestickSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QCandlestickSeries::init()
{
    m_series = new QCandlestickSeries();
    m_series->setMaximumColumnWidth(5432.1);
    m_series->setMinimumColumnWidth(2.0);
    m_series->setBodyWidth(0.99);
    m_series->setCapsWidth(0.99);

    for (int i = 0; i < 5; ++i) {
        qreal timestamp = QDateTime::currentMSecsSinceEpoch() + i * 1000000;

        QCandlestickSet *set = new QCandlestickSet(timestamp);
        set->setOpen(4);
        set->setHigh(4);
        set->setLow(1);
        set->setClose(1);

        m_sets.append(set);
    }
}

void tst_QCandlestickSeries::cleanup()
{
    foreach (QCandlestickSet *set, m_sets) {
        m_series->remove(set);
        m_sets.removeAll(set);
        delete set;
    }

    delete m_series;
    m_series = nullptr;
}

void tst_QCandlestickSeries::qCandlestickSeries()
{
    QCandlestickSeries *series = new QCandlestickSeries();

    QVERIFY(series != nullptr);

    delete series;
    series = nullptr;
}

void tst_QCandlestickSeries::append()
{
    QCOMPARE(m_series->count(), 0);

    // Try adding set
    QCandlestickSet *set1 = new QCandlestickSet(1234.0);
    QVERIFY(m_series->append(set1));
    QCOMPARE(m_series->count(), 1);

    // Try adding another set
    QCandlestickSet *set2 = new QCandlestickSet(2345.0);
    QVERIFY(m_series->append(set2));
    QCOMPARE(m_series->count(), 2);

    // Try adding same set again
    QVERIFY(!m_series->append(set2));
    QCOMPARE(m_series->count(), 2);

    // Try adding null set
    QVERIFY(!m_series->append(nullptr));
    QCOMPARE(m_series->count(), 2);
}

void tst_QCandlestickSeries::remove()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->count(), m_sets.count());

    // Try to remove null pointer (should not remove, should not crash)
    QVERIFY(!m_series->remove(nullptr));
    QCOMPARE(m_series->count(), m_sets.count());

    // Try to remove invalid pointer (should not remove, should not crash)
    QVERIFY(!m_series->remove((QCandlestickSet *)(m_sets.at(0) + 1)));
    QCOMPARE(m_series->count(), m_sets.count());

    // Remove some sets
    const int removeCount = 3;
    for (int i = 0; i < removeCount; ++i)
        QVERIFY(m_series->remove(m_sets.at(i)));
    QCOMPARE(m_series->count(), m_sets.count() - removeCount);

    for (int i = removeCount; i < m_sets.count(); ++i)
        QCOMPARE(m_series->sets().at(i - removeCount), m_sets.at(i));

    // Try removing all sets again (should be ok, even if some sets have already been removed)
    for (int i = 0; i < m_sets.count(); ++i)
        m_series->remove(m_sets.at(i));
    QCOMPARE(m_series->count(), 0);
}

void tst_QCandlestickSeries::appendList()
{
    QCOMPARE(m_series->count(), 0);

    // Append new sets (should succeed, count should match the count of sets)
    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_series->count());

    // Append same sets again (should fail, count should remain same)
    QVERIFY(!m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_series->count());

    // Try append empty list (should succeed, but count should remain same)
    QList<QCandlestickSet *> invalidList;
    QVERIFY(m_series->append(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());

    // Try append list with one new and one existing set (should fail, count remains same)
    invalidList.append(new QCandlestickSet());
    invalidList.append(m_sets.at(0));
    QVERIFY(!m_series->append(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());
    delete invalidList.at(0);
    invalidList.clear();

    // Try append list with null pointers (should fail, count remains same)
    QVERIFY(invalidList.isEmpty());
    invalidList.append(nullptr);
    invalidList.append(nullptr);
    invalidList.append(nullptr);
    QVERIFY(!m_series->append(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());
}

void tst_QCandlestickSeries::removeList()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->count(), m_sets.count());

    // Try remove empty list (should fail, but count should remain same)
    QList<QCandlestickSet *> invalidList;
    QVERIFY(!m_series->remove(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());

    // Try remove list with one new and one existing set (should fail, count remains same)
    invalidList.append(new QCandlestickSet());
    invalidList.append(m_sets.at(0));
    QVERIFY(!m_series->remove(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());
    delete invalidList.at(0);
    invalidList.clear();

    // Try remove list with null pointers (should fail, count remains same)
    QVERIFY(invalidList.isEmpty());
    invalidList.append(nullptr);
    invalidList.append(nullptr);
    invalidList.append(nullptr);
    QVERIFY(!m_series->remove(invalidList));
    QCOMPARE(m_series->count(), m_sets.count());

    // Remove all sets (should succeed, count should be zero)
    QVERIFY(m_series->remove(m_sets));
    QCOMPARE(m_series->count(), 0);

    // Remove same sets again (should fail, count should remain zero)
    QVERIFY(!m_series->remove(m_sets));
    QCOMPARE(m_series->count(), 0);
}

void tst_QCandlestickSeries::insert()
{
    QCOMPARE(m_series->count(), 0);

    QSignalSpy countSpy(m_series, SIGNAL(countChanged()));
    QSignalSpy addedSpy(m_series, SIGNAL(candlestickSetsAdded(QList<QCandlestickSet *>)));

    for (int i = 0; i < m_sets.count(); ++i) {
        QCandlestickSet *set = m_sets.at(i);
        QVERIFY(m_series->insert(0, set));
        QCOMPARE(m_series->count(), i + 1);
        QTRY_COMPARE(countSpy.count(), i + 1);
        QTRY_COMPARE(addedSpy.count(), i + 1);

        QList<QVariant> args = addedSpy.value(i);
        QCOMPARE(args.count(), 1);
        QList<QCandlestickSet *> sets = qvariant_cast<QList<QCandlestickSet *>>(args.at(0));
        QCOMPARE(sets.count(), 1);
        QCOMPARE(sets.first(), set);
    }
}

void tst_QCandlestickSeries::take()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->count(), m_sets.count());

    QSignalSpy countSpy(m_series, SIGNAL(countChanged()));
    QSignalSpy removedSpy(m_series, SIGNAL(candlestickSetsRemoved(QList<QCandlestickSet *>)));

    for (int i = 0; i < m_sets.count(); ++i) {
        QCandlestickSet *set = m_sets.at(i);
        QVERIFY(m_series->take(set));
        QCOMPARE(m_series->count(), m_sets.count() - i - 1);
        QTRY_COMPARE(countSpy.count(), i + 1);
        QTRY_COMPARE(removedSpy.count(), i + 1);

        QList<QVariant> args = removedSpy.value(i);
        QCOMPARE(args.count(), 1);
        QList<QCandlestickSet *> sets = qvariant_cast<QList<QCandlestickSet *>>(args.at(0));
        QCOMPARE(sets.count(), 1);
        QCOMPARE(sets.first(), set);
    }
}

void tst_QCandlestickSeries::clear()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->count(), m_sets.count());

    m_series->clear();
    QCOMPARE(m_series->count(), 0);
}

void tst_QCandlestickSeries::sets()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->sets(), m_sets);

    for (int i = 0; i < m_sets.count(); ++i)
        QCOMPARE(m_series->sets().at(i), m_sets.at(i));

    m_series->clear();
    QCOMPARE(m_series->sets(), QList<QCandlestickSet *>());
}

void tst_QCandlestickSeries::count()
{
    m_series->append(m_sets);
    QCOMPARE(m_series->count(), m_sets.count());
    QCOMPARE(m_series->count(), m_series->sets().count());
}

void tst_QCandlestickSeries::type()
{
    QCOMPARE(m_series->type(), QAbstractSeries::SeriesTypeCandlestick);
}

void tst_QCandlestickSeries::maximumColumnWidth_data()
{
    QTest::addColumn<qreal>("maximumColumnWidth");
    QTest::addColumn<qreal>("expectedMaximumColumnWidth");

    QTest::newRow("maximum column width less than -1.0") << -3.0 << -1.0;
    QTest::newRow("maximum column equals to -1.0") << -1.0 << -1.0;
    QTest::newRow("maximum column width greater than -1.0, but less than zero") << -0.5 << -1.0;
    QTest::newRow("maximum column width equals zero") << 0.0 << 0.0;
    QTest::newRow("maximum column width greater than zero") << 1.0 << 1.0;
    QTest::newRow("maximum column width contains a fractional part") << 3.4 << 3.4;
}

void tst_QCandlestickSeries::maximumColumnWidth()
{
    QFETCH(qreal, maximumColumnWidth);
    QFETCH(qreal, expectedMaximumColumnWidth);

    QSignalSpy spy(m_series, SIGNAL(maximumColumnWidthChanged()));

    m_series->setMaximumColumnWidth(maximumColumnWidth);
    QCOMPARE(m_series->maximumColumnWidth(), expectedMaximumColumnWidth);
    QCOMPARE(spy.count(), 1);

    // Try set same maximum column width
    m_series->setMaximumColumnWidth(expectedMaximumColumnWidth);
    QCOMPARE(m_series->maximumColumnWidth(), expectedMaximumColumnWidth);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::minimumColumnWidth_data()
{
    QTest::addColumn<qreal>("minimumColumnWidth");
    QTest::addColumn<qreal>("expectedMinimumColumnWidth");

    QTest::newRow("minimum column width less than -1.0") << -3.0 << -1.0;
    QTest::newRow("minimum column equals to -1.0") << -1.0 << -1.0;
    QTest::newRow("minimum column width greater than -1.0, but less than zero") << -0.5 << -1.0;
    QTest::newRow("minimum column width equals zero") << 0.0 << 0.0;
    QTest::newRow("minimum column width greater than zero") << 1.0 << 1.0;
    QTest::newRow("minimum column width contains a fractional part") << 3.4 << 3.4;
}

void tst_QCandlestickSeries::minimumColumnWidth()
{
    QFETCH(qreal, minimumColumnWidth);
    QFETCH(qreal, expectedMinimumColumnWidth);

    QSignalSpy spy(m_series, SIGNAL(minimumColumnWidthChanged()));

    m_series->setMinimumColumnWidth(minimumColumnWidth);
    QCOMPARE(m_series->minimumColumnWidth(), expectedMinimumColumnWidth);
    QCOMPARE(spy.count(), 1);

    // Try set same minimum column width
    m_series->setMinimumColumnWidth(expectedMinimumColumnWidth);
    QCOMPARE(m_series->minimumColumnWidth(), expectedMinimumColumnWidth);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::bodyWidth_data()
{
    QTest::addColumn<qreal>("bodyWidth");
    QTest::addColumn<qreal>("expectedBodyWidth");

    QTest::newRow("body width less than zero") << -1.0 << 0.0;
    QTest::newRow("body width equals zero") << 0.0 << 0.0;
    QTest::newRow("body width greater than zero and less than one") << 0.5 << 0.5;
    QTest::newRow("body width equals one") << 1.0 << 1.0;
    QTest::newRow("body width greater than one") << 2.0 << 1.0;
}

void tst_QCandlestickSeries::bodyWidth()
{
    QFETCH(qreal, bodyWidth);
    QFETCH(qreal, expectedBodyWidth);

    QSignalSpy spy(m_series, SIGNAL(bodyWidthChanged()));

    m_series->setBodyWidth(bodyWidth);
    QCOMPARE(m_series->bodyWidth(), expectedBodyWidth);
    QCOMPARE(spy.count(), 1);

    // Try set same body width
    m_series->setBodyWidth(bodyWidth);
    QCOMPARE(m_series->bodyWidth(), expectedBodyWidth);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::bodyOutlineVisible()
{
    QSignalSpy spy(m_series, SIGNAL(bodyOutlineVisibilityChanged()));

    bool visible = !m_series->bodyOutlineVisible();
    m_series->setBodyOutlineVisible(visible);
    QCOMPARE(m_series->bodyOutlineVisible(), visible);
    QCOMPARE(spy.count(), 1);

    // Try set same body outline visibility
    m_series->setBodyOutlineVisible(visible);
    QCOMPARE(m_series->bodyOutlineVisible(), visible);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::capsWidth_data()
{
    QTest::addColumn<qreal>("capsWidth");
    QTest::addColumn<qreal>("expectedCapsWidth");

    QTest::newRow("caps width less than zero") << -1.0 << 0.0;
    QTest::newRow("caps width equals zero") << 0.0 << 0.0;
    QTest::newRow("caps width greater than zero and less than one") << 0.5 << 0.5;
    QTest::newRow("caps width equals one") << 1.0 << 1.0;
    QTest::newRow("caps width greater than one") << 2.0 << 1.0;
}

void tst_QCandlestickSeries::capsWidth()
{
    QFETCH(qreal, capsWidth);
    QFETCH(qreal, expectedCapsWidth);

    QSignalSpy spy(m_series, SIGNAL(capsWidthChanged()));

    m_series->setCapsWidth(capsWidth);
    QCOMPARE(m_series->capsWidth(), expectedCapsWidth);
    QCOMPARE(spy.count(), 1);

    // Try set same caps width
    m_series->setCapsWidth(capsWidth);
    QCOMPARE(m_series->capsWidth(), expectedCapsWidth);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::capsVisible()
{
    QSignalSpy spy(m_series, SIGNAL(capsVisibilityChanged()));

    bool visible = !m_series->capsVisible();
    m_series->setCapsVisible(visible);
    QCOMPARE(m_series->capsVisible(), visible);
    QCOMPARE(spy.count(), 1);

    // Try set same caps visibility
    m_series->setCapsVisible(visible);
    QCOMPARE(m_series->capsVisible(), visible);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::increasingColor()
{
    QSignalSpy spy(m_series, SIGNAL(increasingColorChanged()));

    // Try set new increasing color
    QColor newColor(200, 200, 200, 200);
    m_series->setIncreasingColor(newColor);
    QCOMPARE(m_series->increasingColor(), newColor);
    QCOMPARE(spy.count(), 1);

    // Try set same increasing color again
    m_series->setIncreasingColor(newColor);
    QCOMPARE(m_series->increasingColor(), newColor);
    QCOMPARE(spy.count(), 1);

    // Try set invalid increasing color (should change to default color)
    QColor defaultColor = m_series->brush().color();
    defaultColor.setAlpha(128);
    m_series->setIncreasingColor(QColor());
    QCOMPARE(m_series->increasingColor(), defaultColor);
    QCOMPARE(spy.count(), 2);

    // Set new brush, increasing color should change accordingly
    QBrush brush(newColor);
    defaultColor = brush.color();
    defaultColor.setAlpha(128);
    m_series->setBrush(brush);
    QCOMPARE(m_series->increasingColor(), defaultColor);
    QCOMPARE(spy.count(), 3);
}

void tst_QCandlestickSeries::decreasingColor()
{
    QSignalSpy spy(m_series, SIGNAL(decreasingColorChanged()));

    // Try set new decreasing color
    QColor newColor(200, 200, 200, 200);
    m_series->setDecreasingColor(newColor);
    QCOMPARE(m_series->decreasingColor(), newColor);
    QCOMPARE(spy.count(), 1);

    // Try set same decreasing color again
    m_series->setDecreasingColor(newColor);
    QCOMPARE(m_series->decreasingColor(), newColor);
    QCOMPARE(spy.count(), 1);

    // Try set invalid decreasing color (should change to default color)
    m_series->setDecreasingColor(QColor());
    QCOMPARE(m_series->decreasingColor(), m_series->brush().color());
    QCOMPARE(spy.count(), 2);

    // Set new brush, decreasing color should change accordingly
    m_series->setBrush(QBrush(newColor));
    QCOMPARE(m_series->decreasingColor(), m_series->brush().color());
    QCOMPARE(spy.count(), 3);
}

void tst_QCandlestickSeries::brush()
{
    QSignalSpy spy(m_series, SIGNAL(brushChanged()));

    QBrush brush(QColor(128, 128, 128, 128));
    QColor increasingColor(brush.color());
    increasingColor.setAlpha(128);
    QColor decreasingColor(brush.color());
    m_series->setBrush(brush);
    QCOMPARE(m_series->brush(), brush);
    QCOMPARE(m_series->increasingColor(), increasingColor);
    QCOMPARE(m_series->decreasingColor(), decreasingColor);
    QCOMPARE(spy.count(), 1);

    // Try set same brush
    m_series->setBrush(brush);
    QCOMPARE(m_series->brush(), brush);
    QCOMPARE(m_series->increasingColor(), increasingColor);
    QCOMPARE(m_series->decreasingColor(), decreasingColor);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::pen()
{
    QSignalSpy spy(m_series, SIGNAL(penChanged()));

    QPen pen(QColor(128, 128, 128, 128));
    m_series->setPen(pen);
    QCOMPARE(m_series->pen(), pen);
    QCOMPARE(spy.count(), 1);

    // Try set same pen
    m_series->setPen(pen);
    QCOMPARE(m_series->pen(), pen);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSeries::mouseClicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_sets.count());

    QCandlestickSet *set1 = m_series->sets().at(1);
    QCandlestickSet *set2 = m_series->sets().at(2);

    QSignalSpy seriesSpy(m_series, SIGNAL(clicked(QCandlestickSet *)));
    QSignalSpy setSpy1(set1, SIGNAL(clicked()));
    QSignalSpy setSpy2(set2, SIGNAL(clicked()));

    QChartView view(new QChart());
    view.resize(400, 300);
    view.chart()->addSeries(m_series);
    view.chart()->createDefaultAxes();
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for candlesticks
    QRectF plotArea = view.chart()->plotArea();
    qreal candlestickWidth = plotArea.width() / m_series->count();
    qreal candlestickHeight = plotArea.height();

    QMap<QCandlestickSet *, QRectF> layout;
    layout.insert(set1, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set1),
                               plotArea.top(), candlestickWidth, candlestickHeight));
    layout.insert(set2, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set2),
                               plotArea.top(), candlestickWidth, candlestickHeight));

    // Click set 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set1);
    seriesSpyArgs.clear();

    QVERIFY(setSpy1.takeFirst().isEmpty());

    // Click set 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set2);
    seriesSpyArgs.clear();

    QVERIFY(setSpy2.takeFirst().isEmpty());
}

void tst_QCandlestickSeries::mouseHovered()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();
    SKIP_IF_FLAKY_MOUSE_MOVE();

    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_sets.count());

    QCandlestickSet *set1 = m_series->sets().at(1);
    QCandlestickSet *set2 = m_series->sets().at(2);

    QSignalSpy seriesSpy(m_series, SIGNAL(hovered(bool, QCandlestickSet *)));
    QSignalSpy setSpy1(set1, SIGNAL(hovered(bool)));
    QSignalSpy setSpy2(set2, SIGNAL(hovered(bool)));

    QChartView view(new QChart());
    view.resize(400, 300);
    view.chart()->addSeries(m_series);
    view.chart()->createDefaultAxes();
    view.show();
    QTest::qWaitForWindowShown(&view);

    // This is hack since view does not get events otherwise
    view.setMouseTracking(true);

    // Calculate expected layout for candlesticks
    QRectF plotArea = view.chart()->plotArea();
    qreal candlestickWidth = plotArea.width() / m_series->count();
    qreal candlestickHeight = plotArea.height();

    QMap<QCandlestickSet *, QRectF> layout;
    layout.insert(set1, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set1),
                               plotArea.top(), candlestickWidth, candlestickHeight));
    layout.insert(set2, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set2),
                               plotArea.top(), candlestickWidth, candlestickHeight));

    // Move mouse to left border
    QTest::mouseMove(view.viewport(), QPoint(0, layout.value(set1).center().y()));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 0);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 0);

    // Move mouse on top of set 1
    QTest::mouseMove(view.viewport(), layout.value(set1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(1)), set1);
    QCOMPARE(seriesSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(seriesSpyArgs.at(0).toBool(), true);
    seriesSpyArgs.clear();

    QList<QVariant> setSpyArgs = setSpy1.takeFirst();
    QCOMPARE(setSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(setSpyArgs.at(0).toBool(), true);
    setSpyArgs.clear();

    // Move mouse from top of set 1 to top of set 2
    QTest::mouseMove(view.viewport(), layout.value(set2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 2);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 1);

    // Should leave set 1
    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(1)), set1);
    QCOMPARE(seriesSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(seriesSpyArgs.at(0).toBool(), false);
    // Don't call seriesSpyArgs.clear() here

    setSpyArgs = setSpy1.takeFirst();
    QCOMPARE(setSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(setSpyArgs.at(0).toBool(), false);
    // Don't call setSpyArgs.clear() here

    // Should enter set 2
    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(1)), set2);
    QCOMPARE(seriesSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(seriesSpyArgs.at(0).toBool(), true);
    seriesSpyArgs.clear();

    setSpyArgs = setSpy2.takeFirst();
    QCOMPARE(setSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(setSpyArgs.at(0).toBool(), true);
    setSpyArgs.clear();

    // Move mouse from top of set 2 to background
    QTest::mouseMove(view.viewport(), QPoint(layout.value(set2).center().x(), 0));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    // Should leave set 2
    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(1)), set2);
    QCOMPARE(seriesSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(seriesSpyArgs.at(0).toBool(), false);
    seriesSpyArgs.clear();

    setSpyArgs = setSpy2.takeFirst();
    QCOMPARE(setSpyArgs.at(0).type(), QVariant::Bool);
    QCOMPARE(setSpyArgs.at(0).toBool(), false);
    setSpyArgs.clear();
}

void tst_QCandlestickSeries::mousePressed()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_sets.count());

    QCandlestickSet *set1 = m_series->sets().at(1);
    QCandlestickSet *set2 = m_series->sets().at(2);

    QSignalSpy seriesSpy(m_series, SIGNAL(pressed(QCandlestickSet *)));
    QSignalSpy setSpy1(set1, SIGNAL(pressed()));
    QSignalSpy setSpy2(set2, SIGNAL(pressed()));

    QChartView view(new QChart());
    view.resize(400, 300);
    view.chart()->addSeries(m_series);
    view.chart()->createDefaultAxes();
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for candlesticks
    QRectF plotArea = view.chart()->plotArea();
    qreal candlestickWidth = plotArea.width() / m_series->count();
    qreal candlestickHeight = plotArea.height();

    QMap<QCandlestickSet *, QRectF> layout;
    layout.insert(set1, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set1),
                               plotArea.top(), candlestickWidth, candlestickHeight));
    layout.insert(set2, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set2),
                               plotArea.top(), candlestickWidth, candlestickHeight));

    // Press set 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set1);
    seriesSpyArgs.clear();

    QVERIFY(setSpy1.takeFirst().isEmpty());

    // Press set 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set2);
    seriesSpyArgs.clear();

    QVERIFY(setSpy2.takeFirst().isEmpty());
}

void tst_QCandlestickSeries::mouseReleased()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_sets.count());

    QCandlestickSet *set1 = m_series->sets().at(1);
    QCandlestickSet *set2 = m_series->sets().at(2);

    QSignalSpy seriesSpy(m_series, SIGNAL(released(QCandlestickSet *)));
    QSignalSpy setSpy1(set1, SIGNAL(released()));
    QSignalSpy setSpy2(set2, SIGNAL(released()));

    QChartView view(new QChart());
    view.resize(400, 300);
    view.chart()->addSeries(m_series);
    view.chart()->createDefaultAxes();
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for candlesticks
    QRectF plotArea = view.chart()->plotArea();
    qreal candlestickWidth = plotArea.width() / m_series->count();
    qreal candlestickHeight = plotArea.height();

    QMap<QCandlestickSet *, QRectF> layout;
    layout.insert(set1, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set1),
                               plotArea.top(), candlestickWidth, candlestickHeight));
    layout.insert(set2, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set2),
                               plotArea.top(), candlestickWidth, candlestickHeight));

    // Release mouse over set 1
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set1);
    seriesSpyArgs.clear();

    QVERIFY(setSpy1.takeFirst().isEmpty());

    // Release mouse over set 2
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, layout.value(set2).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 0);
    QCOMPARE(setSpy2.count(), 1);

    seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set2);
    seriesSpyArgs.clear();

    QVERIFY(setSpy2.takeFirst().isEmpty());
}

void tst_QCandlestickSeries::mouseDoubleClicked()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QVERIFY(m_series->append(m_sets));
    QCOMPARE(m_series->count(), m_sets.count());

    QCandlestickSet *set1 = m_series->sets().at(1);
    QCandlestickSet *set2 = m_series->sets().at(2);

    QSignalSpy seriesSpy(m_series, SIGNAL(doubleClicked(QCandlestickSet *)));
    QSignalSpy setSpy1(set1, SIGNAL(doubleClicked()));
    QSignalSpy setSpy2(set2, SIGNAL(doubleClicked()));

    QChartView view(new QChart());
    view.resize(400, 300);
    view.chart()->addSeries(m_series);
    view.chart()->createDefaultAxes();
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Calculate expected layout for candlesticks
    QRectF plotArea = view.chart()->plotArea();
    qreal candlestickWidth = plotArea.width() / m_series->count();
    qreal candlestickHeight = plotArea.height();

    QMap<QCandlestickSet *, QRectF> layout;
    layout.insert(set1, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set1),
                               plotArea.top(), candlestickWidth, candlestickHeight));
    layout.insert(set2, QRectF(plotArea.left() + candlestickWidth * m_sets.indexOf(set2),
                               plotArea.top(), candlestickWidth, candlestickHeight));

    // Double-click set 1
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, 0, layout.value(set1).center().toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QCOMPARE(setSpy1.count(), 1);
    QCOMPARE(setSpy2.count(), 0);

    QList<QVariant> seriesSpyArgs = seriesSpy.takeFirst();
    QCOMPARE(seriesSpyArgs.count(), 1);
    QCOMPARE(qvariant_cast<QCandlestickSet *>(seriesSpyArgs.at(0)), set1);
    seriesSpyArgs.clear();

    QVERIFY(setSpy1.takeFirst().isEmpty());
}

QTEST_MAIN(tst_QCandlestickSeries)

#include "tst_qcandlestickseries.moc"
