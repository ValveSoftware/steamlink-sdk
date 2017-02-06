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

#include <QtDataVisualization/Q3DBars>
#include <QtDataVisualization/QCustom3DItem>
#include <QtDataVisualization/Q3DInputHandler>
#include <QtDataVisualization/QTouch3DInputHandler>

using namespace QtDataVisualization;

class tst_bars: public QObject
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

    // The following tests are not required for scatter or surface, as they are handled identically
    void addInputHandler();
    void removeInputHandler();

    void addTheme();
    void removeTheme();

    void addCustomItem();
    void removeCustomItem();

    void renderToImage();

private:
    Q3DBars *m_graph;
};

QBar3DSeries *newSeries()
{
    QBar3DSeries *series = new QBar3DSeries;
    QBarDataRow *data = new QBarDataRow;
    *data << -1.0f << 3.0f << 7.5f << 5.0f << 2.2f;
    series->dataProxy()->addRow(data);
    return series;
}

void tst_bars::initTestCase()
{
}

void tst_bars::cleanupTestCase()
{
}

void tst_bars::init()
{
    m_graph = new Q3DBars();
}

void tst_bars::cleanup()
{
    delete m_graph;
}

void tst_bars::construct()
{
    Q3DBars *graph = new Q3DBars();
    QVERIFY(graph);
    delete graph;

    QSurfaceFormat format;
    graph = new Q3DBars(&format);
    QVERIFY(graph);
    delete graph;
}

void tst_bars::initialProperties()
{
    QVERIFY(m_graph);
    QCOMPARE(m_graph->isMultiSeriesUniform(), false);
    QCOMPARE(m_graph->barThickness(), 1.0);
    QCOMPARE(m_graph->barSpacing(), QSizeF(1.0f, 1.0f));
    QCOMPARE(m_graph->isBarSpacingRelative(), true);
    QCOMPARE(m_graph->seriesList().length(), 0);
    QVERIFY(!m_graph->selectedSeries());
    QVERIFY(!m_graph->primarySeries());
    QCOMPARE(m_graph->floorLevel(), 0.0);
    QCOMPARE(m_graph->columnAxis()->orientation(), QAbstract3DAxis::AxisOrientationX);
    QCOMPARE(m_graph->valueAxis()->orientation(), QAbstract3DAxis::AxisOrientationY);
    QCOMPARE(m_graph->rowAxis()->orientation(), QAbstract3DAxis::AxisOrientationZ);

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

void tst_bars::initializeProperties()
{
    QVERIFY(m_graph);

    m_graph->setMultiSeriesUniform(true);
    m_graph->setBarThickness(0.2f);
    m_graph->setBarSpacing(QSizeF(0.1f, 0.1f));
    m_graph->setBarSpacingRelative(false);
    m_graph->setFloorLevel(1.0f);

    QCOMPARE(m_graph->isMultiSeriesUniform(), true);
    QCOMPARE(m_graph->barThickness(), 0.2f);
    QCOMPARE(m_graph->barSpacing(), QSizeF(0.1f, 0.1f));
    QCOMPARE(m_graph->isBarSpacingRelative(), false);
    QCOMPARE(m_graph->floorLevel(), 1.0f);

    Q3DTheme *theme = new Q3DTheme(Q3DTheme::ThemeDigia);
    m_graph->setActiveTheme(theme);
    m_graph->setSelectionMode(QAbstract3DGraph::SelectionItem | QAbstract3DGraph::SelectionRow | QAbstract3DGraph::SelectionSlice);
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
    QCOMPARE(m_graph->selectionMode(), QAbstract3DGraph::SelectionItem | QAbstract3DGraph::SelectionRow | QAbstract3DGraph::SelectionSlice);
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

void tst_bars::invalidProperties()
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

void tst_bars::addSeries()
{
    QBar3DSeries *series = newSeries();

    m_graph->addSeries(series);

    QCOMPARE(m_graph->seriesList().length(), 1);
    QVERIFY(!m_graph->selectedSeries());
    QCOMPARE(m_graph->primarySeries(), series);
}

void tst_bars::addMultipleSeries()
{
    QBar3DSeries *series = newSeries();
    QBar3DSeries *series2 = newSeries();
    QBar3DSeries *series3 = newSeries();

    m_graph->addSeries(series);
    m_graph->addSeries(series2);
    m_graph->addSeries(series3);

    QCOMPARE(m_graph->seriesList().length(), 3);
    QCOMPARE(m_graph->primarySeries(), series);

    m_graph->setPrimarySeries(series2);

    QCOMPARE(m_graph->primarySeries(), series2);
}

void tst_bars::selectSeries()
{
    QBar3DSeries *series = newSeries();

    m_graph->addSeries(series);
    m_graph->primarySeries()->setSelectedBar(QPoint(0, 0));

    QCOMPARE(m_graph->seriesList().length(), 1);
    QCOMPARE(m_graph->selectedSeries(), series);

    m_graph->clearSelection();
    QVERIFY(!m_graph->selectedSeries());
}

void tst_bars::removeSeries()
{
    QBar3DSeries *series = newSeries();

    m_graph->addSeries(series);
    m_graph->removeSeries(series);
    QCOMPARE(m_graph->seriesList().length(), 0);
    delete series;
}

void tst_bars::removeMultipleSeries()
{
    QBar3DSeries *series = newSeries();
    QBar3DSeries *series2 = newSeries();
    QBar3DSeries *series3 = newSeries();

    m_graph->addSeries(series);
    m_graph->addSeries(series2);
    m_graph->addSeries(series3);

    m_graph->primarySeries()->setSelectedBar(QPoint(0, 0));
    QCOMPARE(m_graph->selectedSeries(), series);

    m_graph->removeSeries(series);
    QCOMPARE(m_graph->seriesList().length(), 2);
    QCOMPARE(m_graph->primarySeries(), series2);
    QVERIFY(!m_graph->selectedSeries());

    m_graph->removeSeries(series2);
    QCOMPARE(m_graph->seriesList().length(), 1);
    QCOMPARE(m_graph->primarySeries(), series3);

    m_graph->removeSeries(series3);
    QCOMPARE(m_graph->seriesList().length(), 0);

    delete series;
    delete series2;
    delete series3;
}

// The following tests are not required for scatter or surface, as they are handled identically
void tst_bars::addInputHandler()
{
    Q3DInputHandler *handler = new Q3DInputHandler();
    QTouch3DInputHandler *handler2 = new QTouch3DInputHandler();
    QAbstract3DInputHandler *initialHandler = m_graph->activeInputHandler();

    m_graph->addInputHandler(handler);
    m_graph->addInputHandler(handler2);

    QCOMPARE(m_graph->inputHandlers().length(), 3); // Default, as it is still active, plus added ones
    QCOMPARE(m_graph->activeInputHandler(), initialHandler);
    m_graph->setActiveInputHandler(handler2);
    QCOMPARE(m_graph->activeInputHandler(), handler2);

    m_graph->setActiveInputHandler(NULL);
    QVERIFY(!m_graph->activeInputHandler());
    QCOMPARE(m_graph->inputHandlers().length(), 2);
}

void tst_bars::removeInputHandler()
{
    Q3DInputHandler *handler = new Q3DInputHandler();
    QTouch3DInputHandler *handler2 = new QTouch3DInputHandler();

    m_graph->addInputHandler(handler);
    m_graph->addInputHandler(handler2);

    m_graph->setActiveInputHandler(handler2);
    QCOMPARE(m_graph->inputHandlers().length(), 2); // Default handler removed by previous call
    QCOMPARE(m_graph->activeInputHandler(), handler2);
    m_graph->releaseInputHandler(handler2);
    QCOMPARE(m_graph->inputHandlers().length(), 1);
    m_graph->releaseInputHandler(handler);
    QCOMPARE(m_graph->inputHandlers().length(), 0);

    delete handler2;
    delete handler;
}

void tst_bars::addTheme()
{
    Q3DTheme *theme = new Q3DTheme(Q3DTheme::ThemeDigia);
    Q3DTheme *theme2 = new Q3DTheme();
    Q3DTheme *initialTheme = m_graph->activeTheme();
    m_graph->addTheme(theme);
    m_graph->addTheme(theme2);

    QCOMPARE(m_graph->themes().length(), 3); // Default, plus added ones
    QCOMPARE(m_graph->activeTheme(), initialTheme);
    m_graph->setActiveTheme(theme2);
    QCOMPARE(m_graph->activeTheme(), theme2);
}

void tst_bars::removeTheme()
{
    Q3DTheme *theme = new Q3DTheme(Q3DTheme::ThemeDigia);
    Q3DTheme *theme2 = new Q3DTheme();
    m_graph->addTheme(theme);
    m_graph->addTheme(theme2);

    m_graph->setActiveTheme(theme2);
    QCOMPARE(m_graph->activeTheme(), theme2);
    m_graph->releaseTheme(theme2);
    QCOMPARE(m_graph->themes().length(), 2);
    m_graph->releaseTheme(theme);
    QCOMPARE(m_graph->themes().length(), 1); // Default theme remains

    delete theme2;
    delete theme;
}

void tst_bars::addCustomItem()
{
    QCustom3DItem *item = new QCustom3DItem();
    QCustom3DItem *item2 = new QCustom3DItem();

    m_graph->addCustomItem(item);
    QCOMPARE(m_graph->customItems().length(), 1);
    m_graph->addCustomItem(item2);
    QCOMPARE(m_graph->customItems().length(), 2);
}

void tst_bars::removeCustomItem()
{
    QCustom3DItem *item = new QCustom3DItem();
    QCustom3DItem *item2 = new QCustom3DItem();
    QCustom3DItem *item3 = new QCustom3DItem();
    item3->setPosition(QVector3D(1, 1, 1));

    m_graph->addCustomItem(item);
    m_graph->addCustomItem(item2);
    m_graph->addCustomItem(item3);

    m_graph->releaseCustomItem(item);
    QCOMPARE(m_graph->customItems().length(), 2);
    m_graph->removeCustomItem(item2);
    QCOMPARE(m_graph->customItems().length(), 1);
    m_graph->addCustomItem(item);
    m_graph->removeCustomItemAt(QVector3D(1, 1, 1));
    QCOMPARE(m_graph->customItems().length(), 1);
    m_graph->removeCustomItems();
    QCOMPARE(m_graph->customItems().length(), 0);
}

void tst_bars::renderToImage()
{
    /* Crashes on some CI machines using Mesa, but can't repro locally, so commented out for now.
    m_graph->addSeries(newSeries());

    QImage image = m_graph->renderToImage();
    QCOMPARE(image.size(), m_graph->size());

    image = m_graph->renderToImage(8);
    QCOMPARE(image.size(), m_graph->size());

    image = m_graph->renderToImage(4, QSize(300, 300));
    QCOMPARE(image.size(), QSize(300, 300));
    */
}

QTEST_MAIN(tst_bars)
#include "tst_bars.moc"
