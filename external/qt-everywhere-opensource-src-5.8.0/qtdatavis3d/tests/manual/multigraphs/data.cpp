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

#define NOMINMAX

#include "data.h"
#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/Q3DCamera>
#include <QtDataVisualization/QBar3DSeries>
#include <QtDataVisualization/QScatter3DSeries>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/Q3DTheme>
#include <QScrollBar>
#include <QSize>
#include <QImage>

using namespace QtDataVisualization;

Data::Data(Q3DSurface *surface, Q3DScatter *scatter, Q3DBars *bars,
           QTextEdit *statusArea, QWidget *widget) :
    m_surface(surface),
    m_scatter(scatter),
    m_bars(bars),
    m_statusArea(statusArea),
    m_widget(widget),
    m_resize(true),
    m_resolution(QSize(300, 300)),
    m_resolutionLevel(0),
    m_mode(Surface),
    m_scatterDataArray(0),
    m_barDataArray(0),
    m_started(false)
{
    // Initialize surface
    m_surface->activeTheme()->setType(Q3DTheme::ThemeIsabelle);
    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::black);
    gradient.setColorAt(0.33, Qt::blue);
    gradient.setColorAt(0.67, Qt::red);
    gradient.setColorAt(1.0, Qt::yellow);
    m_surface->setSelectionMode(QAbstract3DGraph::SelectionNone);
    m_surface->activeTheme()->setGridEnabled(false);
    m_surface->activeTheme()->setBackgroundEnabled(false);
    m_surface->scene()->activeCamera()->setCameraPosition(0.0, 90.0, 150.0);
    QSurface3DSeries *series1 = new QSurface3DSeries(new QHeightMapSurfaceDataProxy());
    series1->setFlatShadingEnabled(true);
    series1->setDrawMode(QSurface3DSeries::DrawSurface);
    series1->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    series1->setBaseGradient(gradient);
    m_surface->addSeries(series1);

    // Initialize scatter
    m_scatter->activeTheme()->setType(Q3DTheme::ThemeStoneMoss);
    m_scatter->setSelectionMode(QAbstract3DGraph::SelectionNone);
    m_scatter->activeTheme()->setGridEnabled(false);
    m_scatter->setShadowQuality(QAbstract3DGraph::ShadowQualitySoftLow);
    m_scatter->scene()->activeCamera()->setCameraPosition(0.0, 85.0, 150.0);
    QScatter3DSeries *series2 = new QScatter3DSeries;
    series2->setMesh(QAbstract3DSeries::MeshPoint);
    m_scatter->addSeries(series2);

    // Initialize bars
    m_bars->activeTheme()->setType(Q3DTheme::ThemeQt);
    m_bars->setSelectionMode(QAbstract3DGraph::SelectionItemAndRow | QAbstract3DGraph::SelectionSlice);
    m_bars->activeTheme()->setGridEnabled(false);
    m_bars->setShadowQuality(QAbstract3DGraph::ShadowQualityLow);
    m_bars->setBarSpacing(QSizeF(0.0, 0.0));
    m_bars->scene()->activeCamera()->setCameraPosition(0.0, 75.0, 150.0);
    QBar3DSeries *series3 = new QBar3DSeries;
    series3->setMesh(QAbstract3DSeries::MeshBar);
    m_bars->addSeries(series3);

    // Hide scroll bar
    m_statusArea->verticalScrollBar()->setVisible(false);
}

Data::~Data()
{
    delete m_bars;
    delete m_surface;
    delete m_scatter;
    delete m_widget;
    delete m_scatterDataArray; // only portion of this array is set to graph
}

void Data::updateData()
{
    if (!m_started) {
        m_statusArea->append(QStringLiteral("<i>We are stopped. The changes will take effect once started.</i>"));
        return;
    }
    QImage depthMap = QImage(":/australia.png");
    if (m_resize) // Resize for better performance
        depthMap = depthMap.scaled(m_resolution);
    if (m_mode != Surface)
        setData(depthMap);
    else
        static_cast<QHeightMapSurfaceDataProxy *>(m_surface->seriesList().at(0)->dataProxy())->setHeightMap(
                depthMap);
}

void Data::clearData()
{
    m_bars->seriesList().at(0)->dataProxy()->resetArray(0);
    m_scatter->seriesList().at(0)->dataProxy()->resetArray(0);
    m_surface->seriesList().at(0)->dataProxy()->resetArray(0);
}

void Data::setResolution(int selection)
{
    m_resolutionLevel = selection;
    switch (selection) {
    case 0: {
        m_resize = true;
        m_resolution = QSize(300, 300);
        break;
    }
    case 1: {
        m_resize = true;
        m_resolution = QSize(600, 600);
        break;
    }
    case 2: {
        m_resize = true;
        m_resolution = QSize(800, 800);
        break;
    }
    case 3: {
        m_resize = false;
        m_resolution = QSize(1020, 1020); // image size
        break;
    }
    };
    if (m_mode == Scatter) {
        m_resize = true;
        m_resolution /= 3;
        delete m_scatterDataArray;
        m_scatterDataArray = new QScatterDataArray;
        m_scatterDataArray->resize(m_resolution.width() * m_resolution.height());
    } else if (m_mode == Bars) {
        m_resize = true;
        m_resolution /= 6;
        m_barDataArray = new QBarDataArray;
        m_barDataArray->reserve(m_resolution.height());
        for (int i = 0; i < m_resolution.height(); i++) {
            QBarDataRow *newProxyRow = new QBarDataRow(m_resolution.width());
            m_barDataArray->append(newProxyRow);
        }
    }

    m_statusArea->append(QString(QStringLiteral("<b>Resolution:</b> %1 x %2")).arg(
                             m_resolution.width()).arg(m_resolution.height()));

    updateData();
}

void Data::scrollDown()
{
    QScrollBar *scrollbar = m_statusArea->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void Data::useGradientOne()
{
    m_surface->activeTheme()->setType(Q3DTheme::ThemeIsabelle);
    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::black);
    gradient.setColorAt(0.33, Qt::blue);
    gradient.setColorAt(0.67, Qt::red);
    gradient.setColorAt(1.0, Qt::yellow);
    m_surface->seriesList().at(0)->setBaseGradient(gradient);
    m_surface->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_statusArea->append(QStringLiteral("<b>Colors:</b> Thermal image imitation"));
}

void Data::useGradientTwo()
{
    m_surface->activeTheme()->setType(Q3DTheme::ThemeQt);
    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(0.8, Qt::red);
    gradient.setColorAt(1.0, Qt::green);
    m_surface->seriesList().at(0)->setBaseGradient(gradient);
    m_surface->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_statusArea->append(QStringLiteral("<b>Colors:</b> Highlight foreground"));
}

void Data::setData(const QImage &image)
{
    QImage heightImage = image;

    uchar *bits = heightImage.bits();

    int imageHeight = heightImage.height();
    int imageWidth = heightImage.width();
    int bitCount = imageWidth * 4 * (imageHeight - 1);
    int widthBits = imageWidth * 4;

    if (m_mode == Scatter) {
        QScatterDataItem *ptrToDataArray = &m_scatterDataArray->first();

        int limitsX = imageWidth / 2;
        int limitsZ = imageHeight / 2;
        float height = 0;
        int count = 0;

        for (int i = -limitsZ; i < limitsZ; i++, bitCount -= widthBits) {
            for (int j = -limitsX; j < limitsX; j++) {
                height = float(bits[bitCount + ((j + limitsX) * 4)]) - 128.0;
                if (height > -128) {
                    ptrToDataArray->setPosition(QVector3D(float(j), height, float(i)));
                    ptrToDataArray++;
                    count++;
                }
            }
        }

        QScatterDataArray *dataArray = new QScatterDataArray(m_scatterDataArray->mid(0, count));
        m_scatter->seriesList().at(0)->dataProxy()->resetArray(dataArray);
    } else {
        QBarDataArray *dataArray = m_barDataArray;
        for (int i = 0; i < imageHeight; i++, bitCount -= widthBits) {
            QBarDataRow &newRow = *dataArray->at(i);
            for (int j = 0; j < imageWidth; j++)
                newRow[j] = float(bits[bitCount + (j * 4)]);
        }

        m_bars->seriesList().at(0)->dataProxy()->resetArray(dataArray);
    }
}

void Data::changeMode(int mode)
{
    m_mode = VisualizationMode(mode);
    switch (m_mode) {
    case Surface: {
        m_statusArea->append(QStringLiteral("<b>Visualization Type:</b> Surface"));
        break;
    }
    case Scatter: {
        m_statusArea->append(QStringLiteral("<b>Visualization Type:</b> Scatter"));
        break;
    }
    default: {
        m_statusArea->append(QStringLiteral("<b>Visualization Type:</b> Bars"));
        break;
    }
    }
    // Reset resolution after mode change
    setResolution(m_resolutionLevel);
}

void Data::start()
{
    m_started = true;
    // Reset resolution before starting (otherwise restart will crash due to empty data)
    setResolution(m_resolutionLevel);
    updateData();
    m_statusArea->append(QStringLiteral("<b>Started<\b>"));
}

void Data::stop()
{
    m_started = false;
    clearData();
    m_statusArea->append(QStringLiteral("<b>Stopped<\b>"));
}

ContainerChanger::ContainerChanger(QWidget *surface, QWidget *scatter, QWidget *bars,
                                   QWidget *buttonOne, QWidget *buttonTwo)
    : m_surface(surface),
      m_scatter(scatter),
      m_bars(bars),
      m_button1(buttonOne),
      m_button2(buttonTwo)
{
}

ContainerChanger::~ContainerChanger()
{
}

void ContainerChanger::changeContainer(int container)
{
    switch (container) {
    case 0: {
        m_scatter->setVisible(false);
        m_bars->setVisible(false);
        m_surface->setVisible(true);
        m_button1->setEnabled(true);
        m_button2->setEnabled(true);
        break;
    }
    case 1: {
        m_surface->setVisible(false);
        m_bars->setVisible(false);
        m_scatter->setVisible(true);
        m_button1->setEnabled(false);
        m_button2->setEnabled(false);
        break;
    }
    case 2: {
        m_surface->setVisible(false);
        m_scatter->setVisible(false);
        m_bars->setVisible(true);
        m_button1->setEnabled(false);
        m_button2->setEnabled(false);
        break;
    }
    }
}
