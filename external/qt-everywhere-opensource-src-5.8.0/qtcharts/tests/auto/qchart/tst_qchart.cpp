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
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(QAbstractAxis *)
Q_DECLARE_METATYPE(QValueAxis *)
Q_DECLARE_METATYPE(QBarCategoryAxis *)
Q_DECLARE_METATYPE(QAbstractSeries *)
Q_DECLARE_METATYPE(QChart::AnimationOption)
Q_DECLARE_METATYPE(QBrush)
Q_DECLARE_METATYPE(QPen)
Q_DECLARE_METATYPE(QChart::ChartTheme)

class tst_QChart : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qchart_data();
    void qchart();
    void addSeries_data();
    void addSeries();
    void animationOptions_data();
    void animationOptions();
    void animationDuration();
    void animationCurve_data();
    void animationCurve();
    void axisX_data();
    void axisX();
    void axisY_data();
    void axisY();
    void backgroundBrush_data();
    void backgroundBrush();
    void backgroundPen_data();
    void backgroundPen();
    void isBackgroundVisible_data();
    void isBackgroundVisible();
    void plotAreaBackgroundBrush_data();
    void plotAreaBackgroundBrush();
    void plotAreaBackgroundPen_data();
    void plotAreaBackgroundPen();
    void isPlotAreaBackgroundVisible_data();
    void isPlotAreaBackgroundVisible();
    void legend_data();
    void legend();
    void plotArea_data();
    void plotArea();
    void removeAllSeries_data();
    void removeAllSeries();
    void removeSeries_data();
    void removeSeries();
    void scroll_right_data();
    void scroll_right();
    void scroll_left_data();
    void scroll_left();
    void scroll_up_data();
    void scroll_up();
    void scroll_down_data();
    void scroll_down();
    void theme_data();
    void theme();
    void title_data();
    void title();
    void titleBrush_data();
    void titleBrush();
    void titleFont_data();
    void titleFont();
    void zoomIn_data();
    void zoomIn();
    void zoomOut_data();
    void zoomOut();
    void zoomReset();
    void createDefaultAxesForLineSeries_data();
    void createDefaultAxesForLineSeries();
    void axisPolarOrientation();
    void backgroundRoundness();
private:
    void createTestData();

private:
    QChartView* m_view;
    QChart* m_chart;
};

void tst_QChart::initTestCase()
{

}

void tst_QChart::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QChart::init()
{
    m_view = new QChartView(newQChartOrQPolarChart());
    m_chart = m_view->chart();
}

void tst_QChart::cleanup()
{
    delete m_view;
    m_view = 0;
    m_chart = 0;
}


void tst_QChart::createTestData()
{
     QLineSeries* series0 = new QLineSeries(this);
     *series0 << QPointF(0, 0) << QPointF(100, 100);
     m_chart->addSeries(series0);
     m_view->show();
     QTest::qWaitForWindowShown(m_view);
}

void tst_QChart::qchart_data()
{
}

void tst_QChart::qchart()
{
    QVERIFY(m_chart);
    QVERIFY(m_chart->legend());
    QVERIFY(m_chart->legend()->isVisible());

    QCOMPARE(m_chart->animationOptions(), QChart::NoAnimation);
    QCOMPARE(m_chart->animationDuration(), 1000);
    QCOMPARE(m_chart->animationEasingCurve(), QEasingCurve(QEasingCurve::OutQuart));
    QVERIFY(!m_chart->axisX());
    QVERIFY(!m_chart->axisY());
    QVERIFY(m_chart->backgroundBrush()!=QBrush());
    QVERIFY(m_chart->backgroundPen()!=QPen());
    QCOMPARE(m_chart->isBackgroundVisible(), true);
    QVERIFY(m_chart->plotArea().top()==0);
    QVERIFY(m_chart->plotArea().left()==0);
    QVERIFY(m_chart->plotArea().right()==0);
    QVERIFY(m_chart->plotArea().bottom()==0);
    QCOMPARE(m_chart->theme(), QChart::ChartThemeLight);
    QCOMPARE(m_chart->title(), QString());

    //QCOMPARE(m_chart->titleBrush(),QBrush());
    //QCOMPARE(m_chart->titleFont(),QFont());

    m_chart->removeAllSeries();
    m_chart->scroll(0,0);

    m_chart->zoomIn();
    m_chart->zoomIn(QRectF());
    m_chart->zoomOut();

    m_view->show();

    QVERIFY(m_chart->plotArea().top()>0);
    QVERIFY(m_chart->plotArea().left()>0);
    QVERIFY(m_chart->plotArea().right()>0);
    QVERIFY(m_chart->plotArea().bottom()>0);
}

void tst_QChart::addSeries_data()
{
    QTest::addColumn<QAbstractSeries *>("series");

    QAbstractSeries* line = new QLineSeries(this);
    QAbstractSeries* area = new QAreaSeries(new QLineSeries(this));
    QAbstractSeries* scatter = new QScatterSeries(this);
    QAbstractSeries* spline = new QSplineSeries(this);

    QTest::newRow("lineSeries") << line;
    QTest::newRow("areaSeries") << area;
    QTest::newRow("scatterSeries") << scatter;
    QTest::newRow("splineSeries") << spline;

    if (!isPolarTest()) {
        QAbstractSeries* pie = new QPieSeries(this);
        QAbstractSeries* bar = new QBarSeries(this);
        QAbstractSeries* percent = new QPercentBarSeries(this);
        QAbstractSeries* stacked = new QStackedBarSeries(this);
        QTest::newRow("pieSeries") << pie;
        QTest::newRow("barSeries") << bar;
        QTest::newRow("percentBarSeries") << percent;
        QTest::newRow("stackedBarSeries") << stacked;
    }
}

void tst_QChart::addSeries()
{
    QFETCH(QAbstractSeries *, series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QVERIFY(!series->chart());
    QCOMPARE(m_chart->series().count(), 0);
    m_chart->addSeries(series);
    QCOMPARE(m_chart->series().count(), 1);
    QCOMPARE(m_chart->series().first(), series);
    QVERIFY(series->chart() == m_chart);
    m_chart->createDefaultAxes();
    if(series->type()!=QAbstractSeries::SeriesTypePie){
        QVERIFY(m_chart->axisY(series));
        QVERIFY(m_chart->axisX(series));
    }else{
        QVERIFY(!m_chart->axisY(series));
        QVERIFY(!m_chart->axisX(series));
    }
    m_chart->removeSeries(series);
    QVERIFY(!series->chart());
    QCOMPARE(m_chart->series().count(), 0);
    delete series;
}

void tst_QChart::animationOptions_data()
{
    QTest::addColumn<QChart::AnimationOption>("animationOptions");
    QTest::newRow("AllAnimations") << QChart::AllAnimations;
    QTest::newRow("NoAnimation") << QChart::NoAnimation;
    QTest::newRow("GridAxisAnimations") << QChart::GridAxisAnimations;
    QTest::newRow("SeriesAnimations") << QChart::SeriesAnimations;
}

void tst_QChart::animationOptions()
{
    createTestData();
    QFETCH(QChart::AnimationOption, animationOptions);
    m_chart->setAnimationOptions(animationOptions);
    QCOMPARE(m_chart->animationOptions(), animationOptions);
}

void tst_QChart::animationDuration()
{
    createTestData();
    m_chart->setAnimationDuration(2000);
    QVERIFY(m_chart->animationDuration() == 2000);
}

void tst_QChart::animationCurve_data()
{
    QTest::addColumn<QEasingCurve>("animationCurve");
    QTest::newRow("Linear") << QEasingCurve(QEasingCurve::Linear);
    QTest::newRow("InCubic") << QEasingCurve(QEasingCurve::InCubic);
    QTest::newRow("OutSine") << QEasingCurve(QEasingCurve::OutSine);
    QTest::newRow("OutInBack") << QEasingCurve(QEasingCurve::OutInBack);
}

void tst_QChart::animationCurve()
{
    createTestData();
    QFETCH(QEasingCurve, animationCurve);
    m_chart->setAnimationEasingCurve(animationCurve);
    QCOMPARE(m_chart->animationEasingCurve(), animationCurve);
}

void tst_QChart::axisX_data()
{

    QTest::addColumn<QAbstractAxis*>("axis");
    QTest::addColumn<QAbstractSeries *>("series");

    QTest::newRow("categories,lineSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QLineSeries(this);
    QTest::newRow("categories,areaSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QAreaSeries(new QLineSeries(this));
    QTest::newRow("categories,scatterSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QScatterSeries(this);
    QTest::newRow("categories,splineSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QSplineSeries(this);
    if (!isPolarTest()) {
        QTest::newRow("categories,pieSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QPieSeries(this);
        QTest::newRow("categories,barSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QBarSeries(this);
        QTest::newRow("categories,percentBarSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QPercentBarSeries(this);
        QTest::newRow("categories,stackedBarSeries") <<  (QAbstractAxis*) new QBarCategoryAxis() << (QAbstractSeries*) new QStackedBarSeries(this);
    }

    QTest::newRow("value,lineSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QLineSeries(this);
    QTest::newRow("value,areaSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QAreaSeries(new QLineSeries(this));
    QTest::newRow("value,scatterSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QScatterSeries(this);
    QTest::newRow("value,splineSeries") << (QAbstractAxis*) new QValueAxis() <<  (QAbstractSeries*) new QSplineSeries(this);
    if (!isPolarTest()) {
        QTest::newRow("value,pieSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QPieSeries(this);
        QTest::newRow("value,barSeries") << (QAbstractAxis*) new QValueAxis() <<   (QAbstractSeries*) new QBarSeries(this);
        QTest::newRow("value,percentBarSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QPercentBarSeries(this);
        QTest::newRow("value,stackedBarSeries") << (QAbstractAxis*) new QValueAxis() << (QAbstractSeries*) new QStackedBarSeries(this);
    }
}

void tst_QChart::axisX()
{
    QFETCH(QAbstractAxis*, axis);
    QFETCH(QAbstractSeries*, series);
    QVERIFY(!m_chart->axisX());
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    m_chart->addSeries(series);
    m_chart->setAxisX(axis,series);
    QVERIFY(m_chart->axisX(series)==axis);
}

void tst_QChart::axisY_data()
{
    axisX_data();
}


void tst_QChart::axisY()
{
    QFETCH(QAbstractAxis*, axis);
    QFETCH(QAbstractSeries*, series);
    QVERIFY(!m_chart->axisY());
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    m_chart->addSeries(series);
    m_chart->setAxisY(axis,series);
    QVERIFY(m_chart->axisY(series)==axis);
}

void tst_QChart::backgroundBrush_data()
{
    QTest::addColumn<QBrush>("backgroundBrush");
    QTest::newRow("null") << QBrush();
    QTest::newRow("blue") << QBrush(Qt::blue);
    QTest::newRow("white") << QBrush(Qt::white);
    QTest::newRow("black") << QBrush(Qt::black);
}

void tst_QChart::backgroundBrush()
{
    QFETCH(QBrush, backgroundBrush);
    m_chart->setBackgroundBrush(backgroundBrush);
    QCOMPARE(m_chart->backgroundBrush(), backgroundBrush);
}

void tst_QChart::backgroundPen_data()
{
    QTest::addColumn<QPen>("backgroundPen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("white") << QPen(Qt::white);
    QTest::newRow("black") << QPen(Qt::black);
}


void tst_QChart::backgroundPen()
{
    QFETCH(QPen, backgroundPen);
    m_chart->setBackgroundPen(backgroundPen);
    QCOMPARE(m_chart->backgroundPen(), backgroundPen);
}

void tst_QChart::isBackgroundVisible_data()
{
    QTest::addColumn<bool>("isBackgroundVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QChart::isBackgroundVisible()
{
    QFETCH(bool, isBackgroundVisible);
    m_chart->setBackgroundVisible(isBackgroundVisible);
    QCOMPARE(m_chart->isBackgroundVisible(), isBackgroundVisible);
}

void tst_QChart::plotAreaBackgroundBrush_data()
{
    QTest::addColumn<QBrush>("plotAreaBackgroundBrush");
    QTest::newRow("null") << QBrush();
    QTest::newRow("blue") << QBrush(Qt::blue);
    QTest::newRow("white") << QBrush(Qt::white);
    QTest::newRow("black") << QBrush(Qt::black);
}

void tst_QChart::plotAreaBackgroundBrush()
{
    QFETCH(QBrush, plotAreaBackgroundBrush);
    m_chart->setPlotAreaBackgroundBrush(plotAreaBackgroundBrush);
    QCOMPARE(m_chart->plotAreaBackgroundBrush(), plotAreaBackgroundBrush);
}

void tst_QChart::plotAreaBackgroundPen_data()
{
    QTest::addColumn<QPen>("plotAreaBackgroundPen");
    QTest::newRow("null") << QPen();
    QTest::newRow("blue") << QPen(Qt::blue);
    QTest::newRow("white") << QPen(Qt::white);
    QTest::newRow("black") << QPen(Qt::black);
}


void tst_QChart::plotAreaBackgroundPen()
{
    QFETCH(QPen, plotAreaBackgroundPen);
    m_chart->setPlotAreaBackgroundPen(plotAreaBackgroundPen);
    QCOMPARE(m_chart->plotAreaBackgroundPen(), plotAreaBackgroundPen);
}

void tst_QChart::isPlotAreaBackgroundVisible_data()
{
    QTest::addColumn<bool>("isPlotAreaBackgroundVisible");
    QTest::newRow("true") << true;
    QTest::newRow("false") << false;
}

void tst_QChart::isPlotAreaBackgroundVisible()
{
    QFETCH(bool, isPlotAreaBackgroundVisible);
    m_chart->setPlotAreaBackgroundVisible(isPlotAreaBackgroundVisible);
    QCOMPARE(m_chart->isPlotAreaBackgroundVisible(), isPlotAreaBackgroundVisible);
}
void tst_QChart::legend_data()
{

}

void tst_QChart::legend()
{
    QLegend *legend = m_chart->legend();
    QVERIFY(legend);
    QVERIFY(!m_chart->legend()->reverseMarkers());

    // Colors related signals
    QSignalSpy colorSpy(legend, SIGNAL(colorChanged(QColor)));
    QSignalSpy borderColorSpy(legend, SIGNAL(borderColorChanged(QColor)));
    QSignalSpy labelColorSpy(legend, SIGNAL(labelColorChanged(QColor)));

    // colorChanged
    legend->setColor(QColor("aliceblue"));
    QCOMPARE(colorSpy.count(), 1);
    QBrush b = legend->brush();
    b.setColor(QColor("aqua"));
    legend->setBrush(b);
    QCOMPARE(colorSpy.count(), 2);

    // borderColorChanged
    legend->setBorderColor(QColor("aliceblue"));
    QCOMPARE(borderColorSpy.count(), 1);
    QPen p = legend->pen();
    p.setColor(QColor("aqua"));
    legend->setPen(p);
    QCOMPARE(borderColorSpy.count(), 2);

    // labelColorChanged
    legend->setLabelColor(QColor("lightsalmon"));
    QCOMPARE(labelColorSpy.count(), 1);
    b = legend->labelBrush();
    b.setColor(QColor("lightseagreen"));
    legend->setLabelBrush(b);
    QCOMPARE(labelColorSpy.count(), 2);

    // fontChanged
    QSignalSpy fontSpy(legend, SIGNAL(fontChanged(QFont)));
    QFont f = legend->font();
    f.setBold(!f.bold());
    legend->setFont(f);
    QCOMPARE(fontSpy.count(), 1);

    // reverseMarkersChanged
    QSignalSpy reverseMarkersSpy(legend, SIGNAL(reverseMarkersChanged(bool)));
    QCOMPARE(reverseMarkersSpy.count(), 0);
    legend->setReverseMarkers();
    QCOMPARE(reverseMarkersSpy.count(), 1);
    QVERIFY(legend->reverseMarkers());
}

void tst_QChart::plotArea_data()
{

}

void tst_QChart::plotArea()
{
    createTestData();
    QRectF rect = m_chart->geometry();
    QVERIFY(m_chart->plotArea().isValid());
    QVERIFY(m_chart->plotArea().height() < rect.height());
    QVERIFY(m_chart->plotArea().width() < rect.width());
}

void tst_QChart::removeAllSeries_data()
{

}

void tst_QChart::removeAllSeries()
{
    QLineSeries* series0 = new QLineSeries(this);
    QLineSeries* series1 = new QLineSeries(this);
    QLineSeries* series2 = new QLineSeries(this);
    QSignalSpy deleteSpy1(series0, SIGNAL(destroyed()));
    QSignalSpy deleteSpy2(series1, SIGNAL(destroyed()));
    QSignalSpy deleteSpy3(series2, SIGNAL(destroyed()));

    m_chart->addSeries(series0);
    m_chart->addSeries(series1);
    m_chart->addSeries(series2);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    m_chart->createDefaultAxes();
    QCOMPARE(m_chart->axes().count(), 2);
    QVERIFY(m_chart->axisY(series0)!=0);
    QVERIFY(m_chart->axisY(series1)!=0);
    QVERIFY(m_chart->axisY(series2)!=0);

    m_chart->removeAllSeries();
    QCOMPARE(m_chart->axes().count(), 2);
    QVERIFY(m_chart->axisX() != 0);
    QVERIFY(m_chart->axisY() != 0);
    QCOMPARE(deleteSpy1.count(), 1);
    QCOMPARE(deleteSpy2.count(), 1);
    QCOMPARE(deleteSpy3.count(), 1);
}

void tst_QChart::removeSeries_data()
{
    axisX_data();
}

void tst_QChart::removeSeries()
{
    QFETCH(QAbstractAxis *, axis);
    QFETCH(QAbstractSeries *, series);
    QSignalSpy deleteSpy(series, SIGNAL(destroyed()));
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    if(!axis) axis = m_chart->axisY();
    m_chart->addSeries(series);
    m_chart->setAxisY(axis,series);
    QCOMPARE(m_chart->axisY(series),axis);
    m_chart->removeSeries(series);
    QCOMPARE(m_chart->axes().count(), 1);
    QVERIFY(m_chart->axisY() != 0);
    QVERIFY(m_chart->axisY(series)==0);
    QCOMPARE(deleteSpy.count(), 0);
    delete series;
}

void tst_QChart::scroll_right_data()
{
     QTest::addColumn<QAbstractSeries *>("series");

     QLineSeries* series0 = new QLineSeries(this);
     *series0 << QPointF(0, 0) << QPointF(100, 100);

     QTest::newRow("lineSeries") << (QAbstractSeries*) series0;


}

void tst_QChart::scroll_right()
{
    QFETCH(QAbstractSeries *, series);
    m_chart->addSeries(series);
    m_chart->createDefaultAxes();
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QAbstractAxis * axis = m_chart->axisX();
    QVERIFY(axis!=0);

    switch(axis->type())
    {
        case QAbstractAxis::AxisTypeValue:{
            QValueAxis* vaxis = qobject_cast<QValueAxis*>(axis);
            QVERIFY(vaxis!=0);
            qreal min = vaxis->min();
            qreal max = vaxis->max();
            QVERIFY(max>min);
            m_chart->scroll(50, 0);
            QVERIFY(min<vaxis->min());
            QVERIFY(max<vaxis->max());
            break;
        }
        case QAbstractAxis::AxisTypeBarCategory:{
            QBarCategoryAxis* caxis = qobject_cast<QBarCategoryAxis*>(axis);
            QVERIFY(caxis!=0);
            qreal min = caxis->min().toDouble();
            qreal max = caxis->max().toDouble();
            m_chart->scroll(50, 0);
            QVERIFY(min<caxis->min().toDouble());
            QVERIFY(max<caxis->max().toDouble());
            break;
        }
        default:
            qFatal("Unsupported type");
            break;
    }
}

void tst_QChart::scroll_left_data()
{
    scroll_right_data();
}

void tst_QChart::scroll_left()
{
     QFETCH(QAbstractSeries *, series);
     m_chart->addSeries(series);
     m_chart->createDefaultAxes();
     m_view->show();
     QTest::qWaitForWindowShown(m_view);
     QAbstractAxis * axis = m_chart->axisX();
     QVERIFY(axis!=0);

     switch(axis->type())
        {
            case QAbstractAxis::AxisTypeValue:{
                QValueAxis* vaxis = qobject_cast<QValueAxis*>(axis);
                QVERIFY(vaxis!=0);
                qreal min = vaxis->min();
                qreal max = vaxis->max();
                m_chart->scroll(-50, 0);
                QVERIFY(min>vaxis->min());
                QVERIFY(max>vaxis->max());
                break;
            }
            case QAbstractAxis::AxisTypeBarCategory:{
                QBarCategoryAxis* caxis = qobject_cast<QBarCategoryAxis*>(axis);
                QVERIFY(caxis!=0);
                qreal min = caxis->min().toDouble();
                qreal max = caxis->max().toDouble();
                m_chart->scroll(-50, 0);
                QVERIFY(min>caxis->min().toDouble());
                QVERIFY(max>caxis->max().toDouble());
                break;
            }
            default:
                qFatal("Unsupported type");
                break;
        }
}

void tst_QChart::scroll_up_data()
{
    scroll_right_data();
}

void tst_QChart::scroll_up()
{
    QFETCH(QAbstractSeries *, series);
    m_chart->addSeries(series);
    m_chart->createDefaultAxes();
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QAbstractAxis * axis = m_chart->axisY();
    QVERIFY(axis!=0);

    switch(axis->type())
    {
        case QAbstractAxis::AxisTypeValue:{
            QValueAxis* vaxis = qobject_cast<QValueAxis*>(axis);
            QVERIFY(vaxis!=0);
            qreal min = vaxis->min();
            qreal max = vaxis->max();
            m_chart->scroll(0, 50);
            QVERIFY(min<vaxis->min());
            QVERIFY(max<vaxis->max());
            break;
        }
        case QAbstractAxis::AxisTypeBarCategory:{
            QBarCategoryAxis* caxis = qobject_cast<QBarCategoryAxis*>(axis);
            QVERIFY(caxis!=0);
            qreal min = caxis->min().toDouble();
            qreal max = caxis->max().toDouble();
            m_chart->scroll(0, 50);
            QVERIFY(min<caxis->min().toDouble());
            QVERIFY(max<caxis->max().toDouble());
            break;
        }
        default:
            qFatal("Unsupported type");
            break;
    }
}

void tst_QChart::scroll_down_data()
{
    scroll_right_data();
}

void tst_QChart::scroll_down()
{
     QFETCH(QAbstractSeries *, series);
     m_chart->addSeries(series);
     m_chart->createDefaultAxes();
     m_view->show();
     QTest::qWaitForWindowShown(m_view);
     QAbstractAxis * axis = m_chart->axisY();
     QVERIFY(axis!=0);

     switch(axis->type())
        {
            case QAbstractAxis::AxisTypeValue:{
                QValueAxis* vaxis = qobject_cast<QValueAxis*>(axis);
                QVERIFY(vaxis!=0);
                qreal min = vaxis->min();
                qreal max = vaxis->max();
                m_chart->scroll(0, -50);
                QVERIFY(min>vaxis->min());
                QVERIFY(max>vaxis->max());
                break;
            }
            case QAbstractAxis::AxisTypeBarCategory:{
                QBarCategoryAxis* caxis = qobject_cast<QBarCategoryAxis*>(axis);
                QVERIFY(caxis!=0);
                qreal min = caxis->min().toDouble();
                qreal max = caxis->max().toDouble();
                m_chart->scroll(0, -50);
                QVERIFY(min>caxis->min().toDouble());
                QVERIFY(max>caxis->max().toDouble());
                break;
            }
            default:
                qFatal("Unsupported type");
                break;
        }
}

void tst_QChart::theme_data()
{
    QTest::addColumn<QChart::ChartTheme>("theme");
    QTest::newRow("ChartThemeBlueCerulean") << QChart::ChartThemeBlueCerulean;
    QTest::newRow("ChartThemeBlueIcy") << QChart::ChartThemeBlueIcy;
    QTest::newRow("ChartThemeBlueNcs") << QChart::ChartThemeBlueNcs;
    QTest::newRow("ChartThemeBrownSand") << QChart::ChartThemeBrownSand;
    QTest::newRow("ChartThemeDark") << QChart::ChartThemeDark;
    QTest::newRow("hartThemeHighContrast") << QChart::ChartThemeHighContrast;
    QTest::newRow("ChartThemeLight") << QChart::ChartThemeLight;
    QTest::newRow("ChartThemeQt") << QChart::ChartThemeQt;
}

void tst_QChart::theme()
{
    QFETCH(QChart::ChartTheme, theme);
    createTestData();
    m_chart->setTheme(theme);
    QVERIFY(m_chart->theme()==theme);
}

void tst_QChart::title_data()
{
    QTest::addColumn<QString>("title");
    QTest::newRow("null") << QString();
    QTest::newRow("foo") << QString("foo");
}

void tst_QChart::title()
{
    QFETCH(QString, title);
    m_chart->setTitle(title);
    QCOMPARE(m_chart->title(), title);
}

void tst_QChart::titleBrush_data()
{
    QTest::addColumn<QBrush>("titleBrush");
    QTest::newRow("null") << QBrush();
    QTest::newRow("blue") << QBrush(Qt::blue);
    QTest::newRow("white") << QBrush(Qt::white);
    QTest::newRow("black") << QBrush(Qt::black);
}

void tst_QChart::titleBrush()
{
    QFETCH(QBrush, titleBrush);
    m_chart->setTitleBrush(titleBrush);
    QCOMPARE(m_chart->titleBrush().color(), titleBrush.color());
}

void tst_QChart::titleFont_data()
{
    QTest::addColumn<QFont>("titleFont");
    QTest::newRow("null") << QFont();
    QTest::newRow("courier") << QFont("Courier", 8, QFont::Bold, true);
}

void tst_QChart::titleFont()
{
    QFETCH(QFont, titleFont);
    m_chart->setTitleFont(titleFont);
    QCOMPARE(m_chart->titleFont(), titleFont);
}

void tst_QChart::zoomIn_data()
{
    QTest::addColumn<QRectF>("rect");
    QTest::newRow("null") << QRectF();
    QTest::newRow("100x100") << QRectF(10,10,100,100);
    QTest::newRow("200x200") << QRectF(10,10,200,200);
}


void tst_QChart::zoomIn()
{

    QFETCH(QRectF, rect);
    createTestData();
    m_chart->createDefaultAxes();
    QRectF marigns = m_chart->plotArea();
    rect.adjust(marigns.left(),marigns.top(),-marigns.right(),-marigns.bottom());
    QValueAxis* axisX = qobject_cast<QValueAxis*>(m_chart->axisX());
    QVERIFY(axisX!=0);
    QValueAxis* axisY = qobject_cast<QValueAxis*>(m_chart->axisY());
    QVERIFY(axisY!=0);
    qreal minX = axisX->min();
    qreal minY = axisY->min();
    qreal maxX = axisX->max();
    qreal maxY = axisY->max();
    m_chart->zoomIn(rect);
    if(rect.isValid()){
        QVERIFY(minX<axisX->min());
        QVERIFY(maxX>axisX->max());
        QVERIFY(minY<axisY->min());
        QVERIFY(maxY>axisY->max());
    }

}

void tst_QChart::zoomOut_data()
{

}

void tst_QChart::zoomOut()
{
    createTestData();
    m_chart->createDefaultAxes();

    QValueAxis* axisX = qobject_cast<QValueAxis*>(m_chart->axisX());
    QVERIFY(axisX!=0);
    QValueAxis* axisY = qobject_cast<QValueAxis*>(m_chart->axisY());
    QVERIFY(axisY!=0);

    qreal minX = axisX->min();
    qreal minY = axisY->min();
    qreal maxX = axisX->max();
    qreal maxY = axisY->max();

    m_chart->zoomIn();

    QVERIFY(minX < axisX->min());
    QVERIFY(maxX > axisX->max());
    QVERIFY(minY < axisY->min());
    QVERIFY(maxY > axisY->max());

    m_chart->zoomOut();

    // min x may be a zero value
    if (qFuzzyIsNull(minX))
        QVERIFY(qFuzzyIsNull(axisX->min()));
    else
        QCOMPARE(minX, axisX->min());

    // min y may be a zero value
    if (qFuzzyIsNull(minY))
        QVERIFY(qFuzzyIsNull(axisY->min()));
    else
        QCOMPARE(minY, axisY->min());

    QVERIFY(maxX == axisX->max());
    QVERIFY(maxY == axisY->max());

}

void tst_QChart::zoomReset()
{
    createTestData();
    m_chart->createDefaultAxes();
    QValueAxis *axisX = qobject_cast<QValueAxis *>(m_chart->axisX());
    QVERIFY(axisX != 0);
    QValueAxis *axisY = qobject_cast<QValueAxis *>(m_chart->axisY());
    QVERIFY(axisY != 0);

    qreal minX = axisX->min();
    qreal minY = axisY->min();
    qreal maxX = axisX->max();
    qreal maxY = axisY->max();

    QVERIFY(!m_chart->isZoomed());

    m_chart->zoomIn();

    QVERIFY(m_chart->isZoomed());
    QVERIFY(minX < axisX->min());
    QVERIFY(maxX > axisX->max());
    QVERIFY(minY < axisY->min());
    QVERIFY(maxY > axisY->max());

    m_chart->zoomReset();

    // Reset after zoomIn should restore originals
    QVERIFY(!m_chart->isZoomed());
    QVERIFY(minX == axisX->min());
    QVERIFY(maxX == axisX->max());
    QVERIFY(minY == axisY->min());
    QVERIFY(maxY == axisY->max());

    m_chart->zoomOut();

    QVERIFY(m_chart->isZoomed());
    QVERIFY(minX > axisX->min());
    QVERIFY(maxX < axisX->max());
    QVERIFY(minY > axisY->min());
    QVERIFY(maxY < axisY->max());

    m_chart->zoomReset();

    // Reset after zoomOut should restore originals
    QVERIFY(!m_chart->isZoomed());
    QVERIFY(minX == axisX->min());
    QVERIFY(maxX == axisX->max());
    QVERIFY(minY == axisY->min());
    QVERIFY(maxY == axisY->max());

    axisX->setRange(234, 345);
    axisY->setRange(345, 456);

    minX = axisX->min();
    minY = axisY->min();
    maxX = axisX->max();
    maxY = axisY->max();

    QVERIFY(!m_chart->isZoomed());

    m_chart->zoomReset();

    // Reset without zoom should not change anything
    QVERIFY(!m_chart->isZoomed());
    QVERIFY(minX == axisX->min());
    QVERIFY(maxX == axisX->max());
    QVERIFY(minY == axisY->min());
    QVERIFY(maxY == axisY->max());

}

void tst_QChart::createDefaultAxesForLineSeries_data()
{
    QTest::addColumn<qreal>("series1minX");
    QTest::addColumn<qreal>("series1midX");
    QTest::addColumn<qreal>("series1maxX");
    QTest::addColumn<qreal>("series2minX");
    QTest::addColumn<qreal>("series2midX");
    QTest::addColumn<qreal>("series2maxX");
    QTest::addColumn<qreal>("overallminX");
    QTest::addColumn<qreal>("overallmaxX");
    QTest::addColumn<qreal>("series1minY");
    QTest::addColumn<qreal>("series1midY");
    QTest::addColumn<qreal>("series1maxY");
    QTest::addColumn<qreal>("series2minY");
    QTest::addColumn<qreal>("series2midY");
    QTest::addColumn<qreal>("series2maxY");
    QTest::addColumn<qreal>("overallminY");
    QTest::addColumn<qreal>("overallmaxY");
    QTest::newRow("series1hasMinAndMax") << (qreal)1.0 << (qreal)2.0 << (qreal)3.0 << (qreal)1.1 << (qreal)1.7 << (qreal)2.9 << (qreal)1.0 << (qreal)3.0
                                         << (qreal)1.0 << (qreal)2.0 << (qreal)3.0 << (qreal)1.1 << (qreal)1.7 << (qreal)2.9 << (qreal)1.0 << (qreal)3.0;
    QTest::newRow("series2hasMinAndMax") << (qreal)1.1 << (qreal)2.0 << (qreal)2.9 << (qreal)1.0 << (qreal)1.7 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0
                                         << (qreal)1.1 << (qreal)2.0 << (qreal)2.9 << (qreal)1.0 << (qreal)1.7 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0;
    QTest::newRow("series1hasMinAndMaxX_series2hasMinAndMaxY") << (qreal)1.0 << (qreal)2.0 << (qreal)3.0 << (qreal)1.1 << (qreal)1.7 << (qreal)2.9 << (qreal)1.0 << (qreal)3.0
                                                               << (qreal)1.1 << (qreal)2.0 << (qreal)2.9 << (qreal)1.0 << (qreal)2.0 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0;
    QTest::newRow("series1hasMin_series2hasMax") << (qreal)1.0 << (qreal)2.0 << (qreal)2.9 << (qreal)1.1 << (qreal)1.7 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0
                                                 << (qreal)1.0 << (qreal)2.0 << (qreal)2.9 << (qreal)1.1 << (qreal)1.7 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0;
    QTest::newRow("bothSeriesHaveSameMinAndMax") << (qreal)1.0 << (qreal)2.0 << (qreal)2.9 << (qreal)1.1 << (qreal)1.7 << (qreal)3.0 << (qreal)1.0 << (qreal)3.0
                                                 << (qreal)1.1 << (qreal)1.1 << (qreal)1.1 << (qreal)1.1 << (qreal)1.1 << (qreal)1.1 << (qreal)0.6 << (qreal)1.6;
}

void tst_QChart::createDefaultAxesForLineSeries()
{
    QFETCH(qreal, series1minX);
    QFETCH(qreal, series1midX);
    QFETCH(qreal, series1maxX);
    QFETCH(qreal, series2minX);
    QFETCH(qreal, series2midX);
    QFETCH(qreal, series2maxX);
    QFETCH(qreal, series1minY);
    QFETCH(qreal, series1midY);
    QFETCH(qreal, series1maxY);
    QFETCH(qreal, series2minY);
    QFETCH(qreal, series2midY);
    QFETCH(qreal, series2maxY);
    QFETCH(qreal, overallminX);
    QFETCH(qreal, overallmaxX);
    QFETCH(qreal, overallminY);
    QFETCH(qreal, overallmaxY);
    QLineSeries* series1 = new QLineSeries(this);
    series1->append(series1minX, series1minY);
    series1->append(series1midX, series1midY);
    series1->append(series1maxX, series1maxY);
    QLineSeries* series2 = new QLineSeries(this);
    series2->append(series2minX, series2minY);
    series2->append(series2midX, series2midY);
    series2->append(series2maxX, series2maxY);
    QChart *chart = newQChartOrQPolarChart();
    chart->addSeries(series1);
    chart->addSeries(series2);
    chart->createDefaultAxes();
    QValueAxis *xAxis = (QValueAxis *)chart->axisX();
    QCOMPARE(xAxis->min(), overallminX);
    QCOMPARE(xAxis->max(), overallmaxX);
    QValueAxis *yAxis = (QValueAxis *)chart->axisY();
    QCOMPARE(yAxis->min(), overallminY);
    QCOMPARE(yAxis->max(), overallmaxY);
    QLineSeries *series3 = new QLineSeries(this);
    // Numbers clearly out of existing range
    series3->append(0, 0);
    series3->append(100, 100);
    // Adding a new series should not change the axes as they have not been told to update
    chart->addSeries(series3);
    QCOMPARE(xAxis->min(), overallminX);
    QCOMPARE(xAxis->max(), overallmaxX);
    QCOMPARE(yAxis->min(), overallminY);
    QCOMPARE(yAxis->max(), overallmaxY);
    delete chart;
}

void tst_QChart::axisPolarOrientation()
{
    QLineSeries* series1 = new QLineSeries(this);
    series1->append(1, 2);
    series1->append(2, 4);
    series1->append(3, 8);
    QPolarChart chart;
    chart.addSeries(series1);

    QValueAxis *xAxis = new QValueAxis();
    QValueAxis *yAxis = new QValueAxis();
    chart.addAxis(xAxis, QPolarChart::PolarOrientationAngular);
    chart.addAxis(yAxis, QPolarChart::PolarOrientationRadial);

    QList<QAbstractAxis *> xAxes = chart.axes(QPolarChart::PolarOrientationAngular);
    QList<QAbstractAxis *> yAxes = chart.axes(QPolarChart::PolarOrientationRadial);

    QCOMPARE(xAxes.size(), 1);
    QCOMPARE(yAxes.size(), 1);
    QCOMPARE(xAxes[0], xAxis);
    QCOMPARE(yAxes[0], yAxis);

    QCOMPARE(chart.axisPolarOrientation(xAxes[0]), QPolarChart::PolarOrientationAngular);
    QCOMPARE(chart.axisPolarOrientation(yAxes[0]), QPolarChart::PolarOrientationRadial);
}

void tst_QChart::backgroundRoundness()
{
    createTestData();
    m_chart->createDefaultAxes();
    m_chart->setBackgroundRoundness(100.0);
    QVERIFY(m_chart->backgroundRoundness() == 100.0);
}

QTEST_MAIN(tst_QChart)
#include "tst_qchart.moc"

