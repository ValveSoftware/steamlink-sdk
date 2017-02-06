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

#include "tst_qxyseries.h"

Q_DECLARE_METATYPE(QList<QPointF>)

void tst_QXYSeries::initTestCase()
{
}

void tst_QXYSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QXYSeries::init()
{
    m_view = new QChartView(newQChartOrQPolarChart());
    m_chart = m_view->chart();
}

void tst_QXYSeries::cleanup()
{
    delete m_view;
    m_view = 0;
    m_chart = 0;
    m_series = 0;
}

void tst_QXYSeries::seriesName()
{
    QSignalSpy nameSpy(m_series, SIGNAL(nameChanged()));
    QCOMPARE(m_series->name(), QString());
    m_series->setName("seriesname");
    QCOMPARE(m_series->name(), QString("seriesname"));
    TRY_COMPARE(nameSpy.count(), 1);
}

void tst_QXYSeries::seriesVisible()
{
    QSignalSpy visibleSpy(m_series, SIGNAL(visibleChanged()));
    QCOMPARE(m_series->isVisible(), true);
    m_series->setVisible(false);
    QCOMPARE(m_series->isVisible(), false);
    m_series->setVisible(true);
    TRY_COMPARE(visibleSpy.count(), 2);
}

void tst_QXYSeries::seriesOpacity()
{
    QSignalSpy opacitySpy(m_series, SIGNAL(opacityChanged()));
    QCOMPARE(m_series->opacity(), 1.0);

    m_series->setOpacity(0.5);
    QCOMPARE(m_series->opacity(), 0.5);
    QCOMPARE(opacitySpy.count(), 1);

    m_series->setOpacity(0.0);
    QCOMPARE(m_series->opacity(), 0.0);
    QCOMPARE(opacitySpy.count(), 2);

    m_series->setOpacity(1.0);
    QCOMPARE(m_series->opacity(), 1.0);
    QCOMPARE(opacitySpy.count(), 3);
}

void tst_QXYSeries::pointLabelsFormat()
{
    QSignalSpy labelsFormatSpy(m_series, SIGNAL(pointLabelsFormatChanged(QString)));
    QCOMPARE(m_series->pointLabelsFormat(), QLatin1String("@xPoint, @yPoint"));

    QString format("@yPoint");
    m_series->setPointLabelsFormat(format);
    TRY_COMPARE(labelsFormatSpy.count(), 1);
    QList<QVariant> arguments = labelsFormatSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == format);

    m_series->setPointLabelsFormat(QString());
    TRY_COMPARE(labelsFormatSpy.count(), 1);
    arguments = labelsFormatSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == QString());

}

void tst_QXYSeries::pointLabelsVisible()
{
    QSignalSpy labelsVisibleSpy(m_series, SIGNAL(pointLabelsVisibilityChanged(bool)));
    QCOMPARE(m_series->pointLabelsVisible(), false);

    m_series->setPointLabelsVisible();
    QCOMPARE(m_series->pointLabelsVisible(), true);
    TRY_COMPARE(labelsVisibleSpy.count(), 1);
    QList<QVariant> arguments = labelsVisibleSpy.takeFirst();
    QVERIFY(arguments.at(0).toBool() == true);

    m_series->setPointLabelsVisible(false);
    QCOMPARE(m_series->pointLabelsVisible(), false);
    TRY_COMPARE(labelsVisibleSpy.count(), 1);
    arguments = labelsVisibleSpy.takeFirst();
    QVERIFY(arguments.at(0).toBool() == false);
}

void tst_QXYSeries::pointLabelsFont()
{
    QFont defaultFont(m_series->pointLabelsFont());
    QSignalSpy labelsFontSpy(m_series, SIGNAL(pointLabelsFontChanged(QFont)));

    QFont font("Times", 10);
    m_series->setPointLabelsFont(font);
    TRY_COMPARE(labelsFontSpy.count(), 1);
    QList<QVariant> arguments = labelsFontSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QFont>() == font);

    m_series->setPointLabelsFont(defaultFont);
    TRY_COMPARE(labelsFontSpy.count(), 1);
    arguments = labelsFontSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QFont>() == defaultFont);

}

void tst_QXYSeries::pointLabelsColor()
{
    QColor defaultColor(QPen().color());
    QSignalSpy labelsColorSpy(m_series, SIGNAL(pointLabelsColorChanged(QColor)));
    QCOMPARE(m_series->pointLabelsColor(), defaultColor);

    QColor color(Qt::blue);
    m_series->setPointLabelsColor(color);
    TRY_COMPARE(labelsColorSpy.count(), 1);
    QList<QVariant> arguments = labelsColorSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QColor>() == color);

    m_series->setPointLabelsColor(defaultColor);
    TRY_COMPARE(labelsColorSpy.count(), 1);
    arguments = labelsColorSpy.takeFirst();
    QVERIFY(arguments.at(0).value<QColor>() == defaultColor);
}

void tst_QXYSeries::pointLabelsClipping()
{
    QSignalSpy labelsClippingSpy(m_series, SIGNAL(pointLabelsClippingChanged(bool)));
    QCOMPARE(m_series->pointLabelsClipping(), true);

    m_series->setPointLabelsClipping(false);
    QCOMPARE(m_series->pointLabelsClipping(), false);
    TRY_COMPARE(labelsClippingSpy.count(), 1);
    QList<QVariant> arguments = labelsClippingSpy.takeFirst();
    QVERIFY(arguments.at(0).toBool() == false);

    m_series->setPointLabelsClipping();
    QCOMPARE(m_series->pointLabelsClipping(), true);
    TRY_COMPARE(labelsClippingSpy.count(), 1);
    arguments = labelsClippingSpy.takeFirst();
    QVERIFY(arguments.at(0).toBool() == true);
}

void tst_QXYSeries::append_data()
{
    QTest::addColumn< QList<QPointF> >("points");
    QTest::addColumn< QList<QPointF> >("otherPoints");
    QTest::newRow("0,0 1,1 2,2 3,3")
            << (QList<QPointF>() << QPointF(0,0) << QPointF(1,1) << QPointF(2,2) << QPointF(3,3))
            << (QList<QPointF>() << QPointF(4,4) << QPointF(5,5) << QPointF(6,6) << QPointF(7,7));
    QTest::newRow("0,0 -1,-1 -2,-2 -3,-3")
            << (QList<QPointF>() << QPointF(0,0) << QPointF(-1,-1) << QPointF(-2,-2) << QPointF(-3,-3))
            << (QList<QPointF>() << QPointF(-4,-4) << QPointF(-5,-5) << QPointF(-6,-6) << QPointF(-7,-7));
}


void tst_QXYSeries::append_raw_data()
{
    append_data();
}

void tst_QXYSeries::append_raw()
{
    QFETCH(QList<QPointF>, points);
    QFETCH(QList<QPointF>, otherPoints);
    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));
    QSignalSpy addedSpy(m_series, SIGNAL(pointAdded(int)));
    m_series->append(points);
    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(addedSpy.count(), points.count());
    QCOMPARE(m_series->points(), points);
    QCOMPARE(m_series->pointsVector(), points.toVector());

    // Process events between appends
    foreach (const QPointF &point, otherPoints) {
        m_series->append(point);
        QApplication::processEvents();
    }
}

void tst_QXYSeries::chart_append_data()
{
    append_data();
}

void tst_QXYSeries::chart_append()
{
    append_raw();
    m_chart->addSeries(m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
}

void tst_QXYSeries::append_chart_data()
{
    append_data();
}

void tst_QXYSeries::append_chart()
{
    m_view->show();
    m_chart->addSeries(m_series);
    QTest::qWaitForWindowShown(m_view);
    append_raw();

}

void tst_QXYSeries::append_chart_animation_data()
{
    append_data();
}

void tst_QXYSeries::append_chart_animation()
{
    m_chart->setAnimationOptions(QChart::AllAnimations);
    append_chart();
}

void tst_QXYSeries::count_data()
{
    QTest::addColumn<int>("count");
    QTest::newRow("0") << 0;
    QTest::newRow("5") << 5;
    QTest::newRow("10") << 5;
}

void tst_QXYSeries::count_raw_data()
{
    count_data();
}

void tst_QXYSeries::count_raw()
{
    QFETCH(int, count);

    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));

    for(int i=0 ; i< count; ++i)
        m_series->append(i,i);

    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->count(), count);
}

void tst_QXYSeries::remove_raw_data()
{
    append_data();
}

void tst_QXYSeries::remove_raw()
{
    QFETCH(QList<QPointF>, points);
    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));
    m_series->append(points);
    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->points(), points);

    foreach (const QPointF& point,points)
        m_series->remove(point);

    QCOMPARE(m_series->points().count(), 0);
    TRY_COMPARE(spy0.count(), 0);

    m_series->append(points);
    QCOMPARE(m_series->points(), points);

    //reverse order
    for(int i = points.count()-1 ; i>=0; i--){
        m_series->remove(points[i]);
    }
    QCOMPARE(m_series->points().count(), 0);

    QApplication::processEvents();

    // Process events between removes
    m_series->append(points);
    QCOMPARE(m_series->points(), points);
    foreach (const QPointF &point, points) {
        m_series->remove(point);
        QApplication::processEvents();
    }

    // Actual meaningful delay between removes, but still shorter than animation duration
    // (simulate e.g. spamming a hypothetical "remove last point"-button)
    QList<QPointF> bunchOfPoints;
    for (int i = 0; i < 10; i++)
        bunchOfPoints.append(QPointF(i, (qreal) rand() / (qreal) RAND_MAX));
    m_series->replace(bunchOfPoints);
    QCOMPARE(m_series->points(), bunchOfPoints);
    QTest::qWait(1500); // Wait for append animations to be over
    for (int i = bunchOfPoints.count() - 1; i >= 0; i--) {
        m_series->remove(bunchOfPoints.at(i));
        QTest::qWait(50);
    }
    QCOMPARE(m_series->points().count(), 0);

    // Removal using index
    for (int i = 0; i < 10; i++)
        bunchOfPoints.append(QPointF(i, (qreal) rand() / (qreal) RAND_MAX));
    m_series->replace(bunchOfPoints);
    m_series->remove(5);
    m_series->remove(0);
    QCOMPARE(m_series->points().count(), (bunchOfPoints.count() - 2));
    for (int i = bunchOfPoints.count() - 3; i >= 0; i--) {
        m_series->remove(i);
        QCOMPARE(m_series->points().count(), i);
    }
    QCOMPARE(m_series->points().count(), 0);

    // Multiple removal using index
    for (int i = 0; i < 10; i++)
        bunchOfPoints.append(QPointF(i, (qreal) rand() / (qreal) RAND_MAX));
    m_series->replace(bunchOfPoints);
    m_series->removePoints(5, 2);
    m_series->removePoints(0, 3);
    QCOMPARE(m_series->points().count(), (bunchOfPoints.count() - 5));
    m_series->removePoints(0, (bunchOfPoints.count() - 5));
    QCOMPARE(m_series->points().count(), 0);
}

void tst_QXYSeries::remove_chart_data()
{
    append_data();
}

void tst_QXYSeries::remove_chart()
{
    m_view->show();
    m_chart->addSeries(m_series);
    QTest::qWaitForWindowShown(m_view);
    remove_raw();
}

void tst_QXYSeries::remove_chart_animation_data()
{
    append_data();
}

void tst_QXYSeries::remove_chart_animation()
{
    m_chart->setAnimationOptions(QChart::AllAnimations);
    remove_chart();
}


void tst_QXYSeries::clear_raw_data()
{
    append_data();
}

void tst_QXYSeries::clear_raw()
{
    QFETCH(QList<QPointF>, points);
    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));
    m_series->append(points);
    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->points(), points);
    m_series->clear();
    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->points().count(), 0);

    QApplication::processEvents();
}

void tst_QXYSeries::clear_chart_data()
{
    append_data();
}

void tst_QXYSeries::clear_chart()
{
    m_view->show();
    m_chart->addSeries(m_series);
    QTest::qWaitForWindowShown(m_view);
    clear_raw();
}

void tst_QXYSeries::clear_chart_animation_data()
{
    append_data();
}

void tst_QXYSeries::clear_chart_animation()
{
    m_chart->setAnimationOptions(QChart::AllAnimations);
    clear_chart();
}

void tst_QXYSeries::replace_raw_data()
{
    append_data();
}

void tst_QXYSeries::replace_raw()
{
    QFETCH(QList<QPointF>, points);
    QFETCH(QList<QPointF>, otherPoints);
    QSignalSpy pointReplacedSpy(m_series, SIGNAL(pointReplaced(int)));
    QSignalSpy pointsReplacedSpy(m_series, SIGNAL(pointsReplaced()));
    m_series->append(points);
    TRY_COMPARE(pointReplacedSpy.count(), 0);
    TRY_COMPARE(pointsReplacedSpy.count(), 0);
    QCOMPARE(m_series->points(), points);

    foreach (const QPointF& point, points)
       m_series->replace(point.x(),point.y(),point.x(),0);
    TRY_COMPARE(pointReplacedSpy.count(), points.count());
    TRY_COMPARE(pointsReplacedSpy.count(), 0);

    // Replace a point that does not exist
    m_series->replace(-123, 999, 0, 0);
    TRY_COMPARE(pointReplacedSpy.count(), points.count());
    TRY_COMPARE(pointsReplacedSpy.count(), 0);

    QList<QPointF> newPoints = m_series->points();
    QCOMPARE(newPoints.count(), points.count());
    for(int i =0 ; i<points.count() ; ++i) {
        QCOMPARE(points[i].x(), newPoints[i].x());
        QCOMPARE(newPoints[i].y(), 0.0);
    }

    // Replace all points
    QList<QPointF> allPoints;
    for (int i = 0; i < 10; i++)
        allPoints.append(QPointF(i, (qreal) rand() / (qreal) RAND_MAX));
    m_series->replace(allPoints);
    TRY_COMPARE(pointReplacedSpy.count(), points.count());
    TRY_COMPARE(pointsReplacedSpy.count(), 1);

    m_series->replace(points);
    QApplication::processEvents();

    // Process events between replaces
    for (int i = 0; i < points.count(); ++i) {
        m_series->replace(points.at(i), otherPoints.at(i));
        QApplication::processEvents();
    }

    newPoints = m_series->points();
    QCOMPARE(newPoints.count(), points.count());
    for (int i = 0; i < points.count(); ++i) {
        QCOMPARE(otherPoints.at(i).x(), newPoints.at(i).x());
        QCOMPARE(otherPoints.at(i).y(), newPoints.at(i).y());
    }

    // Append followed by a replace shouldn't crash
    m_series->clear();
    m_series->append(QPointF(22,22));
    m_series->append(QPointF(23,23));
    QApplication::processEvents();
    m_series->replace(QPointF(23,23), otherPoints.at(1));
    QCOMPARE(m_series->points().at(1).x(), otherPoints.at(1).x());
    QCOMPARE(m_series->points().at(1).y(), otherPoints.at(1).y());

    // Replace using index
    m_series->append(otherPoints);
    m_series->replace(0, QPointF(333, 333));
    m_series->replace(3, 444, 444);
    m_series->replace(m_series->count() - 1, QPointF(555, 555));
    QCOMPARE(m_series->points().at(0),  QPointF(333, 333));
    QCOMPARE(m_series->points().at(3), QPointF(444, 444));
    QCOMPARE(m_series->points().at(m_series->count() - 1), QPointF(555, 555));
}


void tst_QXYSeries::replace_chart_data()
{
    append_data();
}

void tst_QXYSeries::replace_chart()
{
    m_view->show();
    m_chart->addSeries(m_series);
    QTest::qWaitForWindowShown(m_view);
    replace_raw();
}

void tst_QXYSeries::replace_chart_animation_data()
{
    append_data();
}

void tst_QXYSeries::replace_chart_animation()
{
    m_chart->setAnimationOptions(QChart::AllAnimations);
    replace_chart();
}

void tst_QXYSeries::insert_data()
{
    append_data();
}

void tst_QXYSeries::insert()
{
    QFETCH(QList<QPointF>, points);
    m_series->append(points);

    QSignalSpy addedSpy(m_series, SIGNAL(pointAdded(int)));

    m_series->insert(0, QPointF(5, 5));
    TRY_COMPARE(addedSpy.count(), 1);
    QCOMPARE(m_series->points().count(), points.count() + 1);

    m_series->insert(m_series->count(), QPointF(6, 6));
    TRY_COMPARE(addedSpy.count(), 2);
    QCOMPARE(m_series->points().count(), points.count() + 2);
}

void tst_QXYSeries::oper_data()
{
    append_data();
}

void tst_QXYSeries::oper()
{
    QFETCH(QList<QPointF>, points);

    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));

    foreach (const QPointF& point,points)
    {
        *m_series<<point;
    }

    QCOMPARE(m_series->points(), points);
    TRY_COMPARE(spy0.count(), 0);
}


void tst_QXYSeries::pen_data()
{
    QTest::addColumn<QPen>("pen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("black") << QPen(Qt::black);
    QTest::newRow("red") << QPen(Qt::red);
}

void tst_QXYSeries::pen()
{
    QFETCH(QPen, pen);

    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));
    m_series->setPen(pen);

    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->pen(), pen);

    m_chart->addSeries(m_series);

    if (pen != QPen())
        QCOMPARE(m_series->pen(), pen);

    m_chart->setTheme(QChart::ChartThemeDark);

    // setting a theme will overwrite all customizations
    if (pen != QPen())
        QVERIFY(m_series->pen() != pen);
}

void tst_QXYSeries::pointsVisible_data()
{
    QTest::addColumn<bool>("pointsVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QXYSeries::pointsVisible_raw_data()
{
    pointsVisible_data();
}

void tst_QXYSeries::pointsVisible_raw()
{
    QFETCH(bool, pointsVisible);
    QSignalSpy spy0(m_series, SIGNAL(clicked(QPointF)));
    m_series->setPointsVisible(pointsVisible);
    TRY_COMPARE(spy0.count(), 0);
    QCOMPARE(m_series->pointsVisible(), pointsVisible);
}

void tst_QXYSeries::changedSignals()
{
    QSignalSpy visibleSpy(m_series, SIGNAL(visibleChanged()));
    QSignalSpy nameSpy(m_series, SIGNAL(nameChanged()));
    QSignalSpy colorSpy(m_series, SIGNAL(colorChanged(QColor)));

    // Visibility
    m_series->setVisible(false);
    m_series->setVisible(false);
    TRY_COMPARE(visibleSpy.count(), 1);
    m_series->setVisible(true);
    TRY_COMPARE(visibleSpy.count(), 2);

    // Color
    m_series->setColor(QColor("aliceblue"));
    TRY_COMPARE(colorSpy.count(), 1);

    // Pen and Brush
    QPen p = m_series->pen();
    p.setColor("aquamarine");
    m_series->setPen(p);
    QBrush b = m_series->brush();
    b.setColor("beige");
    m_series->setBrush(b);
    TRY_COMPARE(colorSpy.count(), 2);

    // Verify all the signals again, to make sure no extra signals were emitted
    TRY_COMPARE(visibleSpy.count(), 2);
    TRY_COMPARE(nameSpy.count(), 0);
    TRY_COMPARE(colorSpy.count(), 2);
}
