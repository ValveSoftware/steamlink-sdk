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

#include "tst_qabstractaxis.h"

Q_DECLARE_METATYPE(QPen)
Q_DECLARE_METATYPE(Qt::Orientation)

void tst_QAbstractAxis::initTestCase()
{
}

void tst_QAbstractAxis::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QAbstractAxis::init(QAbstractAxis* axis, QAbstractSeries* series)
{
    m_axis = axis;
    m_series = series;
    m_view = new QChartView(newQChartOrQPolarChart());
    m_chart = m_view->chart();
}

void tst_QAbstractAxis::cleanup()
{
    delete m_view;
    m_view = 0;
    m_chart = 0;
    m_axis = 0;
}

void tst_QAbstractAxis::qabstractaxis()
{
    QCOMPARE(m_axis->linePen(), QPen());
    QCOMPARE(m_axis->gridLinePen(), QPen());
    QCOMPARE(m_axis->isLineVisible(), true);
    QCOMPARE(m_axis->isGridLineVisible(), true);
    QCOMPARE(m_axis->isVisible(), true);
    QCOMPARE(m_axis->labelsAngle(), 0);
    QCOMPARE(m_axis->labelsBrush(), QBrush());
    QCOMPARE(m_axis->labelsFont(), QFont());
    QCOMPARE(m_axis->labelsVisible(), true);
    QCOMPARE(m_axis->orientation(), Qt::Orientation(0));
    QCOMPARE(m_axis->minorGridLinePen(), QPen());
    QCOMPARE(m_axis->isMinorGridLineVisible(), true);
    m_axis->setLineVisible(false);
    m_axis->setLinePen(QPen());
    m_axis->setLinePenColor(QColor());
    m_axis->setGridLinePen(QPen());
    m_axis->setGridLineVisible(false);
    m_axis->setGridLineColor(QColor());
    m_axis->setMinorGridLineColor(QColor());
    m_axis->setLabelsAngle(-1);
    m_axis->setLabelsBrush(QBrush());
    m_axis->setLabelsColor(QColor());
    m_axis->setLabelsFont(QFont());
    m_axis->setLabelsVisible(false);
    m_axis->setMax(QVariant());
    m_axis->setMin(QVariant());
    m_axis->setRange(QVariant(), QVariant());
    m_axis->setShadesBorderColor(QColor());
    m_axis->setShadesBrush(QBrush());
    m_axis->setShadesColor(QColor());
    m_axis->setShadesPen(QPen());
    m_axis->setShadesVisible(false);
    m_axis->setVisible(false);
    m_axis->setMinorGridLinePen(QPen());
    m_axis->setMinorGridLineVisible(false);
    //TODO QCOMPARE(m_axis->shadesBrush(), QBrush());
    QCOMPARE(m_axis->shadesPen(), QPen());
    QCOMPARE(m_axis->shadesVisible(), false);
    m_axis->show();
    m_axis->hide();
}

void tst_QAbstractAxis::axisPen_data()
{
    QTest::addColumn<QPen>("axisPen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("black") << QPen(Qt::black);
    QTest::newRow("red") << QPen(Qt::red);
}

void tst_QAbstractAxis::axisPen()
{
    QFETCH(QPen, axisPen);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLinePen(axisPen);
    QCOMPARE(m_axis->linePen(), axisPen);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    //TODO QCOMPARE(m_axis->axisPen(), axisPen);
}

void tst_QAbstractAxis::axisPenColor_data()
{
}

void tst_QAbstractAxis::axisPenColor()
{
    QSKIP("Test is not implemented. This is deprecated function");
}

void tst_QAbstractAxis::gridLinePen_data()
{

    QTest::addColumn<QPen>("gridLinePen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("black") << QPen(Qt::black);
    QTest::newRow("red") << QPen(Qt::red);

}

void tst_QAbstractAxis::gridLinePen()
{
    QFETCH(QPen, gridLinePen);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setGridLinePen(gridLinePen);
    QCOMPARE(m_axis->gridLinePen(), gridLinePen);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    //TODO QCOMPARE(m_axis->gridLinePen(), gridLinePen);
}

void tst_QAbstractAxis::minorGridLinePen_data()
{

    QTest::addColumn<QPen>("minorGridLinePen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("black") << QPen(Qt::black);
    QTest::newRow("red") << QPen(Qt::red);

}

void tst_QAbstractAxis::minorGridLinePen()
{
    QFETCH(QPen, minorGridLinePen);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setMinorGridLinePen(minorGridLinePen);
    QCOMPARE(m_axis->minorGridLinePen(), minorGridLinePen);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

}

void tst_QAbstractAxis::lineVisible_data()
{
    QTest::addColumn<bool>("lineVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::lineVisible()
{
    QFETCH(bool, lineVisible);

    m_axis->setLineVisible(!lineVisible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLineVisible(lineVisible);
    QCOMPARE(m_axis->isLineVisible(), lineVisible);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->isLineVisible(), lineVisible);
}

void tst_QAbstractAxis::gridLineVisible_data()
{
    QTest::addColumn<bool>("gridLineVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::gridLineVisible()
{
    QFETCH(bool, gridLineVisible);

    m_axis->setGridLineVisible(!gridLineVisible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(gridLineColorChanged(QColor)));

    m_axis->setGridLineVisible(gridLineVisible);
    QCOMPARE(m_axis->isGridLineVisible(), gridLineVisible);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->isGridLineVisible(), gridLineVisible);

}

void tst_QAbstractAxis::minorGridLineVisible_data()
{
    QTest::addColumn<bool>("minorGridLineVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::minorGridLineVisible()
{
    QFETCH(bool, minorGridLineVisible);

    m_axis->setMinorGridLineVisible(!minorGridLineVisible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setMinorGridLineVisible(minorGridLineVisible);
    QCOMPARE(m_axis->isMinorGridLineVisible(), minorGridLineVisible);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 1);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->isMinorGridLineVisible(), minorGridLineVisible);

}

void tst_QAbstractAxis::gridLineColor_data()
{
    QTest::addColumn<QColor>("gridLineColor");
    QTest::newRow("blue") << QColor(Qt::blue);
    QTest::newRow("red") << QColor(Qt::red);
    QTest::newRow("yellow") << QColor(Qt::yellow);
}

void tst_QAbstractAxis::gridLineColor()
{
    QFETCH(QColor, gridLineColor);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setGridLineColor(gridLineColor);
    QCOMPARE(m_axis->gridLineColor(), gridLineColor);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 1);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

}

void tst_QAbstractAxis::minorGridLineColor_data()
{
    QTest::addColumn<QColor>("minorGridLineColor");
    QTest::newRow("blue") << QColor(Qt::blue);
    QTest::newRow("red") << QColor(Qt::red);
    QTest::newRow("yellow") << QColor(Qt::yellow);
}

void tst_QAbstractAxis::minorGridLineColor()
{
    QFETCH(QColor, minorGridLineColor);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setMinorGridLineColor(minorGridLineColor);
    QCOMPARE(m_axis->minorGridLineColor(), minorGridLineColor);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 1);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

}

void tst_QAbstractAxis::visible_data()
{
    QTest::addColumn<bool>("visible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::visible()
{
    QFETCH(bool, visible);

    m_axis->setVisible(!visible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setVisible(visible);
    QCOMPARE(m_axis->isVisible(), visible);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 1);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->isVisible(), visible);
}

void tst_QAbstractAxis::labelsAngle_data()
{
    QTest::addColumn<int>("labelsAngle");
    QTest::newRow("0") << 0;
    QTest::newRow("45") << 45;
    QTest::newRow("90") << 90;
}

void tst_QAbstractAxis::labelsAngle()
{
    QFETCH(int, labelsAngle);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLabelsAngle(labelsAngle);
    QCOMPARE(m_axis->labelsAngle(), labelsAngle);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->labelsAngle(), labelsAngle);
}

void tst_QAbstractAxis::labelsBrush_data()
{
    QTest::addColumn<QBrush>("labelsBrush");
    QTest::newRow("null") << QBrush();
    QTest::newRow("blue") << QBrush(Qt::blue);
    QTest::newRow("black") << QBrush(Qt::black);

}

void tst_QAbstractAxis::labelsBrush()
{

    QFETCH(QBrush, labelsBrush);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLabelsBrush(labelsBrush);
    QCOMPARE(m_axis->labelsBrush(), labelsBrush);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    //TODO QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->labelsBrush(), labelsBrush);

}

void tst_QAbstractAxis::labelsColor_data()
{

}

void tst_QAbstractAxis::labelsColor()
{
    QSKIP("Test is not implemented. This is deprecated function");
}

void tst_QAbstractAxis::labelsFont_data()
{
    QTest::addColumn<QFont>("labelsFont");
    QTest::newRow("null") << QFont();
    QTest::newRow("serif") << QFont("SansSerif");
}

void tst_QAbstractAxis::labelsFont()
{

    QFETCH(QFont, labelsFont);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLabelsFont(labelsFont);
    QCOMPARE(m_axis->labelsFont(), labelsFont);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->labelsFont(), labelsFont);

}

void tst_QAbstractAxis::labelsVisible_data()
{
    QTest::addColumn<bool>("labelsVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::labelsVisible()
{
    QFETCH(bool, labelsVisible);

    m_axis->setLabelsVisible(!labelsVisible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setLabelsVisible(labelsVisible);
    QCOMPARE(m_axis->labelsVisible(), labelsVisible);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->labelsVisible(), labelsVisible);
}

void tst_QAbstractAxis::orientation_data()
{
    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::newRow("Vertical") << Qt::Vertical;
    QTest::newRow("Horizontal") << Qt::Horizontal;
}

void tst_QAbstractAxis::orientation()
{
    QFETCH(Qt::Orientation, orientation);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    if(orientation==Qt::Vertical){
        m_chart->setAxisY(m_axis,m_series);
    }else{
        m_chart->setAxisX(m_axis,m_series);
    }
    QCOMPARE(m_axis->orientation(), orientation);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->orientation(), orientation);
}

void tst_QAbstractAxis::setMax_data()
{
    //just check if it does not crash
    QTest::addColumn<QVariant>("max");
    QTest::newRow("something") << QVariant("something");
    QTest::newRow("1.0") << QVariant(1.0);
}

void tst_QAbstractAxis::setMax()
{
    QFETCH(QVariant, max);
    m_axis->setMax(max);
}

void tst_QAbstractAxis::setMin_data()
{
    //just check if it does not crash
    QTest::addColumn<QVariant>("min");
    QTest::newRow("something") << QVariant("something");
    QTest::newRow("1.0") << QVariant(1.0);
}

// public void setMin(QVariant const& min)
void tst_QAbstractAxis::setMin()
{
    QFETCH(QVariant, min);
    m_axis->setMin(min);
}

void tst_QAbstractAxis::setRange_data()
{
    //just check if it does not crash
    QTest::addColumn<QVariant>("min");
    QTest::addColumn<QVariant>("max");
    QTest::newRow("something") << QVariant("something0") << QVariant("something1");
    QTest::newRow("-1 1") << QVariant(-1.0) << QVariant(1.0);
}

// public void setRange(QVariant const& min, QVariant const& max)
void tst_QAbstractAxis::setRange()
{

    QFETCH(QVariant, min);
    QFETCH(QVariant, max);
    m_axis->setRange(min,max);
}

void tst_QAbstractAxis::shadesBorderColor_data()
{

}

void tst_QAbstractAxis::shadesBorderColor()
{
    QSKIP("Test is not implemented. This is deprecated function");
}

void tst_QAbstractAxis::shadesBrush_data()
{
    QTest::addColumn<QBrush>("shadesBrush");
    QTest::newRow("null") << QBrush();
    QTest::newRow("blue") << QBrush(Qt::blue);
    QTest::newRow("black") << QBrush(Qt::black);
}

void tst_QAbstractAxis::shadesBrush()
{
    QFETCH(QBrush, shadesBrush);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setShadesBrush(shadesBrush);
    QCOMPARE(m_axis->shadesBrush(), shadesBrush);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    //TODO QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->shadesBrush(), shadesBrush);
}

void tst_QAbstractAxis::shadesColor_data()
{
}

// public QColor shadesColor() const
void tst_QAbstractAxis::shadesColor()
{
    QSKIP("Test is not implemented. This is deprecated function");
}

void tst_QAbstractAxis::shadesPen_data()
{
    QTest::addColumn<QPen>("shadesPen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("black") << QPen(Qt::black);
    QTest::newRow("red") << QPen(Qt::red);
}

void tst_QAbstractAxis::shadesPen()
{
    QFETCH(QPen, shadesPen);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setShadesPen(shadesPen);
    QCOMPARE(m_axis->shadesPen(), shadesPen);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->shadesPen(), shadesPen);
}

void tst_QAbstractAxis::shadesVisible_data()
{
    QTest::addColumn<bool>("shadesVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QAbstractAxis::shadesVisible()
{
    QFETCH(bool, shadesVisible);

    m_axis->setShadesVisible(!shadesVisible);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->setShadesVisible(shadesVisible);
    QCOMPARE(m_axis->shadesVisible(), shadesVisible);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 1);
    QCOMPARE(spy8.count(), 0);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);

    m_chart->setAxisX(m_axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_axis->shadesVisible(), shadesVisible);
}

void tst_QAbstractAxis::show_data()
{

}

void tst_QAbstractAxis::show()
{
    m_axis->hide();
    QCOMPARE(m_axis->isVisible(), false);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->show();

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 1);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);
    QCOMPARE(m_axis->isVisible(), true);
}

void tst_QAbstractAxis::hide_data()
{

}

void tst_QAbstractAxis::hide()
{
    m_axis->show();
    QCOMPARE(m_axis->isVisible(),true);

    QSignalSpy spy0(m_axis, SIGNAL(lineVisibleChanged(bool)));
    QSignalSpy spy1(m_axis, SIGNAL(colorChanged(QColor)));
    QSignalSpy spy2(m_axis, SIGNAL(gridVisibleChanged(bool)));
    QSignalSpy spy3(m_axis, SIGNAL(labelsColorChanged(QColor)));
    QSignalSpy spy4(m_axis, SIGNAL(labelsVisibleChanged(bool)));
    QSignalSpy spy5(m_axis, SIGNAL(shadesBorderColorChanged(QColor)));
    QSignalSpy spy6(m_axis, SIGNAL(shadesColorChanged(QColor)));
    QSignalSpy spy7(m_axis, SIGNAL(shadesVisibleChanged(bool)));
    QSignalSpy spy8(m_axis, SIGNAL(visibleChanged(bool)));
    QSignalSpy spy9(m_axis, SIGNAL(minorGridVisibleChanged(bool)));
    QSignalSpy spy10(m_axis, SIGNAL(gridLineColorChanged(QColor)));
    QSignalSpy spy11(m_axis, SIGNAL(minorGridLineColorChanged(QColor)));

    m_axis->hide();

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
    QCOMPARE(spy5.count(), 0);
    QCOMPARE(spy6.count(), 0);
    QCOMPARE(spy7.count(), 0);
    QCOMPARE(spy8.count(), 1);
    QCOMPARE(spy9.count(), 0);
    QCOMPARE(spy10.count(), 0);
    QCOMPARE(spy11.count(), 0);
    QCOMPARE(m_axis->isVisible(),false);
}












