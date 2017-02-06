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
#include <QtCharts/QLegend>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieLegendMarker>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QAreaLegendMarker>
#include <QtCharts/QLineSeries>
#include <QtCharts/QXYLegendMarker>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarLegendMarker>
#include "tst_definitions.h"

QT_CHARTS_USE_NAMESPACE

class tst_QLegend : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qlegend();
    void qpieLegendMarker();
    void qareaLegendMarker();
    void qxyLegendMarker();
    void qbarLegendMarker();
    void markers();
    void addAndRemoveSeries();
    void pieMarkerProperties();
    void barMarkerProperties();
    void areaMarkerProperties();
    void xyMarkerPropertiesLine();
    void xyMarkerPropertiesScatter();
    void markerSignals();

private:

    QChart *m_chart;
};

void tst_QLegend::initTestCase()
{
}

void tst_QLegend::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QLegend::init()
{
    m_chart = newQChartOrQPolarChart();
}

void tst_QLegend::cleanup()
{
    delete m_chart;
    m_chart = 0;
}

void tst_QLegend::qlegend()
{
    QVERIFY(m_chart);

    QLegend *legend = m_chart->legend();
    QVERIFY(legend);

    QList<QLegendMarker*> markers = legend->markers();
    QVERIFY(markers.count() == 0);
}

void tst_QLegend::qpieLegendMarker()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QPieSeries *series = new QPieSeries();
    QPieSlice *s1 = new QPieSlice(QString("s1"), 111.0);
    series->append(s1);
    m_chart->addSeries(series);

    // Should have one marker
    QList<QLegendMarker*> markers = legend->markers();
    QVERIFY(markers.count() == 1);
    QLegendMarker *m = markers.at(0);

    // Should be piemarker
    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypePie);

    // Related series and slice must match
    QPieLegendMarker *pm = qobject_cast<QPieLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
    QVERIFY(pm->slice() == s1);

    // Add another slice
    QPieSlice *s2 = new QPieSlice(QString("s2"), 111.0);
    series->append(s2);

    markers = legend->markers();
    QVERIFY(markers.count() == 2);
    m = markers.at(1);

    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypePie);

    // Related series and slice must match
    pm = qobject_cast<QPieLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
    QVERIFY(pm->slice() == s2);
}

void tst_QLegend::qareaLegendMarker()
{
    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();
    QAreaSeries *series = new QAreaSeries();

    QLineSeries *upper = new QLineSeries(series);
    QLineSeries *lower = new QLineSeries(series);

    upper->append(1,1);
    lower->append(1,0);

    series->setUpperSeries(upper);
    series->setLowerSeries(lower);

    m_chart->addSeries(series);

    // Should have one marker
    QList<QLegendMarker *> markers = legend->markers();
    QVERIFY(markers.count() == 1);
    QLegendMarker *m = markers.at(0);

    QVERIFY(m->series() == series);
    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypeArea);

    QAreaLegendMarker *pm = qobject_cast<QAreaLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
}

void tst_QLegend::qxyLegendMarker()
{
    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QLineSeries *series = new QLineSeries();
    m_chart->addSeries(series);

    // Should have one marker
    QList<QLegendMarker *> markers = legend->markers();
    QVERIFY(markers.count() == 1);
    QLegendMarker *m = markers.at(0);

    QVERIFY(m->series() == series);
    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypeXY);

    QXYLegendMarker *pm = qobject_cast<QXYLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
}

void tst_QLegend::qbarLegendMarker()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QBarSeries *series = new QBarSeries();
    QBarSet *set0 = new QBarSet(QString("set0"));
    series->append(set0);
    m_chart->addSeries(series);

    // Should have one marker
    QList<QLegendMarker *> markers = legend->markers();
    QVERIFY(markers.count() == 1);
    QLegendMarker *m = markers.at(0);

    QVERIFY(m->series() == series);
    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypeBar);

    QBarLegendMarker *pm = qobject_cast<QBarLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
    QVERIFY(pm->barset() == set0);

    // Add another barset
    QBarSet *set1 = new QBarSet(QString("set1"));
    series->append(set1);

    markers = legend->markers();
    QVERIFY(markers.count() == 2);
    m = markers.at(1);

    QVERIFY(m->series() == series);
    QVERIFY(m->type() == QLegendMarker::LegendMarkerTypeBar);

    pm = qobject_cast<QBarLegendMarker *> (m);
    QVERIFY(pm);
    QVERIFY(pm->series() == series);
    QVERIFY(pm->barset() == set1);
}

void tst_QLegend::markers()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QPieSeries *pie = new QPieSeries();
    pie->append(QString("slice1"), 1);
    pie->append(QString("slice2"), 2);
    pie->append(QString("slice3"), 3);
    m_chart->addSeries(pie);
    QList<QPieSlice *> slices = pie->slices();

    QBarSeries *bar = new QBarSeries();
    QList<QBarSet *> sets;
    sets.append(new QBarSet(QString("set0")));
    sets.append(new QBarSet(QString("set1")));
    bar->append(sets);
    m_chart->addSeries(bar);

    QLineSeries *line = new QLineSeries();
    line->setName(QString("line1"));
    m_chart->addSeries(line);

    QAreaSeries *area = new QAreaSeries();
    QLineSeries *upper = new QLineSeries(area);
    QLineSeries *lower = new QLineSeries(area);
    upper->append(1,1);
    lower->append(1,0);
    area->setUpperSeries(upper);
    area->setLowerSeries(lower);

    m_chart->addSeries(area);

    QVERIFY(legend->markers(pie).count() == 3);
    QVERIFY(legend->markers(bar).count() == 2);
    QVERIFY(legend->markers(line).count() == 1);
    QVERIFY(legend->markers(area).count() == 1);
    QVERIFY(legend->markers().count() == 3+2+1+1);
}

void tst_QLegend::addAndRemoveSeries()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QPieSeries *pie = new QPieSeries();
    pie->append(QString("slice1"), 1);
    pie->append(QString("slice2"), 2);
    pie->append(QString("slice3"), 3);

    m_chart->addSeries(pie);

    QList<QLegendMarker *> markers = legend->markers();
    QVERIFY(markers.count() == 3);

    m_chart->removeSeries(pie);

    markers = legend->markers();
    QVERIFY(markers.count() == 0);

    delete pie;
}

void tst_QLegend::pieMarkerProperties()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QPieSeries *pie = new QPieSeries();
    QPieSlice *slice = pie->append(QString("Slice1"), 1);
    m_chart->addSeries(pie);

    QLegendMarker *m = legend->markers(pie).at(0);
    QPieLegendMarker *pm = qobject_cast<QPieLegendMarker *> (m);
    QVERIFY(pm);

    QBrush b1(QColor(12,34,56,78));
    QBrush b2(QColor(34,56,78,90));

    // Change brush
    slice->setBrush(b1);
    QVERIFY(pm->brush() == b1);
    slice->setBrush(b2);
    QVERIFY(pm->brush() == b2);

    // Change label
    QVERIFY(pm->label().compare(QString("Slice1")) == 0);
    slice->setLabel(QString("foo"));
    QVERIFY(pm->label().compare(QString("foo")) == 0);
    slice->setLabel(QString("bar"));
    QVERIFY(pm->label().compare(QString("bar")) == 0);

    // Change visibility
    pie->setVisible(false);
    TRY_COMPARE(pm->isVisible(), false);
    pie->setVisible(true);
    TRY_COMPARE(pm->isVisible(), true);
}

void tst_QLegend::barMarkerProperties()
{
    SKIP_ON_POLAR();

    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QBarSeries *bar = new QBarSeries();
    QBarSet *barSet = new QBarSet(QString("Set0"));
    *barSet << 1;
    bar->append(barSet);
    m_chart->addSeries(bar);

    QLegendMarker *m = legend->markers(bar).at(0);
    QBarLegendMarker *pm = qobject_cast<QBarLegendMarker *> (m);
    QVERIFY(pm);

    QBrush b1(QColor(12,34,56,78));
    QBrush b2(QColor(34,56,78,90));

    // Change brush
    barSet->setBrush(b1);
    QVERIFY(pm->brush() == b1);
    barSet->setBrush(b2);
    QVERIFY(pm->brush() == b2);

    // Change label
    QVERIFY(pm->label().compare(QString("Set0")) == 0);
    barSet->setLabel(QString("foo"));
    QVERIFY(pm->label().compare(QString("foo")) == 0);
    barSet->setLabel(QString("bar"));
    QVERIFY(pm->label().compare(QString("bar")) == 0);

    // Change visibility
    bar->setVisible(false);
    TRY_COMPARE(pm->isVisible(), false);
    bar->setVisible(true);
    TRY_COMPARE(pm->isVisible(), true);
}

void tst_QLegend::areaMarkerProperties()
{
    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QAreaSeries *area = new QAreaSeries();
    area->setName(QString("Area"));
    QLineSeries *upper = new QLineSeries(area);
    QLineSeries *lower = new QLineSeries(area);
    upper->append(1,1);
    lower->append(1,0);
    area->setUpperSeries(upper);
    area->setLowerSeries(lower);
    m_chart->addSeries(area);

    QLegendMarker *m = legend->markers(area).at(0);
    QAreaLegendMarker *pm = qobject_cast<QAreaLegendMarker *> (m);
    QVERIFY(pm);

    QBrush b1(QColor(12,34,56,78));
    QBrush b2(QColor(34,56,78,90));

    // Change brush
    area->setBrush(b1);
    QVERIFY(pm->brush() == b1);
    area->setBrush(b2);
    QVERIFY(pm->brush() == b2);

    // Change name
    QVERIFY(pm->label().compare(QString("Area")) == 0);
    area->setName(QString("foo"));
    QVERIFY(pm->label().compare(QString("foo")) == 0);
    area->setName(QString("bar"));
    QVERIFY(pm->label().compare(QString("bar")) == 0);

    // Change visibility
    area->setVisible(false);
    TRY_COMPARE(pm->isVisible(), false);
    area->setVisible(true);
    TRY_COMPARE(pm->isVisible(), true);
}

void tst_QLegend::xyMarkerPropertiesLine()
{
    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QLineSeries *line = new QLineSeries();
    line->setName(QString("Line"));
    line->append(1,1);
    m_chart->addSeries(line);

    QLegendMarker *m = legend->markers(line).at(0);
    QXYLegendMarker *pm = qobject_cast<QXYLegendMarker *> (m);
    QVERIFY(pm);

    // With line series, the marker is colored after pen, not brush
    QPen b1(QColor(12,34,56,78));
    QPen b2(QColor(34,56,78,90));

    // Change brush
    line->setPen(b1);
    QVERIFY(pm->brush().color() == b1.color());
    line->setPen(b2);
    QVERIFY(pm->brush().color() == b2.color());

    // Change name
    QVERIFY(pm->label().compare(QString("Line")) == 0);
    line->setName(QString("foo"));
    QVERIFY(pm->label().compare(QString("foo")) == 0);
    line->setName(QString("bar"));
    QVERIFY(pm->label().compare(QString("bar")) == 0);

    // Change visibility
    line->setVisible(false);
    TRY_COMPARE(pm->isVisible(), false);
    line->setVisible(true);
    TRY_COMPARE(pm->isVisible(), true);
}

void tst_QLegend::xyMarkerPropertiesScatter()
{
    QVERIFY(m_chart);
    QLegend *legend = m_chart->legend();

    QScatterSeries *scatter = new QScatterSeries();
    scatter->setName(QString("Scatter"));
    scatter->append(1,1);
    m_chart->addSeries(scatter);

    QLegendMarker *m = legend->markers(scatter).at(0);
    QXYLegendMarker *pm = qobject_cast<QXYLegendMarker *> (m);
    QVERIFY(pm);

    QBrush b1(QColor(12,34,56,78));
    QBrush b2(QColor(34,56,78,90));

    // Change brush
    scatter->setBrush(b1);
    QVERIFY(pm->brush() == b1);
    scatter->setBrush(b2);
    QVERIFY(pm->brush() == b2);

    // Change name
    QVERIFY(pm->label().compare(QString("Scatter")) == 0);
    scatter->setName(QString("foo"));
    QVERIFY(pm->label().compare(QString("foo")) == 0);
    scatter->setName(QString("bar"));
    QVERIFY(pm->label().compare(QString("bar")) == 0);

    // Change visibility
    scatter->setVisible(false);
    TRY_COMPARE(pm->isVisible(), false);
    scatter->setVisible(true);
    TRY_COMPARE(pm->isVisible(), true);
}

void tst_QLegend::markerSignals()
{
    SKIP_ON_POLAR();
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();
    SKIP_IF_FLAKY_MOUSE_MOVE();

    QChart *chart = newQChartOrQPolarChart();
    QLegend *legend = chart->legend();

    QBarSeries *bar = new QBarSeries();
    QBarSet *set1 = new QBarSet(QString("set 1"));
    *set1 << 10 << 10 << 10;
    bar->append(set1);
    QBarSet *set2 = new QBarSet(QString("set 2"));
    *set2 << 10 << 10 << 10;
    bar->append(set2);
    chart->addSeries(bar);

    QPieSeries *pie = new QPieSeries();
    pie->append(QString("slice1"), 1);
    pie->append(QString("slice2"), 2);
    pie->append(QString("slice3"), 3);
    chart->addSeries(pie);
    legend->setAlignment(Qt::AlignRight);

    QLineSeries *line = new QLineSeries();
    line->setName(QString("Line 1"));
    line->append(1,1);
    chart->addSeries(line);

    QAreaSeries *area = new QAreaSeries();
    area->setName(QString("Area 1"));
    QLineSeries *upper = new QLineSeries(area);
    QLineSeries *lower = new QLineSeries(area);
    upper->append(2,2);
    lower->append(1,1);
    area->setUpperSeries(upper);
    area->setLowerSeries(lower);
    chart->addSeries(area);

    QScatterSeries *scatter = new QScatterSeries();
    scatter->setName(QString("Scatter"));
    scatter->append(3,3);
    chart->addSeries(scatter);

    QList<QSignalSpy *> spies;
    foreach(QLegendMarker *m, legend->markers()) {
        QSignalSpy *spy = new QSignalSpy(m, SIGNAL(hovered(bool)));
        spies.append(spy);
    }

    QChartView view(chart);
    view.resize(400, 400);
    view.show();
    QTest::qWaitForWindowShown(&view);

    // Sweep mouse over all legend items
    for (int i = 0; i < 400; i++)
        QTest::mouseMove(view.viewport(), QPoint(333, i));

    foreach (QSignalSpy *spy, spies)
        TRY_COMPARE(spy->count(), 2);

    qDeleteAll(spies);
}

QTEST_MAIN(tst_QLegend)

#include "tst_qlegend.moc"

