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

/*
 * Galaxy creation code obtained from http://beltoforion.de/galaxy/galaxy_en.html
 * Thanks to Ingo Berg, great work.
 * Licensed under a  Creative Commons Attribution 3.0 License
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "galaxydata.h"
#include "cumulativedistributor.h"
#include "star.h"
#include <QtDataVisualization/qscatterdataproxy.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/qscatter3dseries.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtCore/qmath.h>
#include <QPainter>

#include <QDebug>

using namespace QtDataVisualization;

static const int numOfStars = 70000;
static const int numOfDust = numOfStars / 2;
static const int numOfH2 = 200;
static const qreal rand_max = qreal(RAND_MAX);

GalaxyData::GalaxyData(Q3DScatter *scatter,
                       qreal rad,
                       qreal radCore,
                       qreal deltaAng,
                       qreal ex1,
                       qreal ex2)
    : m_graph(scatter),
      m_pStars(0),
      m_pDust(0),
      m_pH2(0),
      m_radGalaxy(rad),
      m_radCore(radCore),
      m_angleOffset(deltaAng),
      m_elEx1(ex1),
      m_elEx2(ex2),
      m_radFarField(m_radGalaxy * 2),
      m_filtered(false),
      m_minx(9999.9),
      m_maxx(-9999.0),
      m_miny(9999.9),
      m_maxy(-9999.0)
{
    m_graph->activeTheme()->setType(Q3DTheme::ThemeEbony);
    m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualitySoftLow);

    m_graph->axisX()->setRange(-25000.0f, 25000.0f);
    m_graph->axisZ()->setRange(-25000.0f, 25000.0f);
    m_graph->setOptimizationHints(QAbstract3DGraph::OptimizationStatic);
    m_graph->activeTheme()->setBackgroundColor(QColor(31, 31, 31));
    m_graph->activeTheme()->setWindowColor(QColor(31, 31, 31));
    m_graph->activeTheme()->setLabelBackgroundColor(QColor(31, 31, 31));
    m_graph->activeTheme()->setLabelTextColor(QColor(31, 31, 31));

    QObject::connect(m_graph, &QAbstract3DGraph::currentFpsChanged,
                     this, &GalaxyData::handleFpsChange);
    m_graph->setMeasureFps(true);

    createSeries();
    createGalaxy();
}

GalaxyData::~GalaxyData()
{
    delete m_graph;
    if (m_pStars)
        delete [] m_pStars;
    if (m_pDust)
        delete [] m_pDust;
    if (m_pH2)
        delete [] m_pH2;
}

void GalaxyData::createGalaxy()
{
    if (m_pStars)
        delete [] m_pStars;
    m_pStars = new Star[numOfStars];

    if (m_pDust)
        delete [] m_pDust;
    m_pDust = new Star[numOfDust];

    if (m_pH2)
        delete [] m_pH2;
    m_pH2 = new Star[numOfH2 * 2];


    m_minx = 9999.9;
    m_maxx = -9999.0;
    m_miny = 9999.9;
    m_maxy = -9999.0;

    // First star is the black hole at the center
    m_pStars[0].m_a = 0;
    m_pStars[0].m_b = 0;
    m_pStars[0].m_angle = 0;
    m_pStars[0].m_theta = 0;
    m_pStars[0].m_center = QVector2D(0.0f, 0.0f);
    m_pStars[0].calcXY();

    // second star is at the edge of the core area
    m_pStars[1].m_a = m_radCore;
    m_pStars[1].m_b = m_radCore * getExcentricity(m_radCore);
    m_pStars[1].m_angle = getAngularOffset(m_radCore);
    m_pStars[1].m_theta = 0;
    m_pStars[1].m_center = QVector2D(0.0f, 0.0f);
    m_pStars[1].calcXY();
    checkMinMax(m_pStars[1]);

    // third star is at the edge of the disk
    m_pStars[2].m_a = m_radGalaxy;
    m_pStars[2].m_b = m_radGalaxy * getExcentricity(m_radGalaxy);
    m_pStars[2].m_angle = getAngularOffset(m_radGalaxy);
    m_pStars[2].m_theta = 0;
    m_pStars[2].m_center = QVector2D(0.0f, 0.0f);
    m_pStars[2].calcXY();
    checkMinMax(m_pStars[2]);

    CumulativeDistributor cd;
    cd.setupRealistic(1.0,             // Maximalintensität
                      0.02,            // k (bulge)
                      m_radGalaxy/3.0, // disc skalenlänge
                      m_radCore,       // bulge radius
                      0,               // start der intensitätskurve
                      m_radFarField,   // ende der intensitätskurve
                      1000.0);           // Anzahl der stützstellen

    for (int i = 3; i < numOfStars; ++i) {
        qreal rad = cd.valFromProp(qreal(qrand()) / rand_max);

        m_pStars[i].m_a = rad;
        m_pStars[i].m_b = rad * getExcentricity(rad);
        m_pStars[i].m_angle = getAngularOffset(rad);
        m_pStars[i].m_theta = 360.0 * ((double)rand() / RAND_MAX);
        m_pStars[i].m_center = QVector2D(0.0f, 0.0f);
        m_pStars[i].calcXY();

        checkMinMax(m_pStars[i]);
    }

    // Initialize Dust
    qreal x, y, rad;
    for (int i = 0; i < numOfDust; ++i)
    {
        x = 2.0 * m_radGalaxy * ((double)rand() / RAND_MAX) - m_radGalaxy;
        y = 2.0 * m_radGalaxy * ((double)rand() / RAND_MAX) - m_radGalaxy;
        rad = sqrt(x*x + y*y);

        m_pDust[i].m_a = rad;
        m_pDust[i].m_b = rad * getExcentricity(rad);
        m_pDust[i].m_angle = getAngularOffset(rad);
        m_pDust[i].m_theta = 360.0 * ((double)rand() / RAND_MAX);
        m_pDust[i].m_center = QVector2D(0.0f, 0.0f);
        m_pDust[i].calcXY();

        checkMinMax(m_pDust[i]);
    }

    // Initialize H2
    for (int i = 0; i < numOfH2; ++i)
    {
        x = 2*(m_radGalaxy) * ((double)rand() / RAND_MAX) - (m_radGalaxy);
        y = 2*(m_radGalaxy) * ((double)rand() / RAND_MAX) - (m_radGalaxy);
        rad = sqrt(x*x + y*y);

        int k1 = 2*i;
        m_pH2[k1].m_a = rad;
        m_pH2[k1].m_b = rad * getExcentricity(rad);
        m_pH2[k1].m_angle = getAngularOffset(rad);
        m_pH2[k1].m_theta = 360.0 * ((double)rand() / RAND_MAX);
        m_pH2[k1].m_center = QVector2D(0.0f, 0.0f);
        m_pH2[k1].calcXY();

        int k2 = 2*i + 1;
        m_pH2[k2].m_a = rad + 1000.0;
        m_pH2[k2].m_b = rad * getExcentricity(rad);
        m_pH2[k2].m_angle = m_pH2[k1].m_angle;
        m_pH2[k2].m_theta = m_pH2[k1].m_theta;
        m_pH2[k2].m_center = m_pH2[k1].m_center;
        m_pH2[k2].calcXY();
    }

    qreal max = qMax(m_maxx, m_maxy);
    qreal min = -qMin(m_minx, m_miny);
    max = qMax(min, max);
    m_range = int((max + 500.0) / 1000.0) * 1000;
    m_graph->axisX()->setRange(-float(m_range), float(m_range));
    m_graph->axisZ()->setRange(-float(m_range), float(m_range));

    if (!m_filtered)
        createNormalDataView();
    else
        createFilteredView();
}

void GalaxyData::createSeries()
{
    QScatterDataProxy *proxyNormal = new QScatterDataProxy;
    m_normalSeries = new QScatter3DSeries(proxyNormal);
    m_normalSeries->setMesh(QAbstract3DSeries::MeshPoint);
    m_graph->addSeries(m_normalSeries);

    QScatterDataProxy *proxyDust = new QScatterDataProxy;
    m_dustSeries = new QScatter3DSeries(proxyDust);
    m_dustSeries->setMesh(QAbstract3DSeries::MeshPoint);
    m_graph->addSeries(m_dustSeries);

    QScatterDataProxy *proxyH2 = new QScatterDataProxy;
    m_H2Series = new QScatter3DSeries(proxyH2);
    m_H2Series->setMesh(QAbstract3DSeries::MeshPoint);
    m_graph->addSeries(m_H2Series);

    QScatterDataProxy *proxyFiltered = new QScatterDataProxy;
    m_filteredSeries = new QScatter3DSeries(proxyFiltered);
    m_filteredSeries->setMesh(QAbstract3DSeries::MeshCube);
    m_graph->addSeries(m_filteredSeries);

}

void GalaxyData::createNormalDataView()
{
    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize(numOfStars);
    QScatterDataItem *ptrToDataArray = &dataArray->first();

    for (uint i = 0; i < numOfStars; i++) {
        ptrToDataArray->setPosition(QVector3D(m_pStars[i].m_pos.x(),
                                              0.0f,
                                              m_pStars[i].m_pos.y()));
        ptrToDataArray++;
    }

    m_normalSeries->dataProxy()->resetArray(dataArray);
    m_normalSeries->setMesh(QAbstract3DSeries::MeshPoint);
    m_normalSeries->setBaseColor(Qt::white);

    dataArray = new QScatterDataArray;
    dataArray->resize(numOfDust);
    ptrToDataArray = &dataArray->first();

    for (uint i = 0; i < numOfDust; i++) {
        ptrToDataArray->setPosition(QVector3D(m_pDust[i].m_pos.x(),
                                              0.0f,
                                              m_pDust[i].m_pos.y()));
        ptrToDataArray++;
    }

    m_dustSeries->dataProxy()->resetArray(dataArray);
    m_dustSeries->setMesh(QAbstract3DSeries::MeshPoint);
    m_dustSeries->setBaseColor(QColor(131, 111, 255));

    dataArray = new QScatterDataArray;
    dataArray->resize(numOfDust);
    ptrToDataArray = &dataArray->first();

    uint H2Count = numOfH2 * 2;
    for (uint i = 0; i < H2Count; i++) {
        ptrToDataArray->setPosition(QVector3D(m_pH2[i].m_pos.x(),
                                              0.0f,
                                              m_pH2[i].m_pos.y()));
        ptrToDataArray++;
    }

    m_H2Series->dataProxy()->resetArray(dataArray);
    m_H2Series->setMesh(QAbstract3DSeries::MeshPoint);
    m_H2Series->setBaseColor(Qt::red);
}

void GalaxyData::createFilteredView()
{
    int steps = (m_range / 1000) * 2;
    int tableSize = steps * steps;
    int *table = new int[tableSize];
    for (int i = 0; i < tableSize; i++)
        table[i] = 0;
    qreal add = qreal(m_range);
    int max = 0;

    for (uint i = 0; i < numOfStars; i++) {
        int x = int(m_pStars[i].m_pos.x() + add) / 1000;
        int y = int(m_pStars[i].m_pos.y() + add) / 1000;
        table[y * steps + x] = table[y * steps + x] + 1;

        if (max < table[y * steps + x])
            max = table[y * steps + x];
    }

    // Count how many cells have data
    int nActiveCell = 0;
    for (int i = 0; i < tableSize; i++) {
        if (table[i])
            nActiveCell++;
    }


    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize(nActiveCell);
    QScatterDataItem *ptrToDataArray = &dataArray->first();

    for (int y = 0; y < steps; y++) {
        for (int x = 0; x < steps; x++) {
            if (table[y * steps + x]) {
                ptrToDataArray->setPosition(QVector3D(float(x) * 1000.0f - add + 500.0f,
                                                      float(table[y * steps + x]),
                                                      float(y) * 1000.0f - add + 500.0f));
                ptrToDataArray++;
            }
        }
    }

    m_filteredSeries->dataProxy()->resetArray(dataArray);
    m_filteredSeries->setMesh(QAbstract3DSeries::MeshCube);
    m_filteredSeries->setItemSize(0.1f);

    m_graph->axisY()->setRange(0.0f, float(max + 1));

    qDebug() << "max = " << max;
}

void GalaxyData::checkMinMax(const Star &star)
{
    if (star.m_pos.x() < m_minx)
        m_minx = star.m_pos.x();
    if (star.m_pos.x() > m_maxx)
        m_maxx = star.m_pos.x();
    if (star.m_pos.y() < m_miny)
        m_miny = star.m_pos.y();
    if (star.m_pos.y() > m_maxy)
        m_maxy = star.m_pos.y();
}

qreal GalaxyData::getExcentricity(qreal r) const
{
    if (r < m_radCore)
    {
        // Core region of the galaxy. Innermost part is round
        // excentricity increasing linear to the border of the core.
        return 1 + (r / m_radCore) * (m_elEx1-1);
    } else if (r > m_radCore && r <= m_radGalaxy) {
        return m_elEx1 + (r - m_radCore) / (m_radGalaxy - m_radCore) * (m_elEx2 - m_elEx1);
    } else if (r > m_radGalaxy && r < m_radFarField) {
        // excentricity is slowly reduced to 1.
        return m_elEx2 + (r - m_radGalaxy) / (m_radFarField - m_radGalaxy) * (1 - m_elEx2);
    } else {
        return 1.0;
    }
}

qreal GalaxyData::getAngularOffset(qreal rad) const
{
  return rad * m_angleOffset;
}

void GalaxyData::radiusGalaxyChanged(int value)
{
    m_radGalaxy = qreal(value);
    createGalaxy();
}

void GalaxyData::radiusCoreChanged(int value)
{
    m_radCore = qreal(value);
    createGalaxy();
}

void GalaxyData::angleOffsetChanged(int value)
{
    m_angleOffset = qreal(value) / 100000.0;
    createGalaxy();
}

void GalaxyData::eccentricityInnerChanged(int value)
{
    m_elEx1 = qreal(value) / 100.0;
    createGalaxy();
}

void GalaxyData::eccentricityOuterChanged(int value)
{
    m_elEx2 = qreal(value) / 100.0;
    createGalaxy();
}

void GalaxyData::setFilteredEnabled(bool enabled)
{
    m_filtered = enabled;
    if (enabled) {
        QLinearGradient gr(0, 0, 1, 100);
        gr.setColorAt(0.0, Qt::white);
        gr.setColorAt(0.04, Qt::green);
        gr.setColorAt(0.1, Qt::darkGreen);
        gr.setColorAt(1.0, Qt::red);

        m_filteredSeries->setBaseGradient(gr);
        m_filteredSeries->setColorStyle(Q3DTheme::ColorStyleRangeGradient);

        m_normalSeries->setVisible(false);

        createFilteredView();

        m_filteredSeries->setVisible(true);
    } else {
        m_normalSeries->setColorStyle(Q3DTheme::ColorStyleUniform);
        m_graph->axisY()->setRange(-1.0f, 1.0f);
        m_normalSeries->setItemSize(0.0f);

        m_filteredSeries->setVisible(false);

        createNormalDataView();

        m_normalSeries->setVisible(true);
    }
}


void GalaxyData::setStaticEnabled(bool enabled)
{
    if (enabled)
        m_graph->setOptimizationHints(QAbstract3DGraph::OptimizationStatic);
    else
        m_graph->setOptimizationHints(QAbstract3DGraph::OptimizationDefault);
}

void GalaxyData::setStarsVisible(bool enabled)
{
    m_normalSeries->setVisible(enabled);
}

void GalaxyData::setDustVisible(bool enabled)
{
    m_dustSeries->setVisible(enabled);
}

void GalaxyData::setH2Visible(bool enabled)
{
    m_H2Series->setVisible(enabled);
}

void GalaxyData::resetValues()
{
    m_radiusGalaxySlider->setValue(15000);
    m_radiusCoreSlider->setValue(4000);
    m_angleOffsetSlider->setValue(40);
    m_eccentricityInnerSlider->setValue(90);
    m_eccentricityOuterSlider->setValue(90);
}

void GalaxyData::handleFpsChange(qreal fps)
{
    static const QString fpsPrefix(QStringLiteral("FPS: "));
    m_fpsLabel->setText(fpsPrefix + QString::number(qRound(fps)));
}
