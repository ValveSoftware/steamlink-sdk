/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include <QtDataVisualization/Q3DScatter>

using namespace QtDataVisualization;

class tst_scatter: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void construct();

    void initialProperties();
    void initializeProperties();
    void invalidProperties();

    void addSeries();
    void addMultipleSeries();
    void selectSeries();
    void removeSeries();
    void removeMultipleSeries();

private:
    Q3DScatter *m_graph;
};

QScatter3DSeries *newSeries()
{
    QScatter3DSeries *series = new QScatter3DSeries;
    QScatterDataArray data;
    data << QVector3D(0.5f, 0.5f, 0.5f) << QVector3D(-0.3f, -0.5f, -0.4f) << QVector3D(0.0f, -0.3f, 0.2f);
    series->dataProxy()->addItems(data);
    return series;
}

void tst_scatter::initTestCase()
{
}

void tst_scatter::cleanupTestCase()
{
}

void tst_scatter::init()
{
    m_graph = new Q3DScatter();
}

void tst_scatter::cleanup()
{
    delete m_graph;
}

void tst_scatter::construct()
{
    Q3DScatter *graph = new Q3DScatter();
    QVERIFY(graph);
    delete graph;

    QSurfaceFormat format;
    graph = new Q3DScatter(&format);
    QVERIFY(graph);
    delete graph;
}

void tst_scatter::initialProperties()
{
    QVERIFY(m_graph);
    QCOMPARE(m_graph->seriesList().length(), 0);
    QVERIFY(!m_graph->selectedSeries());
    QCOMPARE(m_graph->axisX()->orientation(), QAbstract3DAxis::AxisOrientationX);
    QCOMPARE(m_graph->axisY()->orientation(), QAbstract3DAxis::AxisOrientationY);
    QCOMPARE(m_graph->axisZ()->orientation(), QAbstract3DAxis::AxisOrientationZ);

    // Common properties
    QCOMPARE(m_graph->activeTheme()->type(), Q3DTheme::ThemeQt);
    QCOMPARE(m_graph->selectionMode(), QAbstract3DGraph::SelectionItem);
    QCOMPARE(m_graph->shadowQuality(), QAbstract3DGraph::ShadowQualityMedium);
    QVERIFY(m_graph->scene());
    QCOMPARE(m_graph->measureFps(), false);
    QCOMPARE(m_graph->isOrthoProjection(), false);
    QCOMPARE(m_graph->selectedElement(), QAbstract3DGraph::ElementNone);
    QCOMPARE(m_graph->aspectRatio(), 2.0);
    QCOMPARE(m_graph->optimizationHints(), QAbstract3DGraph::OptimizationDefault);
    QCOMPARE(m_graph->isPolar(), false);
    QCOMPARE(m_graph->radialLabelOffset(), 1.0);
    QCOMPARE(m_graph->horizontalAspectRatio(), 0.0);
    QCOMPARE(m_graph->isReflection(), false);
    QCOMPARE(m_graph->reflectivity(), 0.5);
    QCOMPARE(m_graph->locale(), QLocale("C"));
    QCOMPARE(m_graph->queriedGraphPosition(), QVector3D(0, 0, 0));
    QCOMPARE(m_graph->margin(), -1.0);
}

void tst_scatter::initializeProperties()
{
    Q3DTheme *theme = new Q3DTheme(Q3DTheme::ThemeDigia);
    m_graph->setActiveTheme(theme);
    m_graph->setSelectionMode(QAbstract3DGraph::SelectionNone);
    m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualitySoftHigh);
    QCOMPARE(m_graph->shadowQuality(), QAbstract3DGraph::ShadowQualitySoftHigh);
    m_graph->setMeasureFps(true);
    m_graph->setOrthoProjection(true);
    m_graph->setAspectRatio(1.0);
    m_graph->setOptimizationHints(QAbstract3DGraph::OptimizationStatic);
    m_graph->setPolar(true);
    m_graph->setRadialLabelOffset(0.1f);
    m_graph->setHorizontalAspectRatio(1.0);
    m_graph->setReflection(true);
    m_graph->setReflectivity(0.1);
    m_graph->setLocale(QLocale("FI"));
    m_graph->setMargin(1.0);

    QCOMPARE(m_graph->activeTheme()->type(), Q3DTheme::ThemeDigia);
    QCOMPARE(m_graph->selectionMode(), QAbstract3DGraph::SelectionNone);
    QCOMPARE(m_graph->shadowQuality(), QAbstract3DGraph::ShadowQualityNone); // Ortho disables shadows
    QCOMPARE(m_graph->measureFps(), true);
    QCOMPARE(m_graph->isOrthoProjection(), true);
    QCOMPARE(m_graph->aspectRatio(), 1.0);
    QCOMPARE(m_graph->optimizationHints(), QAbstract3DGraph::OptimizationStatic);
    QCOMPARE(m_graph->isPolar(), true);
    QCOMPARE(m_graph->radialLabelOffset(), 0.1f);
    QCOMPARE(m_graph->horizontalAspectRatio(), 1.0);
    QCOMPARE(m_graph->isReflection(), true);
    QCOMPARE(m_graph->reflectivity(), 0.1);
    QCOMPARE(m_graph->locale(), QLocale("FI"));
    QCOMPARE(m_graph->margin(), 1.0);
}

void tst_scatter::invalidProperties()
{
    m_graph->setSelectionMode(QAbstract3DGraph::SelectionColumn | QAbstract3DGraph::SelectionRow | QAbstract3DGraph::SelectionSlice);
    m_graph->setAspectRatio(-1.0);
    m_graph->setHorizontalAspectRatio(-1.0);
    m_graph->setReflectivity(-1.0);
    m_graph->setLocale(QLocale("XX"));

    QCOMPARE(m_graph->selectionMode(), QAbstract3DGraph::SelectionItem);
    QCOMPARE(m_graph->aspectRatio(), -1.0/*2.0*/); // TODO: Fix once QTRD-3367 is done
    QCOMPARE(m_graph->horizontalAspectRatio(), -1.0/*0.0*/); // TODO: Fix once QTRD-3367 is done
    QCOMPARE(m_graph->reflectivity(), -1.0/*0.5*/); // TODO: Fix once QTRD-3367 is done
    QCOMPARE(m_graph->locale(), QLocale("C"));
}

void tst_scatter::addSeries()
{
    m_graph->addSeries(newSeries());

    QCOMPARE(m_graph->seriesList().length(), 1);
    QVERIFY(!m_graph->selectedSeries());
}

void tst_scatter::addMultipleSeries()
{
    QScatter3DSeries *series = newSeries();
    QScatter3DSeries *series2 = newSeries();
    QScatter3DSeries *series3 = newSeries();

    m_graph->addSeries(series);
    m_graph->addSeries(series2);
    m_graph->addSeries(series3);

    QCOMPARE(m_graph->seriesList().length(), 3);
}

void tst_scatter::selectSeries()
{
    QScatter3DSeries *series = newSeries();

    m_graph->addSeries(series);
    m_graph->seriesList()[0]->setSelectedItem(1);

    QCOMPARE(m_graph->seriesList().length(), 1);
    QCOMPARE(m_graph->selectedSeries(), series);

    m_graph->clearSelection();
    QVERIFY(!m_graph->selectedSeries());
}

void tst_scatter::removeSeries()
{
    QScatter3DSeries *series = newSeries();

    m_graph->addSeries(series);
    m_graph->removeSeries(series);
    QCOMPARE(m_graph->seriesList().length(), 0);

    delete series;
}

void tst_scatter::removeMultipleSeries()
{
    QScatter3DSeries *series = newSeries();
    QScatter3DSeries *series2 = newSeries();
    QScatter3DSeries *series3 = newSeries();

    m_graph->addSeries(series);
    m_graph->addSeries(series2);
    m_graph->addSeries(series3);

    m_graph->seriesList()[0]->setSelectedItem(1);
    QCOMPARE(m_graph->selectedSeries(), series);

    m_graph->removeSeries(series);
    QCOMPARE(m_graph->seriesList().length(), 2);
    QVERIFY(!m_graph->selectedSeries());

    m_graph->removeSeries(series2);
    QCOMPARE(m_graph->seriesList().length(), 1);

    m_graph->removeSeries(series3);
    QCOMPARE(m_graph->seriesList().length(), 0);

    delete series;
    delete series2;
    delete series3;
}

QTEST_MAIN(tst_scatter)
#include "tst_scatter.moc"
