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

#include "customitemgraph.h"

#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/QCustom3DItem>
#include <QtDataVisualization/QCustom3DLabel>
#include <QtGui/QImage>

using namespace QtDataVisualization;

CustomItemGraph::CustomItemGraph(Q3DSurface *surface, QLabel *label)
    : m_graph(surface),
      m_textField(label),
      m_previouslyAnimatedItem(0)
{
    QImage layerOneHMap(":/maps/layer_1.png");
    QHeightMapSurfaceDataProxy *layerOneProxy = new QHeightMapSurfaceDataProxy(layerOneHMap);
    QSurface3DSeries *layerOneSeries = new QSurface3DSeries(layerOneProxy);
    layerOneSeries->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel): @yLabel"));
    layerOneProxy->setValueRanges(34.0f, 40.0f, 18.0f, 24.0f);
    layerOneSeries->setDrawMode(QSurface3DSeries::DrawSurface);
    layerOneSeries->setFlatShadingEnabled(false);

    QImage layerTwoHMap(":/maps/layer_2.png");
    QHeightMapSurfaceDataProxy *layerTwoProxy = new QHeightMapSurfaceDataProxy(layerTwoHMap);
    QSurface3DSeries *layerTwoSeries = new QSurface3DSeries(layerTwoProxy);
    layerTwoSeries->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel): @yLabel"));
    layerTwoProxy->setValueRanges(34.0f, 40.0f, 18.0f, 24.0f);
    layerTwoSeries->setDrawMode(QSurface3DSeries::DrawSurface);
    layerTwoSeries->setFlatShadingEnabled(false);

    QImage layerThreeHMap(":/maps/layer_3.png");
    QHeightMapSurfaceDataProxy *layerThreeProxy = new QHeightMapSurfaceDataProxy(layerThreeHMap);
    QSurface3DSeries *layerThreeSeries = new QSurface3DSeries(layerThreeProxy);
    layerThreeSeries->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel): @yLabel"));
    layerThreeProxy->setValueRanges(34.0f, 40.0f, 18.0f, 24.0f);
    layerThreeSeries->setDrawMode(QSurface3DSeries::DrawSurface);
    layerThreeSeries->setFlatShadingEnabled(false);

    m_graph->axisX()->setLabelFormat("%.1f N");
    m_graph->axisZ()->setLabelFormat("%.1f E");
    m_graph->axisX()->setRange(34.0f, 40.0f);
    m_graph->axisY()->setRange(0.0f, 200.0f);
    m_graph->axisZ()->setRange(18.0f, 24.0f);

    m_graph->axisX()->setTitle(QStringLiteral("Latitude"));
    m_graph->axisY()->setTitle(QStringLiteral("Height"));
    m_graph->axisZ()->setTitle(QStringLiteral("Longitude"));

    m_graph->addSeries(layerOneSeries);
    m_graph->addSeries(layerTwoSeries);
    m_graph->addSeries(layerThreeSeries);

    QLinearGradient grOne;
    grOne.setColorAt(0.0, Qt::black);
    grOne.setColorAt(0.38, Qt::darkYellow);
    grOne.setColorAt(0.39, Qt::darkGreen);
    grOne.setColorAt(0.5, Qt::darkGray);
    grOne.setColorAt(1.0, Qt::gray);
    m_graph->seriesList().at(0)->setBaseGradient(grOne);
    m_graph->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);

    QLinearGradient grTwo;
    grTwo.setColorAt(0.385, Qt::blue);
    grTwo.setColorAt(0.395, Qt::white);
    m_graph->seriesList().at(1)->setBaseGradient(grTwo);
    m_graph->seriesList().at(1)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);

    QLinearGradient grThree;
    grThree.setColorAt(0.0, Qt::white);
    grThree.setColorAt(0.05, Qt::black);
    m_graph->seriesList().at(2)->setBaseGradient(grThree);
    m_graph->seriesList().at(2)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);

    m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFront);

    connect(m_graph, &QAbstract3DGraph::selectedElementChanged,
            this, &CustomItemGraph::handleElementSelected);

    m_selectionAnimation = new QPropertyAnimation(this);
    m_selectionAnimation->setPropertyName("scaling");
    m_selectionAnimation->setDuration(500);
    m_selectionAnimation->setLoopCount(-1);

    QFont titleFont = QFont("Century Gothic", 30);
    titleFont.setBold(true);
    QCustom3DLabel *titleLabel = new QCustom3DLabel("Oil Rigs on Imaginary Sea", titleFont,
                                                    QVector3D(0.0f, 1.2f, 0.0f),
                                                    QVector3D(1.0f, 1.0f, 0.0f),
                                                    QQuaternion());
    titleLabel->setPositionAbsolute(true);
    titleLabel->setFacingCamera(true);
    titleLabel->setBackgroundColor(QColor(0x66cdaa));
    m_graph->addCustomItem(titleLabel);

    toggleItemOne(true);
    toggleItemTwo(true);
}

CustomItemGraph::~CustomItemGraph()
{
    delete m_graph;
}

void CustomItemGraph::toggleItemOne(bool show)
{
    //! [1]
    QVector3D positionOne = QVector3D(39.0f, 77.0f, 19.2f);
    //! [1]
    QVector3D positionOnePipe = QVector3D(39.0f, 45.0f, 19.2f);
    QVector3D positionOneLabel = QVector3D(39.0f, 107.0f, 19.2f);
    if (show) {
        //! [0]
        QImage color = QImage(2, 2, QImage::Format_RGB32);
        color.fill(Qt::red);
        //! [0]
        //! [2]
        QCustom3DItem *item = new QCustom3DItem(":/items/oilrig.obj", positionOne,
                                                QVector3D(0.025f, 0.025f, 0.025f),
                                                QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, 45.0f),
                                                color);
        //! [2]
        //! [3]
        m_graph->addCustomItem(item);
        //! [3]
        item = new QCustom3DItem(":/items/pipe.obj", positionOnePipe,
                                 QVector3D(0.005f, 0.5f, 0.005f),
                                 QQuaternion(),
                                 color);
        item->setShadowCasting(false);
        m_graph->addCustomItem(item);

        QCustom3DLabel *label = new QCustom3DLabel();
        label->setText("Oil Rig One");
        label->setPosition(positionOneLabel);
        label->setScaling(QVector3D(1.0f, 1.0f, 1.0f));
        m_graph->addCustomItem(label);
    } else {
        resetSelection();
        //! [4]
        m_graph->removeCustomItemAt(positionOne);
        //! [4]
        m_graph->removeCustomItemAt(positionOnePipe);
        m_graph->removeCustomItemAt(positionOneLabel);
    }
}

void CustomItemGraph::toggleItemTwo(bool show)
{
    QVector3D positionTwo = QVector3D(34.5f, 77.0f, 23.4f);
    QVector3D positionTwoPipe = QVector3D(34.5f, 45.0f, 23.4f);
    QVector3D positionTwoLabel = QVector3D(34.5f, 107.0f, 23.4f);
    if (show) {
        QImage color = QImage(2, 2, QImage::Format_RGB32);
        color.fill(Qt::red);
        QCustom3DItem *item = new QCustom3DItem();
        item->setMeshFile(":/items/oilrig.obj");
        item->setPosition(positionTwo);
        item->setScaling(QVector3D(0.025f, 0.025f, 0.025f));
        item->setRotation(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, 25.0f));
        item->setTextureImage(color);
        m_graph->addCustomItem(item);
        item = new QCustom3DItem(":/items/pipe.obj", positionTwoPipe,
                                 QVector3D(0.005f, 0.5f, 0.005f),
                                 QQuaternion(),
                                 color);
        item->setShadowCasting(false);
        m_graph->addCustomItem(item);

        QCustom3DLabel *label = new QCustom3DLabel();
        label->setText("Oil Rig Two");
        label->setPosition(positionTwoLabel);
        label->setScaling(QVector3D(1.0f, 1.0f, 1.0f));
        m_graph->addCustomItem(label);
    } else {
        resetSelection();
        m_graph->removeCustomItemAt(positionTwo);
        m_graph->removeCustomItemAt(positionTwoPipe);
        m_graph->removeCustomItemAt(positionTwoLabel);
    }
}

void CustomItemGraph::toggleItemThree(bool show)
{
    QVector3D positionThree = QVector3D(34.5f, 86.0f, 19.1f);
    QVector3D positionThreeLabel = QVector3D(34.5f, 116.0f, 19.1f);
    if (show) {
        QImage color = QImage(2, 2, QImage::Format_RGB32);
        color.fill(Qt::darkMagenta);
        QCustom3DItem *item = new QCustom3DItem();
        item->setMeshFile(":/items/refinery.obj");
        item->setPosition(positionThree);
        item->setScaling(QVector3D(0.04f, 0.04f, 0.04f));
        item->setRotation(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, 75.0f));
        item->setTextureImage(color);
        m_graph->addCustomItem(item);

        QCustom3DLabel *label = new QCustom3DLabel();
        label->setText("Refinery");
        label->setPosition(positionThreeLabel);
        label->setScaling(QVector3D(1.0f, 1.0f, 1.0f));
        m_graph->addCustomItem(label);
    } else {
        resetSelection();
        m_graph->removeCustomItemAt(positionThree);
        m_graph->removeCustomItemAt(positionThreeLabel);
    }
}

void CustomItemGraph::toggleSeeThrough(bool seethrough)
{
    if (seethrough) {
        m_graph->seriesList().at(0)->setDrawMode(QSurface3DSeries::DrawWireframe);
        m_graph->seriesList().at(1)->setDrawMode(QSurface3DSeries::DrawWireframe);
    } else {
        m_graph->seriesList().at(0)->setDrawMode(QSurface3DSeries::DrawSurface);
        m_graph->seriesList().at(1)->setDrawMode(QSurface3DSeries::DrawSurface);
    }
}

void CustomItemGraph::toggleOilHighlight(bool highlight)
{
    if (highlight) {
        QLinearGradient grThree;
        grThree.setColorAt(0.0, Qt::black);
        grThree.setColorAt(0.05, Qt::red);
        m_graph->seriesList().at(2)->setBaseGradient(grThree);
    } else {
        QLinearGradient grThree;
        grThree.setColorAt(0.0, Qt::white);
        grThree.setColorAt(0.05, Qt::black);
        m_graph->seriesList().at(2)->setBaseGradient(grThree);
    }
}

void CustomItemGraph::toggleShadows(bool shadows)
{
    if (shadows)
        m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualityMedium);
    else
        m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
}

void CustomItemGraph::handleElementSelected(QAbstract3DGraph::ElementType type)
{
    resetSelection();
    if (type == QAbstract3DGraph::ElementCustomItem) {
        QCustom3DItem *item = m_graph->selectedCustomItem();
        QString text;
        if (qobject_cast<QCustom3DLabel *>(item) != 0) {
            text.append("Custom label: ");
        } else {
            QStringList split = item->meshFile().split("/");
            text.append(split.last());
            text.append(": ");
        }
        int index = m_graph->selectedCustomItemIndex();
        text.append(QString::number(index));
        m_textField->setText(text);
        m_previouslyAnimatedItem = item;
        m_previousScaling = item->scaling();
        m_selectionAnimation->setTargetObject(item);
        m_selectionAnimation->setStartValue(item->scaling());
        m_selectionAnimation->setEndValue(item->scaling() * 1.5f);
        m_selectionAnimation->start();
    } else if (type == QAbstract3DGraph::ElementSeries) {
        QString text = "Surface (";
        QSurface3DSeries *series = m_graph->selectedSeries();
        if (series) {
            QPoint point = series->selectedPoint();
            QString posStr;
            posStr.setNum(point.x());
            text.append(posStr);
            text.append(", ");
            posStr.setNum(point.y());
            text.append(posStr);
        }
        text.append(")");
        m_textField->setText(text);
    } else if (type > QAbstract3DGraph::ElementSeries
               && type < QAbstract3DGraph::ElementCustomItem) {
        int index = m_graph->selectedLabelIndex();
        QString text;
        if (type == QAbstract3DGraph::ElementAxisXLabel)
            text.append("Axis X label: ");
        else if (type == QAbstract3DGraph::ElementAxisYLabel)
            text.append("Axis Y label: ");
        else
            text.append("Axis Z label: ");
        text.append(QString::number(index));
        m_textField->setText(text);
    } else {
        m_textField->setText("Nothing");
    }
}

void CustomItemGraph::resetSelection()
{
    m_selectionAnimation->stop();
    if (m_previouslyAnimatedItem)
        m_previouslyAnimatedItem->setScaling(m_previousScaling);
    m_previouslyAnimatedItem = 0;
}
