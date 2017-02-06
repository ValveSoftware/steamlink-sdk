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

#include "scatterchart.h"
#include <QtDataVisualization/qscatterdataproxy.h>
#include <QtDataVisualization/qscatter3dseries.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtDataVisualization/Q3DInputHandler>
#include <qmath.h>
using namespace QtDataVisualization;

//#define RANDOM_SCATTER

const int numberOfItems = 10000;

ScatterDataModifier::ScatterDataModifier(Q3DScatter *scatter)
    : m_chart(scatter),
      m_fontSize(30.0f),
      m_loopCounter(0),
      m_selectedItem(-1),
      m_targetSeries(0)
{
    m_chart->activeTheme()->setType(Q3DTheme::ThemeStoneMoss);
    QFont font = m_chart->activeTheme()->font();
    font.setPointSize(m_fontSize);
    m_chart->activeTheme()->setFont(font);
    m_chart->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
    m_chart->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFront);
    m_chart->setAxisX(new QValue3DAxis);
    m_chart->setAxisY(new QValue3DAxis);
    m_chart->setAxisZ(new QValue3DAxis);
    m_chart->axisY()->setLabelFormat(QStringLiteral("%.7f"));
    static_cast<Q3DInputHandler *>(m_chart->activeInputHandler())->setZoomAtTargetEnabled(true);

    createAndAddSeries();
    createAndAddSeries();

    m_chart->setSelectionMode(QAbstract3DGraph::SelectionItem);

    QObject::connect(&m_timer, &QTimer::timeout, this, &ScatterDataModifier::timeout);
    QObject::connect(m_chart, &Q3DScatter::shadowQualityChanged, this,
                     &ScatterDataModifier::shadowQualityUpdatedByVisual);

    QObject::connect(m_chart, &Q3DScatter::axisXChanged, this,
                     &ScatterDataModifier::handleAxisXChanged);
    QObject::connect(m_chart, &Q3DScatter::axisYChanged, this,
                     &ScatterDataModifier::handleAxisYChanged);
    QObject::connect(m_chart, &Q3DScatter::axisZChanged, this,
                     &ScatterDataModifier::handleAxisZChanged);
    QObject::connect(m_chart, &QAbstract3DGraph::currentFpsChanged, this,
                     &ScatterDataModifier::handleFpsChange);
}

ScatterDataModifier::~ScatterDataModifier()
{
    delete m_chart;
}

void ScatterDataModifier::start()
{
    addData();
}

static const int itemsPerUnit = 100; // "unit" is one unit range along Z-axis

void ScatterDataModifier::massiveDataTest()
{
    static int testPhase = 0;
    static QTimer *massiveTestTimer = 0;

    if (!massiveTestTimer)
        massiveTestTimer = new QTimer;

    int items = 1000000;
    int visibleRange = 200;
    int unitCount = items / itemsPerUnit;
    int cacheSize = visibleRange * itemsPerUnit * 5;

    switch (testPhase) {
    case 0: {
        float yRangeMin = 0.0f;
        float yRangeMax = 1.0f;
        float yRangeMargin = 0.05f;
        float minY = yRangeMin + yRangeMargin;
        float maxY = yRangeMax - yRangeMargin;
        float unitBase = minY;
        float direction = 1.0f;

        if (!m_massiveTestCacheArray.size()) {
            m_massiveTestCacheArray.resize(cacheSize);
            int totalIndex = 0;
            for (int i = 0; i < unitCount && totalIndex < cacheSize; i++) {
                unitBase += direction * (float(rand() % 3) / 100.0f);
                if (unitBase > maxY) {
                    unitBase = maxY;
                    direction = -1.0f;
                } else if (unitBase < minY) {
                    unitBase = minY;
                    direction = 1.0f;
                }
                for (int j = 0; j < itemsPerUnit && totalIndex < cacheSize; j++) {
                    float randFactor = float(rand() % 100) / (100 / yRangeMargin);
                    m_massiveTestCacheArray[totalIndex].setPosition(
                                QVector3D(float(qrand() % itemsPerUnit),
                                          unitBase + randFactor, 0.0f));
                    // Z value is irrelevant, we replace it anyway when we take item to use
                    totalIndex++;
                }
            }
        }

        qDebug() << __FUNCTION__ << testPhase << ": Setting the graph up...";
        QValue3DAxis *xAxis = new QValue3DAxis();
        QValue3DAxis *yAxis = new QValue3DAxis();
        QValue3DAxis *zAxis = new QValue3DAxis();
        xAxis->setRange(0.0f, float(itemsPerUnit - 1));
        yAxis->setRange(yRangeMin, yRangeMax);
        zAxis->setRange(0.0f, float(visibleRange - 1));
        xAxis->setSegmentCount(1);
        yAxis->setSegmentCount(1);
        zAxis->setSegmentCount(1);
        m_chart->setAxisX(xAxis);
        m_chart->setAxisY(yAxis);
        m_chart->setAxisZ(zAxis);
        m_chart->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetRight);
        m_chart->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
        foreach (QAbstract3DSeries *series, m_chart->seriesList())
            m_chart->removeSeries(static_cast<QScatter3DSeries *>(series));

        qDebug() << __FUNCTION__ << testPhase << ": Creating massive array..." << items;
        QScatterDataArray *massiveArray = new QScatterDataArray;
        massiveArray->resize(items);

        int cacheIndex = 0;
        for (int i = 0; i < items; i++) {
            // Use qreals for precicion as the numbers can overflow int
            float currentZ = float(qreal(i) * qreal(unitCount) / qreal(items));
            (*massiveArray)[i] = m_massiveTestCacheArray.at(cacheIndex++);
            (*massiveArray)[i].setZ(currentZ);
            if (cacheIndex >= cacheSize)
                cacheIndex = 0;
        }
        qDebug() << __FUNCTION__ << testPhase << ": Massive array creation finished!";

        QScatter3DSeries *series =  new QScatter3DSeries;
        series->dataProxy()->resetArray(massiveArray);
        series->setMesh(QAbstract3DSeries::MeshPoint);
        m_chart->addSeries(series);
        break;
    }
    case 1: {
        qDebug() << __FUNCTION__ << testPhase << ": Scroll";
        QObject::disconnect(massiveTestTimer, 0, this, 0);
        QObject::connect(massiveTestTimer, &QTimer::timeout, this,
                         &ScatterDataModifier::massiveTestScroll);
        massiveTestTimer->start(16);
        break;
    }
    case 2: {
        qDebug() << __FUNCTION__ << testPhase << ": Append and scroll";
        massiveTestTimer->stop();
        QObject::disconnect(massiveTestTimer, 0, this, 0);
        QObject::connect(massiveTestTimer, &QTimer::timeout, this,
                         &ScatterDataModifier::massiveTestAppendAndScroll);
        m_chart->axisZ()->setRange(unitCount - visibleRange, unitCount);
        massiveTestTimer->start(16);
        break;
    }
    default:
        QObject::disconnect(massiveTestTimer, 0, this, 0);
        massiveTestTimer->stop();
        qDebug() << __FUNCTION__ << testPhase << ": Resetting the test";
        testPhase = -1;
    }
    testPhase++;
}

void ScatterDataModifier::massiveTestScroll()
{
    const int scrollAmount = 20;
    int itemCount = m_chart->seriesList().at(0)->dataProxy()->itemCount();
    int min = m_chart->axisZ()->min() + scrollAmount;
    int max = m_chart->axisZ()->max() + scrollAmount;
    if (max >= itemCount / itemsPerUnit) {
        max = max - min - 1;
        min = 0;
    }
    m_chart->axisZ()->setRange(min, max);
}

void ScatterDataModifier::massiveTestAppendAndScroll()
{
    const int addedUnits = 50;
    const int addedItems = itemsPerUnit * addedUnits;
    int cacheSize = m_massiveTestCacheArray.size();
    int itemCount = m_chart->seriesList().at(0)->dataProxy()->itemCount();
    static int cacheIndex = 0;

    // Copy items from cache
    QScatterDataArray appendArray;
    appendArray.resize(addedItems);

    float zOffset = m_chart->seriesList().at(0)->dataProxy()->itemAt(itemCount - 1)->z();
    for (int i = 0; i < addedItems; i++) {
        float currentZ = zOffset + float(qreal(i) * qreal(addedUnits) / qreal(addedItems));
        appendArray[i] = m_massiveTestCacheArray.at(cacheIndex++);
        appendArray[i].setZ(currentZ);
        if (cacheIndex >= cacheSize)
            cacheIndex = 0;
    }

    m_chart->seriesList().at(0)->dataProxy()->addItems(appendArray);
    int min = m_chart->axisZ()->min() + addedUnits;
    int max = m_chart->axisZ()->max() + addedUnits;
    m_chart->axisZ()->setRange(min, max);
}

void ScatterDataModifier::setFpsMeasurement(bool enable)
{
    m_chart->setMeasureFps(enable);
}

void ScatterDataModifier::testItemChanges()
{
    static int counter = 0;
    const int rowCount = 12;
    const int colCount = 10;
    static QScatter3DSeries *series0 = 0;
    static QScatter3DSeries *series1 = 0;
    static QScatter3DSeries *series2 = 0;

    switch (counter) {
    case 0: {
        qDebug() << __FUNCTION__ << counter << "Setup test";
        foreach (QScatter3DSeries *series, m_chart->seriesList())
            m_chart->removeSeries(series);
        foreach (QValue3DAxis *axis, m_chart->axes())
            deleteAxis(axis);
        delete series0;
        delete series1;
        delete series2;
        series0 = new QScatter3DSeries;
        series1 = new QScatter3DSeries;
        series2 = new QScatter3DSeries;
        populateFlatSeries(series0, rowCount, colCount, 10.0f);
        populateFlatSeries(series1, rowCount, colCount, 30.0f);
        populateFlatSeries(series2, rowCount, colCount, 50.0f);
        m_chart->axisX()->setRange(3.0f, 6.0f);
        m_chart->axisY()->setRange(0.0f, 100.0f);
        m_chart->axisZ()->setRange(4.0f, 8.0f);
        m_chart->addSeries(series0);
        m_chart->addSeries(series1);
        m_chart->addSeries(series2);
    }
        break;
    case 1: {
        qDebug() << __FUNCTION__ << counter << "Change single item, unselected";
        int itemIndex = 3 * colCount + 5;
        QScatterDataItem item = *series0->dataProxy()->itemAt(itemIndex);
        item.setY(75.0f);
        series0->dataProxy()->setItem(itemIndex, item);
    }
        break;
    case 2: {
        qDebug() << __FUNCTION__ << counter << "Change single item, selected";
        int itemIndex = 4 * colCount + 4;
        series1->setSelectedItem(itemIndex);
        QScatterDataItem item = *series1->dataProxy()->itemAt(itemIndex);
        item.setY(75.0f);
        series1->dataProxy()->setItem(itemIndex, item);
    }
        break;
    case 3: {
        qDebug() << __FUNCTION__ << counter << "Change item outside visible area";
        int itemIndex = 2;
        QScatterDataItem item = *series1->dataProxy()->itemAt(itemIndex);
        item.setY(75.0f);
        series1->dataProxy()->setItem(itemIndex, item);
    }
        break;
    case 4: {
        qDebug() << __FUNCTION__ << counter << "Change single item from two series, unselected";
        int itemIndex = 4 * colCount + 6;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex);
        QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex);
        item0.setY(65.0f);
        item1.setY(85.0f);
        series0->dataProxy()->setItem(itemIndex, item0);
        series1->dataProxy()->setItem(itemIndex, item1);
    }
        break;
    case 5: {
        qDebug() << __FUNCTION__ << counter << "Change single item from two series, one selected";
        int itemIndex0 = 5 * colCount + 5;
        int itemIndex1 = 4 * colCount + 4;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex0);
        QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex1);
        item0.setY(65.0f);
        item1.setY(85.0f);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series1->dataProxy()->setItem(itemIndex1, item1);
    }
        break;
    case 6: {
        qDebug() << __FUNCTION__ << counter << "Change single item from two series, one outside range";
        int itemIndex0 = 6 * colCount + 6;
        int itemIndex1 = 9 * colCount + 2;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex0);
        QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex1);
        item0.setY(65.0f);
        item1.setY(85.0f);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series1->dataProxy()->setItem(itemIndex1, item1);
    }
        break;
    case 7: {
        qDebug() << __FUNCTION__ << counter << "Change single item from two series, both outside range";
        int itemIndex0 = 1 * colCount + 3;
        int itemIndex1 = 9 * colCount + 2;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex0);
        QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex1);
        item0.setY(65.0f);
        item1.setY(85.0f);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series1->dataProxy()->setItem(itemIndex1, item1);
    }
        break;
    case 8: {
        qDebug() << __FUNCTION__ << counter << "Change item to same value as previously";
        int itemIndex0 = 5 * colCount + 7;
        int itemIndex1 = 4 * colCount + 7;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex0);
        QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex1);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series1->dataProxy()->setItem(itemIndex1, item1);
    }
        break;
    case 9: {
        qDebug() << __FUNCTION__ << counter << "Change 3 items on each series";
        int itemIndex0 = 5 * colCount + 6;
        int itemIndex1 = 4 * colCount + 6;
        QScatterDataItem item00 = *series0->dataProxy()->itemAt(itemIndex0);
        QScatterDataItem item01 = *series0->dataProxy()->itemAt(itemIndex0 + 1);
        QScatterDataItem item02 = *series0->dataProxy()->itemAt(itemIndex0 + 2);
        QScatterDataItem item10 = *series1->dataProxy()->itemAt(itemIndex1);
        QScatterDataItem item11 = *series1->dataProxy()->itemAt(itemIndex1 + 1);
        QScatterDataItem item12 = *series1->dataProxy()->itemAt(itemIndex1 + 2);
        item00.setY(65.0f);
        item01.setY(70.0f);
        item02.setY(75.0f);
        item10.setY(80.0f);
        item11.setY(85.0f);
        item12.setY(90.0f);
        series0->dataProxy()->setItem(itemIndex0, item00);
        series0->dataProxy()->setItem(itemIndex0 + 1, item01);
        series0->dataProxy()->setItem(itemIndex0 + 2, item02);
        series1->dataProxy()->setItem(itemIndex1, item10);
        series1->dataProxy()->setItem(itemIndex1 + 1, item11);
        series1->dataProxy()->setItem(itemIndex1 + 2, item12);
    }
        break;
    case 10: {
        qDebug() << __FUNCTION__ << counter << "Level the field single item at a time";
        QScatterDataItem item;
        for (int i = 0; i < rowCount; i++) {
            for (int j = 0; j < colCount; j++) {
                int itemIndex = i * colCount + j;
                QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex);
                QScatterDataItem item1 = *series1->dataProxy()->itemAt(itemIndex);
                QScatterDataItem item2 = *series2->dataProxy()->itemAt(itemIndex);
                item0.setY(10.0f);
                item1.setY(15.0f);
                item2.setY(20.0f);
                series0->dataProxy()->setItem(itemIndex, item0);
                series1->dataProxy()->setItem(itemIndex, item1);
                series2->dataProxy()->setItem(itemIndex, item2);
            }
        }
    }
        break;
    case 11: {
        qDebug() << __FUNCTION__ << counter << "Change same items multiple times";
        int itemIndex0 = 6 * colCount + 6;
        QScatterDataItem item0 = *series0->dataProxy()->itemAt(itemIndex0);
        item0.setY(90.0f);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series0->dataProxy()->setItem(itemIndex0, item0);
        series0->dataProxy()->setItem(itemIndex0, item0);
    }
        break;
    default:
        qDebug() << __FUNCTION__ << "Resetting test";
        counter = -1;
    }
    counter++;
}

void ScatterDataModifier::testAxisReverse()
{
    static int counter = 0;
    const int rowCount = 16;
    const int colCount = 16;
    static QScatter3DSeries *series0 = 0;
    static QScatter3DSeries *series1 = 0;

    switch (counter) {
    case 0: {
        qDebug() << __FUNCTION__ << counter << "Setup test";
        foreach (QScatter3DSeries *series, m_chart->seriesList())
            m_chart->removeSeries(series);
        foreach (QValue3DAxis *axis, m_chart->axes())
            deleteAxis(axis);
        delete series0;
        delete series1;
        series0 = new QScatter3DSeries;
        series1 = new QScatter3DSeries;
        populateRisingSeries(series0, rowCount, colCount, 0.0f, 50.0f);
        populateRisingSeries(series1, rowCount, colCount, -20.0f, 30.0f);
        m_chart->axisX()->setRange(0.0f, 10.0f);
        m_chart->axisY()->setRange(-20.0f, 50.0f);
        m_chart->axisZ()->setRange(5.0f, 15.0f);
        m_chart->axisX()->setTitle("Axis X");
        m_chart->axisZ()->setTitle("Axis Z");
        m_chart->axisX()->setTitleVisible(true);
        m_chart->axisZ()->setTitleVisible(true);
        m_chart->addSeries(series0);
        m_chart->addSeries(series1);
    }
        break;
    case 1: {
        qDebug() << __FUNCTION__ << counter << "Reverse X axis";
        m_chart->axisX()->setReversed(true);
    }
        break;
    case 2: {
        qDebug() << __FUNCTION__ << counter << "Reverse Y axis";
        m_chart->axisY()->setReversed(true);
    }
        break;
    case 3: {
        qDebug() << __FUNCTION__ << counter << "Reverse Z axis";
        m_chart->axisZ()->setReversed(true);
    }
        break;
    case 4: {
        qDebug() << __FUNCTION__ << counter << "Return all axes to normal";
        m_chart->axisX()->setReversed(false);
        m_chart->axisY()->setReversed(false);
        m_chart->axisZ()->setReversed(false);
    }
        break;
    case 5: {
        qDebug() << __FUNCTION__ << counter << "Reverse all axes";
        m_chart->axisX()->setReversed(true);
        m_chart->axisY()->setReversed(true);
        m_chart->axisZ()->setReversed(true);
    }
        break;
    default:
        qDebug() << __FUNCTION__ << "Resetting test";
        counter = -1;
    }
    counter++;
}

void ScatterDataModifier::addData()
{
    // Add labels
    m_chart->axisX()->setTitle("X - Axis");
    m_chart->axisY()->setTitle("Y - Axis");
    m_chart->axisZ()->setTitle("Z - Axis");
    m_chart->axisX()->setRange(-50.0f, 50.0f);
    m_chart->axisY()->setRange(-1.0f, 1.2f);
    m_chart->axisZ()->setRange(-50.0f, 50.0f);
    m_chart->axisX()->setSegmentCount(5);
    m_chart->axisY()->setSegmentCount(4);
    m_chart->axisZ()->setSegmentCount(10);
    m_chart->axisX()->setSubSegmentCount(2);
    m_chart->axisY()->setSubSegmentCount(3);
    m_chart->axisZ()->setSubSegmentCount(1);

    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize(numberOfItems);
    QScatterDataItem *ptrToDataArray = &dataArray->first();
    QScatterDataArray *dataArray2 = new QScatterDataArray;
    dataArray2->resize(numberOfItems);
    QScatterDataItem *ptrToDataArray2 = &dataArray2->first();

#ifdef RANDOM_SCATTER
    for (int i = 0; i < numberOfItems; i++) {
        ptrToDataArray->setPosition(randVector());
        ptrToDataArray++;
        ptrToDataArray2->setPosition(randVector());
        ptrToDataArray2++;
    }
#else
    float limit = qSqrt(numberOfItems) / 2.0f;
    for (float i = -limit; i < limit; i++) {
        for (float j = -limit; j < limit; j++) {
            ptrToDataArray->setPosition(QVector3D(i, qCos(qDegreesToRadians((i * j) / 7.5)), j));
            ptrToDataArray++;
            ptrToDataArray2->setPosition(QVector3D(i, qCos(qDegreesToRadians((i * j) / 7.5)) + 0.2, j));
            ptrToDataArray2++;
        }
    }
#endif

    m_chart->seriesList().at(0)->dataProxy()->resetArray(dataArray);
    m_chart->seriesList().at(1)->dataProxy()->resetArray(dataArray2);
    m_chart->seriesList().at(0)->setItemSize(0.0f);
    m_chart->seriesList().at(1)->setItemSize(0.0f);
}

void ScatterDataModifier::changeStyle()
{
    if (!m_targetSeries)
        createAndAddSeries();

    if (m_targetSeries->isMeshSmooth()) {
        m_targetSeries->setMeshSmooth(false);
        switch (m_targetSeries->mesh()) {
        case QAbstract3DSeries::MeshCube:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshPyramid);
            break;
        case QAbstract3DSeries::MeshPyramid:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshCone);
            break;
        case QAbstract3DSeries::MeshCone:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshCylinder);
            break;
        case QAbstract3DSeries::MeshCylinder:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshBevelCube);
            break;
        case QAbstract3DSeries::MeshBevelCube:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshSphere);
            break;
        case QAbstract3DSeries::MeshSphere:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshMinimal);
            break;
        case QAbstract3DSeries::MeshMinimal:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshPoint);
            break;
        default:
            m_targetSeries->setMesh(QAbstract3DSeries::MeshCube);
            break;
        }
    } else {
        m_targetSeries->setMeshSmooth(true);
    }

    qDebug() << __FUNCTION__ << m_targetSeries->mesh() << m_targetSeries->isMeshSmooth();
}

void ScatterDataModifier::changePresetCamera()
{
    static int preset = Q3DCamera::CameraPresetFrontLow;

    m_chart->scene()->activeCamera()->setCameraPreset((Q3DCamera::CameraPreset)preset);

    if (++preset > Q3DCamera::CameraPresetDirectlyAboveCCW45)
        preset = Q3DCamera::CameraPresetFrontLow;
}

void ScatterDataModifier::changeTheme()
{
    static int theme = Q3DTheme::ThemeQt;

    m_chart->activeTheme()->setType(Q3DTheme::Theme(theme));

    if (++theme > Q3DTheme::ThemeIsabelle)
        theme = Q3DTheme::ThemeQt;
}

void ScatterDataModifier::changeLabelStyle()
{
    m_chart->activeTheme()->setLabelBackgroundEnabled(!m_chart->activeTheme()->isLabelBackgroundEnabled());
}

void ScatterDataModifier::changeFont(const QFont &font)
{
    QFont newFont = font;
    newFont.setPointSizeF(m_fontSize);
    m_chart->activeTheme()->setFont(newFont);
}

void ScatterDataModifier::changeFontSize(int fontSize)
{
    m_fontSize = fontSize;
    QFont font = m_chart->activeTheme()->font();
    font.setPointSize(m_fontSize);
    m_chart->activeTheme()->setFont(font);
}

void ScatterDataModifier::changePointSize(int pointSize)
{
    m_targetSeries->setItemSize(0.01f *  float(pointSize));
}

void ScatterDataModifier::shadowQualityUpdatedByVisual(QAbstract3DGraph::ShadowQuality sq)
{
    int quality = int(sq);
     // Updates the UI component to show correct shadow quality
    emit shadowQualityChanged(quality);
}

void ScatterDataModifier::clear()
{
    foreach (QScatter3DSeries *series, m_chart->seriesList()) {
        m_chart->removeSeries(series);
        delete series;
    }

    m_targetSeries = 0;

    qDebug() << m_loopCounter << "Cleared array";
}

void ScatterDataModifier::deleteAxis(QValue3DAxis *axis)
{
    m_chart->releaseAxis(axis);
    delete axis;
}

void ScatterDataModifier::resetAxes()
{
    deleteAxis(m_chart->axisX());
    deleteAxis(m_chart->axisY());
    deleteAxis(m_chart->axisZ());

    m_chart->setAxisX(new QValue3DAxis);
    m_chart->setAxisY(new QValue3DAxis);
    m_chart->setAxisZ(new QValue3DAxis);
    m_chart->axisX()->setSegmentCount(6);
    m_chart->axisY()->setSegmentCount(4);
    m_chart->axisZ()->setSegmentCount(9);
    m_chart->axisX()->setSubSegmentCount(2);
    m_chart->axisY()->setSubSegmentCount(3);
    m_chart->axisZ()->setSubSegmentCount(1);
    m_chart->axisX()->setTitle("X");
    m_chart->axisY()->setTitle("Y");
    m_chart->axisZ()->setTitle("Z");
}

void ScatterDataModifier::addOne()
{
    if (!m_targetSeries)
        createAndAddSeries();

    QScatterDataItem item(randVector());
    int addIndex = m_targetSeries->dataProxy()->addItem(item);
    qDebug() << m_loopCounter << "added one to index:" << addIndex << "array size:" << m_targetSeries->dataProxy()->array()->size();
}

void ScatterDataModifier::addBunch()
{
    if (!m_targetSeries)
        createAndAddSeries();

    QScatterDataArray items(100);
    for (int i = 0; i < items.size(); i++)
        items[i].setPosition(randVector());
    int addIndex = m_targetSeries->dataProxy()->addItems(items);
    qDebug() << m_loopCounter << "added bunch to index:" << addIndex << "array size:" << m_targetSeries->dataProxy()->array()->size();
}

void ScatterDataModifier::insertOne()
{
    if (!m_targetSeries)
        createAndAddSeries();

    QScatterDataItem item(randVector());
    m_targetSeries->dataProxy()->insertItem(0, item);
    qDebug() << m_loopCounter << "Inserted one, array size:" << m_targetSeries->dataProxy()->array()->size();
}

void ScatterDataModifier::insertBunch()
{
    if (!m_targetSeries)
        createAndAddSeries();

    QScatterDataArray items(100);
    for (int i = 0; i < items.size(); i++)
        items[i].setPosition(randVector());
    m_targetSeries->dataProxy()->insertItems(0, items);
    qDebug() << m_loopCounter << "Inserted bunch, array size:" << m_targetSeries->dataProxy()->array()->size();
}

void ScatterDataModifier::changeOne()
{
    if (!m_targetSeries)
        createAndAddSeries();

    if (m_selectedItem >= 0 && m_selectedItem < m_targetSeries->dataProxy()->itemCount()) {
        QScatterDataItem item(randVector());
        m_targetSeries->dataProxy()->setItem(m_selectedItem, item);
        qDebug() << m_loopCounter << "Changed one, array size:" << m_targetSeries->dataProxy()->array()->size();
    }
}

void ScatterDataModifier::changeBunch()
{
    if (!m_targetSeries)
        createAndAddSeries();

    if (m_targetSeries->dataProxy()->array()->size()) {
        int amount = qMin(m_targetSeries->dataProxy()->array()->size(), 100);
        QScatterDataArray items(amount);
        for (int i = 0; i < items.size(); i++) {
            items[i].setPosition(randVector());
            // Change the Y-values of first few items to exact gradient boundaries
            if (i == 0)
                items[i].setY(0.65f);
            else  if (i == 1)
                items[i].setY(0.1f);
            else  if (i == 2)
                items[i].setY(-0.45f);
            else  if (i == 3)
                items[i].setY(-1.0f);
            else  if (i == 4)
                items[i].setY(1.2f);
//            else
//                items[i].setY(0.1001f - (0.00001f * float(i)));

        }

        m_targetSeries->dataProxy()->setItems(0, items);
        qDebug() << m_loopCounter << "Changed bunch, array size:" << m_targetSeries->dataProxy()->array()->size();
    }
}

void ScatterDataModifier::removeOne()
{
    if (!m_targetSeries)
        createAndAddSeries();

    if (m_selectedItem >= 0) {
        m_targetSeries->dataProxy()->removeItems(m_selectedItem, 1);
        qDebug() << m_loopCounter << "Removed one, array size:" << m_targetSeries->dataProxy()->array()->size();
    }
}

void ScatterDataModifier::removeBunch()
{
    if (!m_targetSeries)
        createAndAddSeries();

    m_targetSeries->dataProxy()->removeItems(0, 100);
    qDebug() << m_loopCounter << "Removed bunch, array size:" << m_targetSeries->dataProxy()->array()->size();
}

void ScatterDataModifier::timeout()
{
    int doWhat = rand() % 10;
    if (!(rand() % 100))
        doWhat = -1;

    switch (doWhat) {
    case 0:
        addOne();
        break;
    case 1:
        addBunch();
        break;
    case 2:
        insertOne();
        break;
    case 3:
        insertBunch();
        break;
    case 4:
        changeOne();
        break;
    case 5:
        changeBunch();
        break;
    case 6:
        removeOne();
        break;
    case 7:
        removeBunch();
        break;
    case 8:
        addSeries();
        break;
    case 9:
        if (m_chart->seriesList().size())
            m_targetSeries = m_chart->seriesList().at(rand() % m_chart->seriesList().size());
        else
            addSeries();
        break;
    default:
        clear();
        break;
    }

    m_loopCounter++;
}

void ScatterDataModifier::startStopTimer()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    } else {
        clear();
        m_loopCounter = 0;
        m_timer.start(0);
    }
}

void ScatterDataModifier::selectItem()
{
    if (!m_targetSeries)
        createAndAddSeries();

    int targetItem(3);
    int noSelection(-1);
    if (m_selectedItem != targetItem || m_targetSeries != m_chart->seriesList().at(0))
        m_chart->seriesList().at(0)->setSelectedItem(targetItem);
    else
        m_chart->seriesList().at(0)->setSelectedItem(noSelection);
}

void ScatterDataModifier::handleSelectionChange(int index)
{
    m_selectedItem = index;
    m_targetSeries = static_cast<QScatter3DSeries *>(sender());
    int seriesIndex = 0;
    foreach (QScatter3DSeries *series, m_chart->seriesList()) {
        if (series == sender())
            break;
        seriesIndex++;
    }

    qDebug() << "Selected item index:" << index << "series:" << seriesIndex;
}

void ScatterDataModifier::setGradient()
{
    QLinearGradient baseGradient(0, 0, 1, 100);
    baseGradient.setColorAt(1.0, Qt::lightGray);
    baseGradient.setColorAt(0.75001, Qt::lightGray);
    baseGradient.setColorAt(0.75, Qt::blue);
    baseGradient.setColorAt(0.50001, Qt::blue);
    baseGradient.setColorAt(0.50, Qt::red);
    baseGradient.setColorAt(0.25001, Qt::red);
    baseGradient.setColorAt(0.25, Qt::yellow);
    baseGradient.setColorAt(0.0, Qt::yellow);

    QLinearGradient singleHighlightGradient(0, 0, 1, 100);
    singleHighlightGradient.setColorAt(1.0, Qt::lightGray);
    singleHighlightGradient.setColorAt(0.75, Qt::blue);
    singleHighlightGradient.setColorAt(0.50, Qt::red);
    singleHighlightGradient.setColorAt(0.25, Qt::yellow);
    singleHighlightGradient.setColorAt(0.0, Qt::white);

    if (m_targetSeries) {
        m_targetSeries->setBaseColor(Qt::green);
        m_targetSeries->setSingleHighlightColor(Qt::white);

        m_targetSeries->setBaseGradient(baseGradient);
        m_targetSeries->setSingleHighlightGradient(singleHighlightGradient);

        Q3DTheme::ColorStyle oldStyle = m_targetSeries->colorStyle();
        if (oldStyle == Q3DTheme::ColorStyleUniform)
            m_targetSeries->setColorStyle(Q3DTheme::ColorStyleObjectGradient);
        else if (oldStyle == Q3DTheme::ColorStyleObjectGradient)
            m_targetSeries->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
        if (oldStyle == Q3DTheme::ColorStyleRangeGradient)
            m_targetSeries->setColorStyle(Q3DTheme::ColorStyleUniform);
    }
}

void ScatterDataModifier::clearSeriesData()
{
    if (m_targetSeries)
        m_targetSeries->dataProxy()->resetArray(0);
}

void ScatterDataModifier::addSeries()
{
    QScatter3DSeries *series = createAndAddSeries();

    QScatter3DSeries *oldTargetSeries = m_targetSeries;
    m_targetSeries = series; // adding always adds to target series, so fake it for a bit
    addOne(); // add one random item to start the new series off
    m_targetSeries = oldTargetSeries;
}

void ScatterDataModifier::removeSeries()
{
    if (m_targetSeries) {
        m_chart->removeSeries(m_targetSeries);
        delete m_targetSeries;
        if (m_chart->seriesList().size())
            m_targetSeries = m_chart->seriesList().at(0);
        else
            m_targetSeries = 0;
    }
}

void ScatterDataModifier::toggleSeriesVisibility()
{
    if (m_targetSeries)
        m_targetSeries->setVisible(!m_targetSeries->isVisible());
}

void ScatterDataModifier::changeSeriesName()
{
    if (m_targetSeries)
        m_targetSeries->setName(m_targetSeries->name().append("-").append(QString::number(qrand() % 10)));
}

void ScatterDataModifier::handleAxisXChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_chart->axisX());
}

void ScatterDataModifier::handleAxisYChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_chart->axisY());
}

void ScatterDataModifier::handleAxisZChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_chart->axisZ());
}

void ScatterDataModifier::handleFpsChange(qreal fps)
{
    static const QString fpsPrefix(QStringLiteral("FPS: "));
    m_fpsLabel->setText(fpsPrefix + QString::number(qRound(fps)));
}

void ScatterDataModifier::changeLabelRotation(int rotation)
{
    m_chart->axisX()->setLabelAutoRotation(float(rotation));
    m_chart->axisY()->setLabelAutoRotation(float(rotation));
    m_chart->axisZ()->setLabelAutoRotation(float(rotation));
}

void ScatterDataModifier::changeRadialLabelOffset(int offset)
{
    m_chart->setRadialLabelOffset(float(offset) / 100.0f);
}

void ScatterDataModifier::toggleAxisTitleVisibility(bool enabled)
{
    m_chart->axisX()->setTitleVisible(enabled);
    m_chart->axisY()->setTitleVisible(enabled);
    m_chart->axisZ()->setTitleVisible(enabled);
}

void ScatterDataModifier::toggleAxisTitleFixed(bool enabled)
{
    m_chart->axisX()->setTitleFixed(enabled);
    m_chart->axisY()->setTitleFixed(enabled);
    m_chart->axisZ()->setTitleFixed(enabled);
}

void ScatterDataModifier::renderToImage()
{
    QImage renderedImage8AA = m_chart->renderToImage(8);
    QImage renderedImageNoAA = m_chart->renderToImage(0);
    QImage renderedImage8AASmall = m_chart->renderToImage(8, QSize(100, 100));
    QImage renderedImageNoAASmall = m_chart->renderToImage(0, QSize(100, 100));

    if (m_chart->isVisible()) {
        renderedImage8AA.save(QStringLiteral("./renderedImage8AA_visible.png"));
        renderedImageNoAA.save(QStringLiteral("./renderedImageNoAA_visible.png"));
        renderedImage8AASmall.save(QStringLiteral("./renderedImage8AASmall_visible.png"));
        renderedImageNoAASmall.save(QStringLiteral("./renderedImageNoAASmall_visible.png"));
        qDebug() << "Visible images rendered!";
    } else {
        renderedImage8AA.save(QStringLiteral("./renderedImage8AA_hidden.png"));
        renderedImageNoAA.save(QStringLiteral("./renderedImageNoAA_hidden.png"));
        renderedImage8AASmall.save(QStringLiteral("./renderedImage8AASmall_hidden.png"));
        renderedImageNoAASmall.save(QStringLiteral("./renderedImageNoAASmall_hidden.png"));
        qDebug() << "Hidden images rendered!";
    }
}

void ScatterDataModifier::togglePolar(bool enable)
{
    m_chart->setPolar(enable);
}

void ScatterDataModifier::toggleStatic(bool enable)
{
    if (enable)
        m_chart->setOptimizationHints(QAbstract3DGraph::OptimizationStatic);
    else
        m_chart->setOptimizationHints(QAbstract3DGraph::OptimizationDefault);
}

void ScatterDataModifier::toggleOrtho(bool enable)
{
    m_chart->setOrthoProjection(enable);
}

void ScatterDataModifier::setCameraTargetX(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setX(float(value) / 100.0f);
    m_chart->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void ScatterDataModifier::setCameraTargetY(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setY(float(value) / 100.0f);
    m_chart->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void ScatterDataModifier::setCameraTargetZ(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setZ(float(value) / 100.0f);
    m_chart->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void ScatterDataModifier::setGraphMargin(int value)
{
    m_chart->setMargin(qreal(value) / 100.0);
    qDebug() << "Setting margin:" << m_chart->margin() << value;
}

void ScatterDataModifier::changeShadowQuality(int quality)
{
    QAbstract3DGraph::ShadowQuality sq = QAbstract3DGraph::ShadowQuality(quality);
    m_chart->setShadowQuality(sq);
    emit shadowQualityChanged(quality);
}

void ScatterDataModifier::setBackgroundEnabled(int enabled)
{
    m_chart->activeTheme()->setBackgroundEnabled((bool)enabled);
}

void ScatterDataModifier::setGridEnabled(int enabled)
{
    m_chart->activeTheme()->setGridEnabled((bool)enabled);
}

void ScatterDataModifier::setMinX(int min)
{
    m_chart->axisX()->setMin(min);
}

void ScatterDataModifier::setMinY(int min)
{
    m_chart->axisY()->setMin(float(min) / 100.0f);
}

void ScatterDataModifier::setMinZ(int min)
{
    m_chart->axisZ()->setMin(min);
}

void ScatterDataModifier::setMaxX(int max)
{
    m_chart->axisX()->setMax(max);
}

void ScatterDataModifier::setMaxY(int max)
{
    m_chart->axisY()->setMax(float(max) / 100.0f);
}

void ScatterDataModifier::setMaxZ(int max)
{
    m_chart->axisZ()->setMax(max);
}

void ScatterDataModifier::setAspectRatio(int ratio)
{
    qreal aspectRatio = qreal(ratio) / 10.0;
    m_chart->setAspectRatio(aspectRatio);
}

void ScatterDataModifier::setHorizontalAspectRatio(int ratio)
{
    qreal aspectRatio = qreal(ratio) / 100.0;
    m_chart->setHorizontalAspectRatio(aspectRatio);
}

QVector3D ScatterDataModifier::randVector()
{
    QVector3D retvec = QVector3D(
        (float)(rand() % 100) / 2.0f - (float)(rand() % 100) / 2.0f,
        (float)(rand() % 100) / 100.0f - (float)(rand() % 100) / 100.0f,
        (float)(rand() % 100) / 2.0f - (float)(rand() % 100) / 2.0f);

    qDebug() << __FUNCTION__ << retvec;

    return retvec;
}

QScatter3DSeries *ScatterDataModifier::createAndAddSeries()
{
    static int counter = 0;

    QScatter3DSeries *series = new QScatter3DSeries;

    if (!m_targetSeries)
        m_targetSeries = series;

    m_chart->addSeries(series);
    series->setName(QString("Series %1").arg(counter++));
    series->setItemLabelFormat(QStringLiteral("@seriesName: (X:@xLabel / Z:@zLabel) Y:@yLabel"));
    series->setMesh(QAbstract3DSeries::MeshSphere);
    series->setMeshSmooth(true);
    series->setBaseColor(QColor(rand() % 256, rand() % 256, rand() % 256));
    series->setItemSize(float(rand() % 90 + 10) / 100.0f);

    QObject::connect(series, &QScatter3DSeries::selectedItemChanged, this,
                     &ScatterDataModifier::handleSelectionChange);

    return series;
}

void ScatterDataModifier::populateFlatSeries(QScatter3DSeries *series, int rows, int columns,
                                             float value)
{
    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize(rows * columns);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++)
            (*dataArray)[i * columns + j].setPosition(QVector3D(float(i), value, float(j)));
    }
    series->dataProxy()->resetArray(dataArray);
}

void ScatterDataModifier::populateRisingSeries(QScatter3DSeries *series, int rows, int columns,
                                               float minValue, float maxValue)
{
    QScatterDataArray *dataArray = new QScatterDataArray;
    int arraySize = rows * columns;
    dataArray->resize(arraySize);
    float range = maxValue - minValue;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            float yValue = minValue + (range * i * j / arraySize);
            (*dataArray)[i * columns + j].setPosition(QVector3D(float(i), yValue, float(j)));
        }
    }
    series->dataProxy()->resetArray(dataArray);
}
