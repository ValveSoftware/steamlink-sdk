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

#include "graphmodifier.h"
#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/Q3DInputHandler>

#include <qmath.h>
#include <QLinearGradient>
#include <QDebug>
#include <QComboBox>
#ifndef QT_NO_CURSOR
#include <QtGui/QCursor>
#endif
using namespace QtDataVisualization;

//#define JITTER_PLANE
//#define WONKY_PLANE

GraphModifier::GraphModifier(Q3DSurface *graph, QWidget *parentWidget)
    : m_graph(graph),
      m_series1(new QSurface3DSeries),
      m_series2(new QSurface3DSeries),
      m_series3(new QSurface3DSeries),
      m_series4(new QSurface3DSeries),
      m_gridSliderX(0),
      m_gridSliderZ(0),
      m_axisRangeSliderX(0),
      m_axisRangeSliderZ(0),
      m_axisMinSliderX(0),
      m_axisMinSliderZ(0),
      m_xCount(24),
      m_zCount(24),
      m_activeSample(0),
      m_fontSize(40),
      m_rangeX(34.0),
      m_rangeY(16.0),
      m_rangeZ(34.0),
      m_minX(-17.0),
      m_minY(-8.0),
      m_minZ(-17.0),
      m_addRowCounter(m_zCount),
      m_insertTestZPos(0),
      m_insertTestIndexPos(1),
      m_planeArray(0),
      m_theSeries(new QSurface3DSeries),
      m_drawMode(QSurface3DSeries::DrawSurfaceAndWireframe),
      m_drawMode2(QSurface3DSeries::DrawSurfaceAndWireframe),
      m_drawMode3(QSurface3DSeries::DrawSurfaceAndWireframe),
      m_drawMode4(QSurface3DSeries::DrawSurfaceAndWireframe),
      m_offset(4.0f),
      m_parentWidget(parentWidget),
      m_ascendingX(true),
      m_ascendingZ(true)
{
    m_graph->setAxisX(new QValue3DAxis);
    m_graph->axisX()->setTitle("X-Axis");
    m_graph->setAxisY(new QValue3DAxis);
    m_graph->axisY()->setTitle("Value Axis");
    m_graph->setAxisZ(new QValue3DAxis);
    m_graph->axisZ()->setTitle("Z-Axis");
#ifdef MULTI_SERIES
    m_limitX = float(m_xCount) / 2.0f;
    m_limitZ = float(m_zCount) / 2.0f;
    // Series 1
    m_multiSampleOffsetX[0] = -m_offset;
    m_multiSampleOffsetZ[0] = -m_offset;
    // Series 2
    m_multiSampleOffsetX[1] = -m_offset;
    m_multiSampleOffsetZ[1] = m_offset;
    // Series 3
    m_multiSampleOffsetX[2] = m_offset;
    m_multiSampleOffsetZ[2] = -m_offset;
    // Series 4
    m_multiSampleOffsetX[3] = m_offset;
    m_multiSampleOffsetZ[3] = m_offset;

//    m_graph->axisX()->setRange(-m_limitX - m_offset, m_limitX + m_offset);
//    m_graph->axisY()->setRange(-1.0f, 4.5f);
//    m_graph->axisZ()->setRange(-m_limitZ - m_offset, m_limitZ + m_offset);
#else
    m_graph->addSeries(m_theSeries);
#endif
    m_graph->axisX()->setRange(m_minX, m_minX + m_rangeX);
    m_graph->axisY()->setRange(m_minY, m_minY + m_rangeY);
    m_graph->axisZ()->setRange(m_minZ, m_minZ + m_rangeZ);

    static_cast<Q3DInputHandler *>(m_graph->activeInputHandler())->setZoomAtTargetEnabled(true);

    for (int i = 0; i < 4; i++) {
        m_multiseries[i] = new QSurface3DSeries;
        m_multiseries[i]->setName(QStringLiteral("Series %1").arg(i+1));
        m_multiseries[i]->setItemLabelFormat(QStringLiteral("@seriesName: (@xLabel, @zLabel): @yLabel"));
    }

    fillSeries();
    changeStyle();

    m_theSeries->setItemLabelFormat(QStringLiteral("@seriesName: (@xLabel, @zLabel): @yLabel"));

    connect(&m_timer, &QTimer::timeout, this, &GraphModifier::timeout);
    connect(&m_graphPositionQueryTimer, &QTimer::timeout, this, &GraphModifier::graphQueryTimeout);
    connect(m_theSeries, &QSurface3DSeries::selectedPointChanged, this, &GraphModifier::selectedPointChanged);

    QObject::connect(m_graph, &Q3DSurface::axisXChanged, this,
                     &GraphModifier::handleAxisXChanged);
    QObject::connect(m_graph, &Q3DSurface::axisYChanged, this,
                     &GraphModifier::handleAxisYChanged);
    QObject::connect(m_graph, &Q3DSurface::axisZChanged, this,
                     &GraphModifier::handleAxisZChanged);
    QObject::connect(m_graph, &QAbstract3DGraph::currentFpsChanged, this,
                     &GraphModifier::handleFpsChange);

    //m_graphPositionQueryTimer.start(100);
}

GraphModifier::~GraphModifier()
{
    delete m_graph;
}

void GraphModifier::fillSeries()
{
    float full = m_limitX * m_limitZ;

    QSurfaceDataArray *dataArray1 = new QSurfaceDataArray;
    dataArray1->reserve(m_zCount);
    QSurfaceDataArray *dataArray2 = new QSurfaceDataArray;
    dataArray2->reserve(m_zCount);
    QSurfaceDataArray *dataArray3 = new QSurfaceDataArray;
    dataArray3->reserve(m_zCount);
    QSurfaceDataArray *dataArray4 = new QSurfaceDataArray;
    dataArray4->reserve(m_zCount);


    for (int i = 0; i < m_zCount; i++) {
        QSurfaceDataRow *newRow[4];
        float zAdjust = 0.0f;
        if (i == 2)
            zAdjust = 0.7f;

        for (int s = 0; s < 4; s++) {
            newRow[s] = new QSurfaceDataRow(m_xCount);
            float z = float(i) - m_limitZ + 0.5f + m_multiSampleOffsetZ[s] + zAdjust;
            for (int j = 0; j < m_xCount; j++) {
                float xAdjust = 0.0f;
                if (j == 4)
                    xAdjust = 0.7f;
                float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[s] + xAdjust;
                float angle = (z * x) / full * 1.57f;
                float y = (qSin(angle * float(qPow(1.3f, s))) + 1.1f * s) * 3.0f - 5.0f + xAdjust + zAdjust;
                (*newRow[s])[j].setPosition(QVector3D(x, y, z));
            }
        }
        *dataArray1 << newRow[0];
        *dataArray2 << newRow[1];
        *dataArray3 << newRow[2];
        *dataArray4 << newRow[3];
    }

    m_multiseries[0]->dataProxy()->resetArray(dataArray1);
    m_multiseries[1]->dataProxy()->resetArray(dataArray2);
    m_multiseries[2]->dataProxy()->resetArray(dataArray3);
    m_multiseries[3]->dataProxy()->resetArray(dataArray4);
}

void GraphModifier::toggleSeries1(bool enabled)
{
    qDebug() << __FUNCTION__ << " enabled = " << enabled;

    if (enabled) {
        m_graph->addSeries(m_multiseries[0]);
    } else {
        m_graph->removeSeries(m_multiseries[0]);
    }
}

void GraphModifier::toggleSeries2(bool enabled)
{
    qDebug() << __FUNCTION__ << " enabled = " << enabled;

    if (enabled) {
        m_graph->addSeries(m_multiseries[1]);
    } else {
        m_graph->removeSeries(m_multiseries[1]);
    }
}

void GraphModifier::toggleSeries3(bool enabled)
{
    qDebug() << __FUNCTION__ << " enabled = " << enabled;

    if (enabled) {
        m_graph->addSeries(m_multiseries[2]);
    } else {
        m_graph->removeSeries(m_multiseries[2]);
    }
}

void GraphModifier::toggleSeries4(bool enabled)
{
    qDebug() << __FUNCTION__ << " enabled = " << enabled;

    if (enabled) {
        m_graph->addSeries(m_multiseries[3]);
    } else {
        m_graph->removeSeries(m_multiseries[3]);
    }
}

void GraphModifier::toggleSmooth(bool enabled)
{
    qDebug() << "GraphModifier::toggleSmooth " << enabled;
    m_theSeries->setFlatShadingEnabled(enabled);
#ifdef MULTI_SERIES
    m_multiseries[0]->setFlatShadingEnabled(enabled);
#endif
}

void GraphModifier::toggleSurfaceGrid(bool enable)
{
    qDebug() << "GraphModifier::toggleSurfaceGrid" << enable;
    if (enable)
        m_drawMode |= QSurface3DSeries::DrawWireframe;
    else
        m_drawMode &= ~QSurface3DSeries::DrawWireframe;

    m_theSeries->setDrawMode(m_drawMode);
#ifdef MULTI_SERIES
    m_multiseries[0]->setDrawMode(m_drawMode);
#endif
}

void GraphModifier::toggleSurface(bool enable)
{
    qDebug() << "GraphModifier::toggleSurface" << enable;
    if (enable)
        m_drawMode |= QSurface3DSeries::DrawSurface;
    else
        m_drawMode &= ~QSurface3DSeries::DrawSurface;

    m_theSeries->setDrawMode(m_drawMode);
#ifdef MULTI_SERIES
    m_multiseries[0]->setDrawMode(m_drawMode);
#endif
}

void GraphModifier::toggleSeriesVisible(bool enable)
{
    m_theSeries->setVisible(enable);
#ifdef MULTI_SERIES
    m_multiseries[0]->setVisible(enable);
#endif
}

void GraphModifier::toggleSmoothS2(bool enabled)
{
    qDebug() << __FUNCTION__ << enabled;
    m_multiseries[1]->setFlatShadingEnabled(enabled);
}

void GraphModifier::toggleSurfaceGridS2(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode2 |= QSurface3DSeries::DrawWireframe;
    else
        m_drawMode2 &= ~QSurface3DSeries::DrawWireframe;

    m_multiseries[1]->setDrawMode(m_drawMode2);
}

void GraphModifier::toggleSurfaceS2(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode2 |= QSurface3DSeries::DrawSurface;
    else
        m_drawMode2 &= ~QSurface3DSeries::DrawSurface;

    m_multiseries[1]->setDrawMode(m_drawMode2);
}

void GraphModifier::toggleSeries2Visible(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    m_multiseries[1]->setVisible(enable);
}

void GraphModifier::toggleSmoothS3(bool enabled)
{
    qDebug() << __FUNCTION__ << enabled;
    m_multiseries[2]->setFlatShadingEnabled(enabled);
}

void GraphModifier::toggleSurfaceGridS3(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode3 |= QSurface3DSeries::DrawWireframe;
    else
        m_drawMode3 &= ~QSurface3DSeries::DrawWireframe;

    m_multiseries[2]->setDrawMode(m_drawMode3);
}

void GraphModifier::toggleSurfaceS3(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode3 |= QSurface3DSeries::DrawSurface;
    else
        m_drawMode3 &= ~QSurface3DSeries::DrawSurface;

    m_multiseries[2]->setDrawMode(m_drawMode3);
}

void GraphModifier::toggleSeries3Visible(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    m_multiseries[2]->setVisible(enable);
}

void GraphModifier::toggleSmoothS4(bool enabled)
{
    qDebug() << __FUNCTION__ << enabled;
    m_multiseries[3]->setFlatShadingEnabled(enabled);
}

void GraphModifier::toggleSurfaceGridS4(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode4 |= QSurface3DSeries::DrawWireframe;
    else
        m_drawMode4 &= ~QSurface3DSeries::DrawWireframe;

    m_multiseries[3]->setDrawMode(m_drawMode4);
}

void GraphModifier::toggleSurfaceS4(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    if (enable)
        m_drawMode4 |= QSurface3DSeries::DrawSurface;
    else
        m_drawMode4 &= ~QSurface3DSeries::DrawSurface;

    m_multiseries[3]->setDrawMode(m_drawMode4);
}

void GraphModifier::toggleSeries4Visible(bool enable)
{
    qDebug() << __FUNCTION__ << enable;
    m_multiseries[3]->setVisible(enable);
}

void GraphModifier::toggleSqrtSin(bool enable)
{
    if (enable) {
        qDebug() << "Create Sqrt&Sin surface, (" << m_xCount << ", " << m_zCount << ")";

        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);

        QSurfaceDataArray *dataArray = new QSurfaceDataArray;
        dataArray->reserve(m_zCount);
        for (float i = 0; i < m_zCount; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            // Keep values within range bounds, since just adding step can cause minor drift due
            // to the rounding errors.
            float z = qMin(maxZ, (i * stepZ + minZ));
            for (float j = 0; j < m_xCount; j++) {
                float x = qMin(maxX, (j * stepX + minX));
                float R = qSqrt(x * x + z * z) + 0.01f;
                float y = (qSin(R) / R + 0.24f) * 1.61f + 1.0f;
                (*newRow)[j].setPosition(QVector3D(x, y, z));
            }
            *dataArray << newRow;
        }

        m_graph->axisY()->setRange(1.0, 3.0);
        m_graph->axisX()->setLabelFormat("%.2f");
        m_graph->axisZ()->setLabelFormat("%.2f");

        m_theSeries->setName("Sqrt & Sin");

        resetArrayAndSliders(dataArray, minZ, maxZ, minX, maxX);

        m_activeSample = GraphModifier::SqrtSin;
    } else {
        qDebug() << "Remove surface";
    }
}

void GraphModifier::togglePlane(bool enable)
{
    qDebug() << "GraphModifier::togglePlane " << enable;

    if (enable) {
        m_planeArray = new QSurfaceDataArray;

#ifdef JITTER_PLANE
        m_timer.start(0);
#endif
        m_graph->axisY()->setRange(0.0, 1.0);
        m_graph->axisX()->setLabelFormat("%.2f");
        m_graph->axisZ()->setLabelFormat("%.2f");

        m_planeArray->reserve(m_zCount);
        float minX = -10.0f;
        float maxX = 20.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);
#ifdef WONKY_PLANE
        float halfZ = m_zCount / 2;
        float wonkyFactor = 0.01f;
        float maxStepX = 0.0f;
        float add = 0.0f;
        for (float i = 0; i < m_zCount; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            if (i < halfZ) {
                stepX += wonkyFactor;
                maxStepX = stepX;
            } else {
                stepX -= wonkyFactor;
            }
            add = 0.0f;
            for (float j = 0; j < m_xCount; j++) {
                (*newRow)[j].setPosition(QVector3D(j * stepX + minX, -0.04f,
                                                   i * stepZ + minZ + add));
                add += 0.5f;

            }
            *m_planeArray << newRow;
        }

        m_theSeries->setName("Wonky Plane");

        resetArrayAndSliders(m_planeArray, minZ, maxZ + add, minX, m_xCount * maxStepX + minX);
#else
        for (float i = 0; i < m_zCount; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            // Keep values within range bounds, since just adding step can cause minor drift due
            // to the rounding errors.
            float zVal;
            if (i == (m_zCount - 1))
                zVal = maxZ;
            else
                zVal = i * stepZ + minZ;

            float j = 0;
            for (; j < m_xCount - 1; j++)
                (*newRow)[j].setPosition(QVector3D(j * stepX + minX, -0.04f, zVal));
            (*newRow)[j].setPosition(QVector3D(maxX, -0.04f, zVal));

            *m_planeArray << newRow;
        }

        m_theSeries->setName("Plane");

        resetArrayAndSliders(m_planeArray, minZ, maxZ, minX, maxX);
#endif

        m_activeSample = GraphModifier::Plane;
    }
#ifdef JITTER_PLANE
    else {
        m_timer.stop();
    }
#endif
}

void GraphModifier::setHeightMapData(bool enable)
{
    if (enable) {
        // Do the height map the hard way.
        // Easier alternative would be to use the QHeightMapSurfaceDataProxy.
        QImage image(":/maps/map");

        QSurfaceDataArray *dataArray = new QSurfaceDataArray;
        uchar *bits = image.bits();

        int p = image.width() * 4 * (image.height() - 1);
        dataArray->reserve(image.height());
        float minX = 34.0;
        float maxX = 40.0;
        float minZ = 18.0;
        float maxZ = 24.0;
        float xMul = (maxX - minX) / float(image.width() - 1);
        float zMul = (maxZ - minZ) / float(image.height() - 1);
        for (int i = 0; i < image.height(); i++, p -= image.width() * 4) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(image.width());
            for (int j = 0; j < image.width(); j++) {
                (*newRow)[j].setPosition(QVector3D((float(j) * xMul) + minX,
                                                   (float(bits[p + (j * 4)]) + 1.0f) / 1.0f,
                                                   (float(i) * zMul) + minZ));
            }
            *dataArray << newRow;
        }

        m_graph->axisY()->setAutoAdjustRange(true);
        m_graph->axisX()->setLabelFormat("%.1f N");
        m_graph->axisZ()->setLabelFormat("%.1f E");

        m_theSeries->setName("Height Map");

        resetArrayAndSliders(dataArray, minZ, maxZ, minX, maxX);

        m_activeSample = GraphModifier::Map;
    }
}

void GraphModifier::toggleGridSliderLock(bool enable)
{
    m_gridSlidersLocked = enable;
    if (m_gridSlidersLocked) {
        m_gridSliderZ->setEnabled(false);
        m_gridSliderZ->setValue(m_gridSliderX->value());
    } else {
        m_gridSliderZ->setEnabled(true);
    }
}

void GraphModifier::adjustXCount(int count)
{
    m_xCount = count;
    if (m_gridSlidersLocked)
        m_gridSliderZ->setValue(count);

    updateSamples();

    qDebug() << "X count =" << count;
}

void GraphModifier::adjustZCount(int count)
{
    m_zCount = count;

    updateSamples();

    qDebug() << "Z count =" << count;
}

void GraphModifier::adjustXRange(int range)
{
    m_rangeX = range;
    m_graph->axisX()->setRange(m_minX, m_minX + m_rangeX);

    qDebug() << "X Range =" << range;
}

void GraphModifier::adjustYRange(int range)
{
    m_rangeY = range;
    m_graph->axisY()->setRange(m_minY, m_minY + m_rangeY);

    qDebug() << "Y Range =" << range;
}

void GraphModifier::adjustZRange(int range)
{
    m_rangeZ = range;
    m_graph->axisZ()->setRange(m_minZ, m_minZ + m_rangeZ);

    qDebug() << "Z Range =" << range;
}

void GraphModifier::adjustXMin(int min)
{
    m_minX = min;
    m_graph->axisX()->setRange(m_minX, m_minX + m_rangeX);

    qDebug() << "X Minimum =" << min;
}

void GraphModifier::adjustYMin(int min)
{
    m_minY = min;
    m_graph->axisY()->setRange(m_minY, m_minY + m_rangeY);

    qDebug() << "Y Minimum =" << min;
}

void GraphModifier::adjustZMin(int min)
{
    m_minZ = min;
    m_graph->axisZ()->setRange(m_minZ, m_minZ + m_rangeZ);

    qDebug() << "Z Minimum =" << min;
}

void GraphModifier::gradientPressed()
{
    static Q3DTheme::ColorStyle colorStyle = Q3DTheme::ColorStyleUniform;

    if (colorStyle == Q3DTheme::ColorStyleRangeGradient) {
        colorStyle = Q3DTheme::ColorStyleObjectGradient;
        qDebug() << "Color style: ColorStyleObjectGradient";
    } else if (colorStyle == Q3DTheme::ColorStyleObjectGradient) {
        colorStyle = Q3DTheme::ColorStyleUniform;
        qDebug() << "Color style: ColorStyleUniform";
    } else {
        colorStyle = Q3DTheme::ColorStyleRangeGradient;
        qDebug() << "Color style: ColorStyleRangeGradient";
    }

    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::black);
    gradient.setColorAt(0.33, Qt::blue);
    gradient.setColorAt(0.67, Qt::red);
    gradient.setColorAt(1.0, Qt::yellow);

    QList<QLinearGradient> gradients;
    gradients << gradient;
    m_graph->activeTheme()->setBaseGradients(gradients);
    m_graph->activeTheme()->setColorStyle(colorStyle);

}

void GraphModifier::changeFont(const QFont &font)
{
    QFont newFont = font;
    newFont.setPointSizeF(m_fontSize);
    m_graph->activeTheme()->setFont(newFont);
}

void GraphModifier::changeStyle()
{
    m_graph->activeTheme()->setLabelBackgroundEnabled(!m_graph->activeTheme()->isLabelBackgroundEnabled());
}

void GraphModifier::selectButtonClicked()
{
    QSurfaceDataProxy *proxy = m_theSeries->dataProxy();
    int row = rand() % proxy->rowCount();
    int col = rand() % proxy->columnCount();

    m_theSeries->setSelectedPoint(QPoint(row, col));
}

void GraphModifier::selectedPointChanged(const QPoint &point)
{
    QString labelText = QStringLiteral("Selected row: %1, column: %2").arg(point.x()).arg(point.y());
    m_selectionInfoLabel->setText(labelText);
}

void GraphModifier::changeTheme(int theme)
{
    m_graph->activeTheme()->setType(Q3DTheme::Theme(theme));
}


void GraphModifier::flipViews()
{
    m_graph->scene()->setSecondarySubviewOnTop(!m_graph->scene()->isSecondarySubviewOnTop());
}

void GraphModifier::timeout()
{
    int rows = m_planeArray->size();
    int columns = m_planeArray->at(0)->size();

    // Induce minor random jitter to the existing plane array
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            (*m_planeArray->at(i))[j].setX(m_planeArray->at(i)->at(j).x()
                                           * ((float((rand() % 10) + 5.0f) / 10000.0f) + 0.999f));
            (*m_planeArray->at(i))[j].setY(m_planeArray->at(i)->at(j).y()
                                           * ((float((rand() % 10) + 5.0f) / 1000.0f) + 0.99f) + 0.0001f);
            (*m_planeArray->at(i))[j].setZ(m_planeArray->at(i)->at(j).z()
                                           * ((float((rand() % 10) + 5.0f) / 10000.0f) + 0.999f));
        }
    }

    // Reset same array to make it redraw
    m_theSeries->dataProxy()->resetArray(m_planeArray);
}

void GraphModifier::graphQueryTimeout()
{
#ifndef QT_NO_CURSOR
    m_graph->scene()->setGraphPositionQuery(m_parentWidget->mapFromGlobal(QCursor::pos()));
    qDebug() << "pos: " << (m_parentWidget->mapFromGlobal(QCursor::pos()));
#else
    m_graph->scene()->setGraphPositionQuery(QPoint(100, 100));
#endif
}

void GraphModifier::handleAxisXChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_graph->axisX());
}

void GraphModifier::handleAxisYChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_graph->axisY());
}

void GraphModifier::handleAxisZChanged(QValue3DAxis *axis)
{
    qDebug() << __FUNCTION__ << axis << axis->orientation() << (axis == m_graph->axisZ());
}

void GraphModifier::handleFpsChange(qreal fps)
{
    qDebug() << "FPS:" << fps;
}

void GraphModifier::changeLabelRotation(int rotation)
{
    m_graph->axisX()->setLabelAutoRotation(float(rotation));
    m_graph->axisY()->setLabelAutoRotation(float(rotation));
    m_graph->axisZ()->setLabelAutoRotation(float(rotation));
}

void GraphModifier::toggleAxisTitleVisibility(bool enabled)
{
    m_graph->axisX()->setTitleVisible(enabled);
    m_graph->axisY()->setTitleVisible(enabled);
    m_graph->axisZ()->setTitleVisible(enabled);
}

void GraphModifier::toggleAxisTitleFixed(bool enabled)
{
    m_graph->axisX()->setTitleFixed(enabled);
    m_graph->axisY()->setTitleFixed(enabled);
    m_graph->axisZ()->setTitleFixed(enabled);
}

void GraphModifier::toggleXAscending(bool enabled)
{
    m_ascendingX = enabled;

    // Flip data array contents if necessary
    foreach (QSurface3DSeries *series, m_graph->seriesList()) {
        QSurfaceDataArray *array = const_cast<QSurfaceDataArray *>(series->dataProxy()->array());
        const int rowCount = array->size();
        const int columnCount = array->at(0)->size();
        const bool dataAscending = array->at(0)->at(0).x() < array->at(0)->at(columnCount - 1).x();
        if (dataAscending != enabled) {
            // Create new array of equal size
            QSurfaceDataArray *newArray = new QSurfaceDataArray;
            newArray->reserve(rowCount);
            for (int i = 0; i < rowCount; i++)
                newArray->append(new QSurfaceDataRow(columnCount));

            // Flip each row
            for (int i = 0; i < rowCount; i++) {
                QSurfaceDataRow *oldRow = array->at(i);
                QSurfaceDataRow *newRow = newArray->at(i);
                for (int j = 0; j < columnCount; j++)
                    (*newRow)[j] = oldRow->at(columnCount - 1 - j);
            }

            series->dataProxy()->resetArray(newArray);
        }
    }
}

void GraphModifier::toggleZAscending(bool enabled)
{
    m_ascendingZ = enabled;

    // Flip data array contents if necessary
    foreach (QSurface3DSeries *series, m_graph->seriesList()) {
        QSurfaceDataArray *array = const_cast<QSurfaceDataArray *>(series->dataProxy()->array());
        const int rowCount = array->size();
        const int columnCount = array->at(0)->size();
        const bool dataAscending = array->at(0)->at(0).z() < array->at(rowCount - 1)->at(0).z();
        if (dataAscending != enabled) {
            // Create new array of equal size
            QSurfaceDataArray *newArray = new QSurfaceDataArray;
            newArray->reserve(rowCount);
            for (int i = 0; i < rowCount; i++)
                newArray->append(new QSurfaceDataRow(columnCount));

            // Flip each column
            for (int i = 0; i < rowCount; i++) {
                QSurfaceDataRow *oldRow = array->at(rowCount - 1 - i);
                QSurfaceDataRow *newRow = newArray->at(i);
                for (int j = 0; j < columnCount; j++)
                    (*newRow)[j] = oldRow->at(j);
            }

            series->dataProxy()->resetArray(newArray);
        }
    }
}

void GraphModifier::togglePolar(bool enabled)
{
    m_graph->setPolar(enabled);
}

void GraphModifier::setCameraTargetX(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setX(float(value) / 100.0f);
    m_graph->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void GraphModifier::setCameraTargetY(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setY(float(value) / 100.0f);
    m_graph->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void GraphModifier::setCameraTargetZ(int value)
{
    // Value is (-100, 100), normalize
    m_cameraTarget.setZ(float(value) / 100.0f);
    m_graph->scene()->activeCamera()->setTarget(m_cameraTarget);
    qDebug() << "m_cameraTarget:" << m_cameraTarget;
}

void GraphModifier::setGraphMargin(int value)
{
    m_graph->setMargin(qreal(value) / 100.0);
    qDebug() << "Setting margin:" << m_graph->margin() << value;
}

void GraphModifier::resetArrayAndSliders(QSurfaceDataArray *array, float minZ, float maxZ, float minX, float maxX)
{
    m_axisMinSliderX->setValue(minX);
    m_axisMinSliderZ->setValue(minZ);
    m_axisRangeSliderX->setValue(maxX - minX);
    m_axisRangeSliderZ->setValue(maxZ - minZ);

    m_theSeries->dataProxy()->resetArray(array);
}

void GraphModifier::changeShadowQuality(int quality)
{
    QAbstract3DGraph::ShadowQuality sq = QAbstract3DGraph::ShadowQuality(quality);
    m_graph->setShadowQuality(sq);
}

void GraphModifier::changeSelectionMode(int mode)
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if (comboBox) {
        int flags = comboBox->itemData(mode).toInt();
        m_graph->setSelectionMode(QAbstract3DGraph::SelectionFlags(flags));
    }
}

void GraphModifier::changeRow()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Generating new values to a row at random pos";
        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);
        float i = float(rand() % m_zCount);

        QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
        float z = qMin(maxZ, (i * stepZ + minZ));
        for (float j = 0; j < m_xCount; j++) {
            float x = qMin(maxX, (j * stepX + minX));
            float R = qSqrt(x * x + z * z) + 0.01f;
            float y = (qSin(R) / R + 0.24f) * 1.61f + 1.2f;
            (*newRow)[j].setPosition(QVector3D(x, y, z));
        }

        m_theSeries->dataProxy()->setRow(int(i), newRow);
    } else {
#ifdef MULTI_SERIES
        static int changeRowSeries = 0;
        qDebug() << "Generating new values to a row at random pos for series " << changeRowSeries;

        int row = rand() % m_zCount;
        QSurfaceDataRow *newRow = createMultiRow(row, changeRowSeries, true);
        if (m_ascendingZ)
            m_multiseries[changeRowSeries]->dataProxy()->setRow(row, newRow);
        else
            m_multiseries[changeRowSeries]->dataProxy()->setRow((m_zCount - 1) - row, newRow);

        changeRowSeries++;
        if (changeRowSeries > 3)
            changeRowSeries = 0;
#else
        qDebug() << "Change row function active only for SqrtSin";
#endif
    }
}

QSurfaceDataRow *GraphModifier::createMultiRow(int row, int series, bool change)
{
    int full = m_limitX * m_limitZ;
    float i = float(row);
    QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
    float z = float(i) - m_limitZ + 0.5f + m_multiSampleOffsetZ[series];
    if (m_ascendingX) {
        for (int j = 0; j < m_xCount; j++) {
            float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[series];
            float angle = (z * x) / float(full) * 1.57f;
            float y = qSin(angle * float(qPow(1.3f, series))) + 0.2f * float(change) + 1.1f *series;
            (*newRow)[j].setPosition(QVector3D(x, y, z));
        }
    } else {
        for (int j = m_xCount - 1; j >= 0 ; j--) {
            float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[series];
            float angle = (z * x) / float(full) * 1.57f;
            float y = qSin(angle * float(qPow(1.3f, series))) + 0.2f * float(change) + 1.1f *series;
            (*newRow)[(m_xCount - 1) - j].setPosition(QVector3D(x, y, z));
        }
    }

    return newRow;
}

void GraphModifier::populateRisingSeries(QSurface3DSeries *series, int rows, int columns,
                                         float minValue, float maxValue, bool ascendingX,
                                         bool ascendingZ)
{
    QSurfaceDataArray *dataArray = new QSurfaceDataArray;
    dataArray->reserve(rows);
    float range = maxValue - minValue;
    int arraySize = rows * columns;
    for (int i = 0; i < rows; i++) {
        QSurfaceDataRow *dataRow = new QSurfaceDataRow(columns);
        for (int j = 0; j < columns; j++) {
            float xValue = ascendingX ? float(j) : float(columns - j - 1);
            float yValue = minValue + (range * i * j / arraySize);
            float zValue = ascendingZ ? float(i) : float(rows - i - 1);
            (*dataRow)[j].setPosition(QVector3D(xValue, yValue, zValue));
        }
        dataArray->append(dataRow);
    }
    series->dataProxy()->resetArray(dataArray);

}

void GraphModifier::changeRows()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Generating new values to 3 rows from random pos";

        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);
        float start = float(rand() % (m_zCount - 3));

        QSurfaceDataArray dataArray;

        for (float i = start; i < (start + 3.0f); i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            float z = qMin(maxZ, (i * stepZ + minZ));
            for (float j = 0; j < m_xCount; j++) {
                float x = qMin(maxX, (j * stepX + minX));
                float R = qSqrt(x * x + z * z) + 0.01f;
                float y = (qSin(R) / R + 0.24f) * 1.61f + 1.2f;
                (*newRow)[j].setPosition(QVector3D(x, y, z));
            }
            dataArray.append(newRow);
        }

        m_theSeries->dataProxy()->setRows(int(start), dataArray);
    } else {
#ifdef MULTI_SERIES
        static int changeRowSeries = 0;
        qDebug() << "Generating new values for 3 rows at random pos for series " << changeRowSeries;

        int row = rand() % (m_zCount - 3);
        QSurfaceDataArray dataArray;
        for (int i = 0; i < 3; i++) {
            QSurfaceDataRow *newRow = createMultiRow(row + i, changeRowSeries, true);
            dataArray.append(newRow);
        }
        m_multiseries[changeRowSeries]->dataProxy()->setRows(row, dataArray);

        changeRowSeries++;
        if (changeRowSeries > 3)
            changeRowSeries = 0;
#else
        qDebug() << "Change row function active only for SqrtSin";
#endif
    }
}

void GraphModifier::changeItem()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Generating new values for an item at random pos";
        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);
        float i = float(rand() % m_zCount);
        float j = float(rand() % m_xCount);

        float x = qMin(maxX, (j * stepX + minX));
        float z = qMin(maxZ, (i * stepZ + minZ));
        float R = qSqrt(x * x + z * z) + 0.01f;
        float y = (qSin(R) / R + 0.24f) * 1.61f + 1.2f;
        QSurfaceDataItem newItem(QVector3D(x, y, z));

        m_theSeries->dataProxy()->setItem(int(i), int(j), newItem);
    } else {
#ifdef MULTI_SERIES
        static int changeItemSeries = 0;
        int full = m_limitX * m_limitZ;
        float i = float(rand() % m_zCount);
        float j = float(rand() % m_xCount);
        float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[changeItemSeries];
        float z = float(i) - m_limitZ + 0.5f + m_multiSampleOffsetZ[changeItemSeries];
        float angle = (z * x) / float(full) * 1.57f;
        float y = qSin(angle * float(qPow(1.3f, changeItemSeries))) + 0.2f + 1.1f *changeItemSeries;
        QSurfaceDataItem newItem(QVector3D(x, y, z));

        if (m_ascendingZ && m_ascendingX)
            m_multiseries[changeItemSeries]->dataProxy()->setItem(int(i), int(j), newItem);
        else if (!m_ascendingZ && m_ascendingX)
            m_multiseries[changeItemSeries]->dataProxy()->setItem(m_zCount - 1 - int(i), int(j), newItem);
        else if (m_ascendingZ && !m_ascendingX)
            m_multiseries[changeItemSeries]->dataProxy()->setItem(int(i), m_xCount - 1 - int(j), newItem);
        else
            m_multiseries[changeItemSeries]->dataProxy()->setItem(m_zCount - 1 - int(i), m_xCount - 1 - int(j), newItem);
        //m_multiseries[changeItemSeries]->setSelectedPoint(QPoint(i, j));
        changeItemSeries++;
        if (changeItemSeries > 3)
            changeItemSeries = 0;
#else
        qDebug() << "Change item function active only for SqrtSin";
#endif
    }
}

void GraphModifier::changeMultipleRows()
{
    for (int i = 0; i < 30; i++)
        changeRow();
}

void GraphModifier::changeMultipleItem()
{
    for (int i = 0; i < 30; i++)
        changeItem();
}

void GraphModifier::addRow()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Adding a new row";

        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);

        QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
        float z = float(m_addRowCounter) * stepZ + minZ;
        for (float j = 0; j < m_xCount; j++) {
            float x = qMin(maxX, (j * stepX + minX));
            float R = qSqrt(x * x + z * z) + 0.01f;
            float y = (qSin(R) / R + 0.24f) * 1.61f + 1.0f;
            (*newRow)[j].setPosition(QVector3D(x, y, z));
        }
        m_addRowCounter++;

        m_theSeries->dataProxy()->addRow(newRow);
    } else {
#ifdef MULTI_SERIES
        qDebug() << "Adding a row into series 3";
        int full = m_limitX * m_limitZ;
        int series = 2;

        QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
        float z = float(m_addRowCounter) - m_limitZ + 0.5f + m_multiSampleOffsetZ[series];
        for (int j = 0; j < m_xCount; j++) {
            float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[series];
            float angle = float(z * x) / float(full) * 1.57f;
            (*newRow)[j].setPosition(QVector3D(x, qSin(angle *float(qPow(1.3f, series))) + 1.1f * series, z));
        }
        m_addRowCounter++;

        m_multiseries[series]->dataProxy()->addRow(newRow);
#else
            qDebug() << "Add row function active only for SqrtSin";
#endif
    }
}

void GraphModifier::addRows()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Adding few new row";

        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);

        QSurfaceDataArray dataArray;

        for (int i = 0; i < 3; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            float z = m_addRowCounter * stepZ + minZ;
            for (float j = 0; j < m_xCount; j++) {
                float x = qMin(maxX, (j * stepX + minX));
                float R = qSqrt(x * x + z * z) + 0.01f;
                float y = (qSin(R) / R + 0.24f) * 1.61f + 1.0f;
                (*newRow)[j].setPosition(QVector3D(x, y, z));
            }
            dataArray.append(newRow);
            m_addRowCounter++;
        }

        m_theSeries->dataProxy()->addRows(dataArray);
    } else {
#ifdef MULTI_SERIES
    qDebug() << "Adding 3 rows into series 3";
    int changedSeries = 2;

    QSurfaceDataArray dataArray;
    for (int i = 0; i < 3; i++) {
        QSurfaceDataRow *newRow = createMultiRow(m_addRowCounter, changedSeries, false);
        dataArray.append(newRow);
        m_addRowCounter++;
    }

    m_multiseries[changedSeries]->dataProxy()->addRows(dataArray);
#else
        qDebug() << "Add rows function active only for SqrtSin";
#endif
    }
}

void GraphModifier::insertRow()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Inserting a row";
        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);

        QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
        float z = qMin(maxZ, (float(m_insertTestZPos) * stepZ + minZ + (stepZ / 2.0f)));
        for (float j = 0; j < m_xCount; j++) {
            float x = qMin(maxX, (j * stepX + minX));
            float R = qSqrt(x * x + z * z) + 0.01f;
            float y = (qSin(R) / R + 0.24f) * 1.61f + 1.3f;
            (*newRow)[j].setPosition(QVector3D(x, y, z));
        }
        m_insertTestZPos++;

        m_theSeries->dataProxy()->insertRow(m_insertTestIndexPos, newRow);
        m_insertTestIndexPos += 2;
    } else {
#ifdef MULTI_SERIES
    qDebug() << "Inserting a row into series 3";
    int full = m_limitX * m_limitZ;
    int changedSeries = 2;

    QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
    float z = float(m_insertTestZPos) - m_limitZ + m_multiSampleOffsetZ[changedSeries];
    for (int j = 0; j < m_xCount; j++) {
        float x = float(j) - m_limitX + m_multiSampleOffsetX[changedSeries];
        float angle = (z * x) / float(full) * 1.57f;
        (*newRow)[j].setPosition(QVector3D(x + 0.5f,
                                           qSin(angle * float(qPow(1.3f, changedSeries))) + 1.2f * changedSeries,
                                           z + 1.0f));
    }

    m_insertTestZPos++;

    m_multiseries[2]->dataProxy()->insertRow(m_insertTestIndexPos, newRow);
    m_insertTestIndexPos += 2;
#else
        qDebug() << "Insert row function active only for SqrtSin";
#endif
    }
}

void GraphModifier::insertRows()
{
    if (m_activeSample == GraphModifier::SqrtSin) {
        qDebug() << "Inserting 3 rows";
        float minX = -10.0f;
        float maxX = 10.0f;
        float minZ = -10.0f;
        float maxZ = 10.0f;
        float stepX = (maxX - minX) / float(m_xCount - 1);
        float stepZ = (maxZ - minZ) / float(m_zCount - 1);

        QSurfaceDataArray dataArray;
        for (int i = 0; i < 3; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
            float z = qMin(maxZ, (float(m_insertTestZPos) * stepZ + minZ + i * (stepZ / 4.0f)));
            for (float j = 0; j < m_xCount; j++) {
                float x = qMin(maxX, (j * stepX + minX));
                float R = qSqrt(x * x + z * z) + 0.01f;
                float y = (qSin(R) / R + 0.24f) * 1.61f + 1.3f;
                (*newRow)[j].setPosition(QVector3D(x, y, z));
            }
            dataArray.append(newRow);
        }
        m_insertTestZPos++;

        m_theSeries->dataProxy()->insertRows(m_insertTestIndexPos, dataArray);
        m_insertTestIndexPos += 4;
    } else {
#ifdef MULTI_SERIES
    qDebug() << "Inserting 3 rows into series 3";
    int full = m_limitX * m_limitZ;
    int changedSeries = 2;

    QSurfaceDataArray dataArray;
    float zAdd = 0.25f;
    for (int i = 0; i < 3; i++) {
        QSurfaceDataRow *newRow = new QSurfaceDataRow(m_xCount);
        float z = float(m_insertTestZPos) - m_limitZ  + 0.5f + zAdd + m_multiSampleOffsetZ[changedSeries];
        for (int j = 0; j < m_xCount; j++) {
            float x = float(j) - m_limitX + 0.5f + m_multiSampleOffsetX[changedSeries];
            float angle = (z * x) / float(full) * 1.57f;
            float y = qSin(angle * float(qPow(1.3f, changedSeries))) + + 1.2f * changedSeries;
            (*newRow)[j].setPosition(QVector3D(x, y, z));
        }
        zAdd += 0.25f;
        dataArray.append(newRow);
    }

    m_insertTestZPos++;

    m_multiseries[2]->dataProxy()->insertRows(m_insertTestIndexPos, dataArray);
    m_insertTestIndexPos += 4;
#else
        qDebug() << "Insert rows function active only for SqrtSin";
#endif
    }
}

void GraphModifier::removeRow()
{
    qDebug() << "Remove an arbitrary row";
    if (m_zCount < 1)
        return;

    int row = rand() % m_zCount;

#ifdef MULTI_SERIES
    int series = rand() % 4;
    m_multiseries[series]->dataProxy()->removeRows(row, 1);
#else
    m_theSeries->dataProxy()->removeRows(row, 1);
#endif
    m_zCount--;
}

void GraphModifier::resetArray()
{
    qDebug() << "Reset series data array";
    int rows = 10;
    int columns = 10;
    float randFactor = float(rand() % 100) / 100.0f;
    QSurfaceDataArray *planeArray = new QSurfaceDataArray;
    planeArray->reserve(rows);

    for (int i = 0; i < rows; i++) {
        planeArray->append(new QSurfaceDataRow);
        (*planeArray)[i]->resize(columns);
        for (int j = 0; j < columns; j++) {
            (*planeArray->at(i))[j].setX(float(j) * randFactor);
            (*planeArray->at(i))[j].setY(float(i - j) * randFactor);
            (*planeArray->at(i))[j].setZ(float(i));
        }
    }

#ifdef MULTI_SERIES
    int series = rand() % 4;
    m_multiseries[series]->dataProxy()->resetArray(planeArray);
#else
    m_theSeries->dataProxy()->resetArray(planeArray);
#endif
}

void GraphModifier::resetArrayEmpty()
{
    QSurfaceDataArray *emptyArray = new QSurfaceDataArray;
#ifdef MULTI_SERIES
    int series = rand() % 4;
    m_multiseries[series]->dataProxy()->resetArray(emptyArray);
#else
    m_theSeries->dataProxy()->resetArray(emptyArray);
#endif
}

void GraphModifier::massiveDataTest()
{
    static int testPhase = 0;
    static const int cacheSize = 1000;
    const int columns = 200;
    const int rows = 200000;
    const int visibleRows = 200;
    const float yRangeMin = 0.0f;
    const float yRangeMax = 1.0f;
    const float yRangeMargin = 0.05f;
    static QTimer *massiveTestTimer = 0;
    static QSurface3DSeries *series = new QSurface3DSeries;

    // To speed up massive array creation, we generate a smaller cache array
    // and copy rows from that to our main array
    if (!m_massiveTestCacheArray.size()) {
        m_massiveTestCacheArray.reserve(cacheSize);
        float minY = yRangeMin + yRangeMargin;
        float maxY = yRangeMax - yRangeMargin;
        float rowBase = minY;
        float direction = 1.0f;
        for (int i = 0; i < cacheSize; i++) {
            m_massiveTestCacheArray.append(new QSurfaceDataRow);
            m_massiveTestCacheArray[i]->resize(columns);
            rowBase += direction * (float(rand() % 3) / 100.0f);
            if (rowBase > maxY) {
                rowBase = maxY;
                direction = -1.0f;
            } else if (rowBase < minY) {
                rowBase = minY;
                direction = 1.0f;
            }
            for (int j = 0; j < columns; j++) {
                float randFactor = float(rand() % 100) / (100 / yRangeMargin);
                (*m_massiveTestCacheArray.at(i))[j].setX(float(j));
                (*m_massiveTestCacheArray.at(i))[j].setY(rowBase + randFactor);
                // Z value is irrelevant, we replace it anyway when we take row to use
            }
        }
        massiveTestTimer = new QTimer;
    }

    switch (testPhase) {
    case 0: {
        qDebug() << __FUNCTION__ << testPhase << ": Setting the graph up...";
        QValue3DAxis *xAxis = new QValue3DAxis();
        QValue3DAxis *yAxis = new QValue3DAxis();
        QValue3DAxis *zAxis = new QValue3DAxis();
        xAxis->setRange(0.0f, float(columns));
        yAxis->setRange(yRangeMin, yRangeMax);
        zAxis->setRange(0.0f, float(visibleRows));
        xAxis->setSegmentCount(1);
        yAxis->setSegmentCount(1);
        zAxis->setSegmentCount(1);
        m_graph->setMeasureFps(true);
        m_graph->setAxisX(xAxis);
        m_graph->setAxisY(yAxis);
        m_graph->setAxisZ(zAxis);
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetRight);
        m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
        foreach (QAbstract3DSeries *series, m_graph->seriesList())
            m_graph->removeSeries(static_cast<QSurface3DSeries *>(series));

        qDebug() << __FUNCTION__ << testPhase << ": Creating massive array..."
                 << rows << "x" << columns;
        // Reset to zero first to avoid having memory allocated for two massive arrays at the same
        // time on the second and subsequent runs.
        series->dataProxy()->resetArray(0);
        QSurfaceDataArray *massiveArray = new QSurfaceDataArray;
        massiveArray->reserve(rows);

        for (int i = 0; i < rows; i++) {
            QSurfaceDataRow *newRow = new QSurfaceDataRow(*m_massiveTestCacheArray.at(i % cacheSize));
            for (int j = 0; j < columns; j++)
                (*newRow)[j].setZ(float(i));
            massiveArray->append(newRow);
        }
        qDebug() << __FUNCTION__ << testPhase << ": Massive array creation finished!";

        series->dataProxy()->resetArray(massiveArray);
        m_graph->addSeries(series);
        break;
    }
    case 1: {
        qDebug() << __FUNCTION__ << testPhase << ": Scroll";
        QObject::disconnect(massiveTestTimer, 0, this, 0);
        QObject::connect(massiveTestTimer, &QTimer::timeout, this,
                         &GraphModifier::massiveTestScroll);
        massiveTestTimer->start(16);
        break;
    }
    case 2: {
        qDebug() << __FUNCTION__ << testPhase << ": Append and scroll";
        massiveTestTimer->stop();
        QObject::disconnect(massiveTestTimer, 0, this, 0);
        QObject::connect(massiveTestTimer, &QTimer::timeout, this,
                         &GraphModifier::massiveTestAppendAndScroll);
        m_graph->axisZ()->setRange(rows - visibleRows, rows);
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

void GraphModifier::massiveTestScroll()
{
    const int scrollAmount = 20;
    int maxRows = m_graph->seriesList().at(0)->dataProxy()->rowCount();
    int min = m_graph->axisZ()->min() + scrollAmount;
    int max = m_graph->axisZ()->max() + scrollAmount;
    if (max >= maxRows) {
        max = max - min;
        min = 0;
    }
    m_graph->axisZ()->setRange(min, max);
}

void GraphModifier::massiveTestAppendAndScroll()
{
    const int addedRows = 50;
    int maxRows = m_graph->seriesList().at(0)->dataProxy()->rowCount();
    int columns = m_graph->seriesList().at(0)->dataProxy()->columnCount();

    QSurfaceDataArray appendArray;
    appendArray.reserve(addedRows);
    for (int i = 0; i < addedRows; i++) {
        QSurfaceDataRow *newRow = new QSurfaceDataRow(*m_massiveTestCacheArray.at((i + maxRows) % 1000));
        for (int j = 0; j < columns; j++)
            (*newRow)[j].setZ(float(maxRows + i));
        appendArray.append(newRow);
    }
    m_graph->seriesList().at(0)->dataProxy()->addRows(appendArray);
    int min = m_graph->axisZ()->min() + addedRows;
    int max = m_graph->axisZ()->max() + addedRows;
    m_graph->axisZ()->setRange(min, max);
}

void GraphModifier::testAxisReverse()
{
    static int counter = 0;
    const int rowCount = 16;
    const int colCount = 16;
    static QSurface3DSeries *series0 = 0;
    static QSurface3DSeries *series1 = 0;

    switch (counter) {
    case 0: {
        qDebug() << __FUNCTION__ << counter << "Setup test";
        foreach (QSurface3DSeries *series, m_graph->seriesList())
            m_graph->removeSeries(series);
        foreach (QValue3DAxis *axis, m_graph->axes())
            m_graph->releaseAxis(axis);
        delete series0;
        delete series1;
        series0 = new QSurface3DSeries;
        series1 = new QSurface3DSeries;
        populateRisingSeries(series0, rowCount, colCount, 0.0f, 50.0f, true, true);
        populateRisingSeries(series1, rowCount, colCount, -20.0f, 30.0f, true, true);
        m_graph->axisX()->setRange(0.0f, 10.0f);
        m_graph->axisY()->setRange(-20.0f, 50.0f);
        m_graph->axisZ()->setRange(5.0f, 15.0f);
        m_graph->addSeries(series0);
        m_graph->addSeries(series1);
    }
        break;
    case 1: {
        qDebug() << __FUNCTION__ << counter << "Reverse X axis";
        m_graph->axisX()->setReversed(true);
    }
        break;
    case 2: {
        qDebug() << __FUNCTION__ << counter << "Reverse Y axis";
        m_graph->axisY()->setReversed(true);
    }
        break;
    case 3: {
        qDebug() << __FUNCTION__ << counter << "Reverse Z axis";
        m_graph->axisZ()->setReversed(true);
    }
        break;
    case 4: {
        qDebug() << __FUNCTION__ << counter << "Return all axes to normal";
        m_graph->axisX()->setReversed(false);
        m_graph->axisY()->setReversed(false);
        m_graph->axisZ()->setReversed(false);
    }
        break;
    case 5: {
        qDebug() << __FUNCTION__ << counter << "Reverse all axes";
        m_graph->axisX()->setReversed(true);
        m_graph->axisY()->setReversed(true);
        m_graph->axisZ()->setReversed(true);
    }
        break;
    default:
        qDebug() << __FUNCTION__ << "Resetting test";
        counter = -1;
    }
    counter++;
}

void GraphModifier::testDataOrdering()
{
    static int counter = 0;
    const int rowCount = 20;
    const int colCount = 20;
    static QSurface3DSeries *series0 = 0;
    static QSurface3DSeries *series1 = 0;
    const float series0min = 0.0f;
    const float series0max = 50.0f;
    const float series1min = -20.0f;
    const float series1max = 30.0f;

    switch (counter) {
    case 0: {
        qDebug() << __FUNCTION__ << counter << "Setup test - both ascending";
        foreach (QSurface3DSeries *series, m_graph->seriesList())
            m_graph->removeSeries(series);
        foreach (QValue3DAxis *axis, m_graph->axes())
            m_graph->releaseAxis(axis);
        delete series0;
        delete series1;
        series0 = new QSurface3DSeries;
        series1 = new QSurface3DSeries;
        populateRisingSeries(series0, rowCount, colCount, series0min, series0max, true, true);
        populateRisingSeries(series1, rowCount, colCount, series1min, series1max, true, true);
        m_graph->axisX()->setRange(5.0f, 15.0f);
        m_graph->axisY()->setRange(-20.0f, 50.0f);
        m_graph->axisZ()->setRange(5.0f, 15.0f);
        m_graph->addSeries(series0);
        m_graph->addSeries(series1);
    }
        break;
    case 1: {
        qDebug() << __FUNCTION__ << counter << "Ascending X, descending Z";
        populateRisingSeries(series0, rowCount, colCount, series0min, series0max, true, false);
        populateRisingSeries(series1, rowCount, colCount, series1min, series1max, true, false);
    }
        break;
    case 2: {
        qDebug() << __FUNCTION__ << counter << "Descending X, ascending Z";
        populateRisingSeries(series0, rowCount, colCount, series0min, series0max, false, true);
        populateRisingSeries(series1, rowCount, colCount, series1min, series1max, false, true);
    }
        break;
    case 3: {
        qDebug() << __FUNCTION__ << counter << "Both descending";
        populateRisingSeries(series0, rowCount, colCount, series0min, series0max, false, false);
        populateRisingSeries(series1, rowCount, colCount, series1min, series1max, false, false);
    }
        break;
    default:
        qDebug() << __FUNCTION__ << "Resetting test";
        counter = -1;
    }
    counter++;
}

void GraphModifier::changeMesh()
{
    static int model = 0;
    switch (model) {
    case 0:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshCylinder);
        m_graph->seriesList().at(0)->setMeshSmooth(false);
        break;
    case 1:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshCylinder);
        m_graph->seriesList().at(0)->setMeshSmooth(true);
        break;
    case 2:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshCone);
        m_graph->seriesList().at(0)->setMeshSmooth(false);
        break;
    case 3:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshCone);
        m_graph->seriesList().at(0)->setMeshSmooth(true);
        break;
    case 4:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshBar);
        m_graph->seriesList().at(0)->setMeshSmooth(false);
        break;
    case 5:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshBar);
        m_graph->seriesList().at(0)->setMeshSmooth(true);
        break;
    case 6:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshPyramid);
        m_graph->seriesList().at(0)->setMeshSmooth(false);
        break;
    case 7:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshPyramid);
        m_graph->seriesList().at(0)->setMeshSmooth(true);
        break;
    case 8:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshBevelBar);
        m_graph->seriesList().at(0)->setMeshSmooth(false);
        break;
    case 9:
        m_graph->seriesList().at(0)->setMesh(QAbstract3DSeries::MeshBevelBar);
        m_graph->seriesList().at(0)->setMeshSmooth(true);
        break;
    }
    model++;
    if (model > 9)
        model = 0;
}

void GraphModifier::updateSamples()
{
    switch (m_activeSample) {
    case SqrtSin:
        toggleSqrtSin(true);
        break;

    case Plane:
        togglePlane(true);
        break;

    default:
        break;
    }
}

void GraphModifier::setAspectRatio(int ratio)
{
    qreal aspectRatio = qreal(ratio) / 10.0;
    m_graph->setAspectRatio(aspectRatio);
}

void GraphModifier::setHorizontalAspectRatio(int ratio)
{
    qreal aspectRatio = qreal(ratio) / 100.0;
    m_graph->setHorizontalAspectRatio(aspectRatio);
}

void GraphModifier::setSurfaceTexture(bool enabled)
{
    if (enabled)
        m_multiseries[3]->setTexture(QImage(":/maps/mapimage"));
    else
        m_multiseries[3]->setTexture(QImage());
}
