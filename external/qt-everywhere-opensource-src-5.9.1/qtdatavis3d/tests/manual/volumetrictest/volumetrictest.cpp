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

#include "volumetrictest.h"
#include <QtDataVisualization/qbar3dseries.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtDataVisualization/qcustom3dlabel.h>
#include <QtCore/qmath.h>
#include <QtGui/QRgb>
#include <QtGui/QImage>
#include <QtWidgets/QLabel>
#include <QtCore/QDebug>

using namespace QtDataVisualization;

const int imageCount = 512;
const float xMiddle = 100.0f;
const float yMiddle = 2.5f;
const float zMiddle = 100.0f;
const float xRange = 40.0f;
const float yRange = 7.5f;
const float zRange = 20.0f;

VolumetricModifier::VolumetricModifier(QAbstract3DGraph *scatter)
    : m_graph(scatter),
      m_volumeItem(0),
      m_volumeItem2(0),
      m_volumeItem3(0),
      m_sliceIndexX(0),
      m_sliceIndexY(0),
      m_sliceIndexZ(0)
{
    m_graph->activeTheme()->setType(Q3DTheme::ThemeQt);
    //m_graph->activeTheme()->setType(Q3DTheme::ThemeIsabelle);
    m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
    m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFront);
    m_graph->setOrthoProjection(true);
    //m_graph->scene()->activeCamera()->setTarget(QVector3D(-2.0f, 1.0f, 2.0f));
    m_scatterGraph = qobject_cast<Q3DScatter *>(m_graph);
    m_surfaceGraph = qobject_cast<Q3DSurface *>(m_graph);
    m_barGraph = qobject_cast<Q3DBars *>(m_graph);
    if (m_scatterGraph) {
        m_scatterGraph->axisX()->setRange(xMiddle - xRange, xMiddle + xRange);
        m_scatterGraph->axisX()->setSegmentCount(8);
        m_scatterGraph->axisY()->setRange(yMiddle - yRange, yMiddle + yRange);
        m_scatterGraph->axisY()->setSegmentCount(3);
        m_scatterGraph->axisZ()->setRange(zMiddle - zRange, zMiddle + zRange);
        m_scatterGraph->axisZ()->setSegmentCount(8);
    } else if (m_surfaceGraph) {
        m_surfaceGraph->axisX()->setRange(xMiddle - xRange, xMiddle + xRange);
        m_surfaceGraph->axisX()->setSegmentCount(8);
        m_surfaceGraph->axisY()->setRange(yMiddle - yRange, yMiddle + yRange);
        m_surfaceGraph->axisY()->setSegmentCount(3);
        m_surfaceGraph->axisZ()->setRange(zMiddle - zRange, zMiddle + zRange);
        m_surfaceGraph->axisZ()->setSegmentCount(8);
    } else if (m_barGraph) {
        QStringList rowLabels;
        QStringList columnLabels;
        for (int i = 0; i < xMiddle + xRange; i++) {
            if (i % 5 == 0)
                columnLabels << QString::number(i);
            else
                columnLabels << QString();
        }
        for (int i = 0; i < zMiddle + zRange; i++) {
            if (i % 5 == 0)
                rowLabels << QString::number(i);
            else
                rowLabels << QString();
        }

        QBar3DSeries *series = new QBar3DSeries;
        QBarDataArray *array = new QBarDataArray();
        array->reserve(zRange * 2 + 1);
        for (int i = 0; i < zRange * 2 + 1; i++)
            array->append(new QBarDataRow(xRange * 2 + 1));

        series->dataProxy()->resetArray(array, rowLabels, columnLabels);
        m_barGraph->addSeries(series);

        m_barGraph->columnAxis()->setRange(xMiddle - xRange, xMiddle + xRange);
        m_barGraph->valueAxis()->setRange(yMiddle - yRange, yMiddle + yRange);
        m_barGraph->rowAxis()->setRange(zMiddle - zRange, zMiddle + zRange);
        //m_barGraph->setReflection(true);
    }
    m_graph->activeTheme()->setBackgroundEnabled(false);

    createVolume();
    createAnotherVolume();
    createYetAnotherVolume();

//    m_volumeItem->setUseHighDefShader(false);
//    m_volumeItem2->setUseHighDefShader(false);
//    m_volumeItem3->setUseHighDefShader(false);

    m_volumeItem->setScalingAbsolute(false);
    m_volumeItem2->setScalingAbsolute(false);
    m_volumeItem3->setScalingAbsolute(false);
    m_volumeItem->setPositionAbsolute(false);
    m_volumeItem2->setPositionAbsolute(false);
    m_volumeItem3->setPositionAbsolute(false);


    m_plainItem = new QCustom3DItem;
    QImage texture(2, 2, QImage::Format_ARGB32);
    texture.fill(QColor(200, 200, 200, 130));
    m_plainItem->setMeshFile(QStringLiteral(":/mesh"));
    m_plainItem->setTextureImage(texture);
    m_plainItem->setRotation(m_volumeItem->rotation());
    m_plainItem->setPosition(QVector3D(xMiddle + xRange / 2.0f, yMiddle + yRange / 2.0f, zMiddle));
    m_plainItem->setScaling(QVector3D(20.0f, 5.0f, 10.0f));
    m_plainItem->setScalingAbsolute(false);

    m_graph->addCustomItem(m_volumeItem);
    m_graph->addCustomItem(m_volumeItem2);
    m_graph->addCustomItem(m_volumeItem3);
    m_graph->addCustomItem(m_plainItem);
    //m_graph->setMeasureFps(true);

    // Create label to cut through the volume 3
    QCustom3DLabel *label = new QCustom3DLabel;
    label->setText(QStringLiteral("FOO BAR - FOO BAR - FOO BAR"));
    QFont font;
    font.setPixelSize(100);
    label->setFont(font);
    label->setScaling(QVector3D(2.0f, 2.0f, 0.0f));
    label->setRotationAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), 45.0f);
    label->setPosition(m_volumeItem3->position());
    label->setPositionAbsolute(false);
    label->setScalingAbsolute(true);

    m_graph->addCustomItem(label);

    QObject::connect(m_graph, &QAbstract3DGraph::currentFpsChanged, this,
                     &VolumetricModifier::handleFpsChange);
//    QObject::connect(m_graph->scene(), &Q3DScene::viewportChanged, this,
//                     &VolumetricModifier::handleFpsChange);
}

VolumetricModifier::~VolumetricModifier()
{
    delete m_graph;
}

void VolumetricModifier::setFpsLabel(QLabel *fpsLabel)
{
    m_fpsLabel = fpsLabel;
}

void VolumetricModifier::sliceX(int enabled)
{
    m_volumeItem->setSliceIndexX(enabled ? m_sliceIndexX : -1);
    m_volumeItem2->setSliceIndexX(enabled ? m_sliceIndexX : -1);
}

void VolumetricModifier::sliceY(int enabled)
{
    m_volumeItem->setSliceIndexY(enabled ? m_sliceIndexY : -1);
    m_volumeItem2->setSliceIndexY(enabled ? m_sliceIndexY : -1);
}

void VolumetricModifier::sliceZ(int enabled)
{
    m_volumeItem->setSliceIndexZ(enabled ? m_sliceIndexZ : -1);
    m_volumeItem2->setSliceIndexZ(enabled ? m_sliceIndexZ : -1);
}

void VolumetricModifier::adjustSliceX(int value)
{
    m_sliceIndexX = int(float(value) / (1024.0f / m_volumeItem->textureWidth()));
    if (m_sliceIndexX == m_volumeItem->textureWidth())
        m_sliceIndexX--;
    qDebug() << "m_sliceIndexX:" << m_sliceIndexX;
    if (m_volumeItem->sliceIndexX() != -1) {
        m_volumeItem->setSliceIndexX(m_sliceIndexX);
        m_volumeItem2->setSliceIndexX(m_sliceIndexX);
    }
    m_sliceLabelX->setPixmap(QPixmap::fromImage(
                                 m_volumeItem->renderSlice(Qt::XAxis, m_sliceIndexX)));

}

void VolumetricModifier::adjustSliceY(int value)
{
    m_sliceIndexY = int(float(value) / (1024.0f / m_volumeItem->textureHeight()));
    if (m_sliceIndexY == m_volumeItem->textureHeight())
        m_sliceIndexY--;
    qDebug() << "m_sliceIndexY:" << m_sliceIndexY;
    if (m_volumeItem->sliceIndexY() != -1) {
        m_volumeItem->setSliceIndexY(m_sliceIndexY);
        m_volumeItem2->setSliceIndexY(m_sliceIndexY);
    }
    m_sliceLabelY->setPixmap(QPixmap::fromImage(
                                 m_volumeItem->renderSlice(Qt::YAxis, m_sliceIndexY)));
}

void VolumetricModifier::adjustSliceZ(int value)
{
    m_sliceIndexZ = int(float(value) / (1024.0f / m_volumeItem->textureDepth()));
    if (m_sliceIndexZ == m_volumeItem->textureDepth())
        m_sliceIndexZ--;
    qDebug() << "m_sliceIndexZ:" << m_sliceIndexZ;
    if (m_volumeItem->sliceIndexZ() != -1) {
        m_volumeItem->setSliceIndexZ(m_sliceIndexZ);
        m_volumeItem2->setSliceIndexZ(m_sliceIndexZ);
    }
    m_sliceLabelZ->setPixmap(QPixmap::fromImage(
                                 m_volumeItem->renderSlice(Qt::ZAxis, m_sliceIndexZ)));
}

void VolumetricModifier::handleFpsChange()
{
    const QString fpsFormat = QStringLiteral("Fps: %1");
    int fps10 = int(m_graph->currentFps() * 10.0);
    m_fpsLabel->setText(fpsFormat.arg(QString::number(qreal(fps10) / 10.0)));
//    const QString sceneDimensionsFormat = QStringLiteral("%1 x %2");
//    m_fpsLabel->setText(sceneDimensionsFormat
//                        .arg(m_graph->scene()->viewport().width())
    //                        .arg(m_graph->scene()->viewport().height()));
}

void VolumetricModifier::testSubtextureSetting()
{
    // Setting the rendered Slice as subtexture should result in identical volume
    QVector<uchar> dataBefore = *m_volumeItem->textureData();
    dataBefore[0] = dataBefore.at(0);    // Make sure we are detached

    checkRenderCase(1, Qt::XAxis, 56, dataBefore, m_volumeItem);
    checkRenderCase(2, Qt::YAxis, 64, dataBefore, m_volumeItem);
    checkRenderCase(3, Qt::ZAxis, 87, dataBefore, m_volumeItem);

    checkRenderCase(4, Qt::XAxis, 0, dataBefore, m_volumeItem);
    checkRenderCase(5, Qt::YAxis, 0, dataBefore, m_volumeItem);
    checkRenderCase(6, Qt::ZAxis, 0, dataBefore, m_volumeItem);

    checkRenderCase(7, Qt::XAxis, m_volumeItem->textureWidth() - 1, dataBefore, m_volumeItem);
    checkRenderCase(8, Qt::YAxis, m_volumeItem->textureHeight() - 1, dataBefore, m_volumeItem);
    checkRenderCase(9, Qt::ZAxis, m_volumeItem->textureDepth() - 1, dataBefore, m_volumeItem);

    dataBefore = *m_volumeItem2->textureData();
    dataBefore[0] = dataBefore.at(0);    // Make sure we are detached

    checkRenderCase(11, Qt::XAxis, 56, dataBefore, m_volumeItem2);
    checkRenderCase(12, Qt::YAxis, 64, dataBefore, m_volumeItem2);
    checkRenderCase(13, Qt::ZAxis, 87, dataBefore, m_volumeItem2);

    checkRenderCase(14, Qt::XAxis, 0, dataBefore, m_volumeItem2);
    checkRenderCase(15, Qt::YAxis, 0, dataBefore, m_volumeItem2);
    checkRenderCase(16, Qt::ZAxis, 0, dataBefore, m_volumeItem2);

    checkRenderCase(17, Qt::XAxis, m_volumeItem2->textureWidth() - 1, dataBefore, m_volumeItem2);
    checkRenderCase(18, Qt::YAxis, m_volumeItem2->textureHeight() - 1, dataBefore, m_volumeItem2);
    checkRenderCase(19, Qt::ZAxis, m_volumeItem2->textureDepth() - 1, dataBefore, m_volumeItem2);

    // Do some visible swaps on volume 3
    QImage slice = m_volumeItem3->renderSlice(Qt::XAxis, 144);
    slice = slice.mirrored();
    m_volumeItem3->setSubTextureData(Qt::XAxis, 144, slice);

    slice = m_volumeItem3->renderSlice(Qt::YAxis, 80);
    slice = slice.mirrored();
    m_volumeItem3->setSubTextureData(Qt::YAxis, 80, slice);

    slice = m_volumeItem3->renderSlice(Qt::ZAxis, 190);
    slice = slice.mirrored(true, false);
    m_volumeItem3->setSubTextureData(Qt::ZAxis, 190, slice);
}

void VolumetricModifier::adjustRangeX(int value)
{
    float adjustment = float(value - 512) / 10.0f;
    if (m_scatterGraph)
        m_scatterGraph->axisX()->setRange(xMiddle + adjustment - xRange, xMiddle + adjustment + xRange);
    if (m_surfaceGraph)
        m_surfaceGraph->axisX()->setRange(xMiddle + adjustment - xRange, xMiddle + adjustment + xRange);
    if (m_barGraph)
        m_barGraph->columnAxis()->setRange(xMiddle + adjustment - xRange, xMiddle + adjustment + xRange);
}

void VolumetricModifier::adjustRangeY(int value)
{
    float adjustment = float(value - 512) / 10.0f;
    if (m_scatterGraph)
        m_scatterGraph->axisY()->setRange(yMiddle + adjustment - yRange, yMiddle + adjustment + yRange);
    if (m_surfaceGraph)
        m_surfaceGraph->axisY()->setRange(yMiddle + adjustment - yRange, yMiddle + adjustment + yRange);
    if (m_barGraph)
        m_barGraph->valueAxis()->setRange(yMiddle + adjustment - yRange, yMiddle + adjustment + yRange);
}

void VolumetricModifier::adjustRangeZ(int value)
{
    float adjustment = float(value - 512) / 10.0f;
    if (m_scatterGraph)
        m_scatterGraph->axisZ()->setRange(zMiddle + adjustment - zRange, zMiddle + adjustment + zRange);
    if (m_surfaceGraph)
        m_surfaceGraph->axisZ()->setRange(zMiddle + adjustment - zRange, zMiddle + adjustment + zRange);
    if (m_barGraph)
        m_barGraph->rowAxis()->setRange(zMiddle + adjustment - zRange, zMiddle + adjustment + zRange);
}

void VolumetricModifier::testBoundsSetting()
{
    static QVector3D scaling1 = m_volumeItem->scaling();
    static QVector3D scaling2 = m_volumeItem2->scaling();
    static QVector3D scaling3 = m_volumeItem3->scaling();
    static QVector3D scaleVector = QVector3D(0.5f, 0.3f, 0.2f);

    if (m_volumeItem->isScalingAbsolute()) {
        m_volumeItem->setScalingAbsolute(false);
        m_volumeItem2->setScalingAbsolute(false);
        m_volumeItem3->setScalingAbsolute(false);

        m_volumeItem->setScaling(scaling1);
        m_volumeItem2->setScaling(scaling2);
        m_volumeItem3->setScaling(scaling3);
    } else {
        m_volumeItem->setScalingAbsolute(true);
        m_volumeItem2->setScalingAbsolute(true);
        m_volumeItem3->setScalingAbsolute(true);

        m_volumeItem->setScaling(scaleVector);
        m_volumeItem2->setScaling(scaleVector);
        m_volumeItem3->setScaling(scaleVector);
    }
}

void VolumetricModifier::checkRenderCase(int id, Qt::Axis axis, int index,
                                         const QVector<uchar> &dataBefore,
                                         QCustom3DVolume *volumeItem)
{
    QImage slice = volumeItem->renderSlice(axis, index);
    volumeItem->setSubTextureData(axis, index, slice);

    if (dataBefore == *volumeItem->textureData())
        qDebug() << __FUNCTION__ << "Case:" << id << "Vectors identical";
    else
        qDebug() << __FUNCTION__ << "Case:" << id << "BUG: VECTORS DIFFER!";
}

void VolumetricModifier::createVolume()
{
    m_volumeItem = new QCustom3DVolume;
    m_volumeItem->setTextureFormat(QImage::Format_ARGB32);
    m_volumeItem->setRotation(QQuaternion::fromAxisAndAngle(1.0f, 1.0f, 0.0f, 20.0f));
    m_volumeItem->setPosition(QVector3D(xMiddle - (xRange / 2.0f), yMiddle + (yRange / 2.0f), zMiddle));
    //m_volumeItem->setPosition(QVector3D(xMiddle, yMiddle, zMiddle));
    m_volumeItem->setDrawSliceFrames(true);
    m_volumeItem->setDrawSlices(true);

    QImage logo;
    logo.load(QStringLiteral(":/logo_no_padding.png"));
    //logo.load(QStringLiteral(":/logo.png"));
    qDebug() << "image dimensions:" << logo.width() << logo.height()
             << logo.byteCount() << (logo.width() * logo.height())
             << logo.bytesPerLine();

    QVector<QImage *> imageArray(imageCount);
    for (int i = 0; i < imageCount; i++) {
        QImage *newImage = new QImage(logo);
        imageArray[i] = newImage;
    }

    m_volumeItem->createTextureData(imageArray);

    for (int i = 0; i < imageCount; i++)
        delete imageArray[i];

    m_sliceIndexX = m_volumeItem->textureWidth() / 2;
    m_sliceIndexY = m_volumeItem->textureWidth() / 2;
    m_sliceIndexZ = m_volumeItem->textureWidth() / 2;

    QVector<QRgb> colorTable = m_volumeItem->colorTable();

    // Hack some alpha to the picture
    for (int i = 0; i < colorTable.size(); i++) {
        if (qAlpha(colorTable.at(i)) > 0) {
            colorTable[i] = qRgba(qRed(colorTable.at(i)),
                                  qGreen(colorTable.at(i)),
                                  qBlue(colorTable.at(i)),
                                  50);
        }
    }

    colorTable.append(qRgba(0, 0, 0, 0));
    colorTable.append(qRgba(255, 0, 0, 255));
    colorTable.append(qRgba(0, 255, 0, 255));
    colorTable.append(qRgba(0, 0, 255, 255));

    m_volumeItem->setColorTable(colorTable);
    int alphaIndex = colorTable.size() - 4;
    int redIndex = colorTable.size() - 3;
    int greenIndex = colorTable.size() - 2;
    int blueIndex = colorTable.size() - 1;
    int width = m_volumeItem->textureDataWidth();
    int height = m_volumeItem->textureHeight();
    int depth = m_volumeItem->textureDepth();
    int frameSize = width * height;
    qDebug() << width << height << depth << m_volumeItem->textureData()->size();
    m_volumeItem->setScaling(QVector3D(xRange, yRange, zRange) / 2.0f);

    uchar *data = m_volumeItem->textureData()->data();
    uchar *p = data;

    // Change one picture using subtexture replacement
    QImage flipped = logo.mirrored();
    m_volumeItem->setSubTextureData(Qt::ZAxis, 100, flipped);

    // Clean up the two extra pixels
    p = data + width - 1;
    for (int k = 0; k < depth; k++) {
        for (int j = 0; j < height; j++) {
            *p = alphaIndex;
            p += width;
        }
    }
    p = data + width - 2;
    for (int k = 0; k < depth; k++) {
        for (int j = 0; j < height; j++) {
            *p = alphaIndex;
            p += width;
        }
    }
    // Red first subtexture
    p = data;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            *p = redIndex;
            p++;
        }
    }

    // Red last subtexture
//    p = data + frameSize * (imageCount - 1);
//    for (int j = 0; j < height; j++) {
//        for (int i = 0; i < width; i++) {
//            *p = redIndex;
//            p++;
//        }
//    }

//    // Blue second to last subtexture
//    p = data + frameSize * (imageCount - 2);
//    for (int j = 0; j < height; j++) {
//        for (int i = 0; i < width; i++) {
//            *p = blueIndex;
//            p++;
//        }
//    }

    // Blue x = 0
//    p = data;
//    for (int k = 0; k < depth; k++) {
//        for (int j = 0; j < height; j++) {
//            *p = blueIndex;
//            p += width;
//        }
//    }

    // Blue x = max
    p = data + width - 1;
    for (int k = 0; k < depth; k++) {
        for (int j = 0; j < height; j++) {
            *p = blueIndex;
            p += width;
        }
    }
    // green x = max - 1
    p = data + width - 2;
    for (int k = 0; k < depth; k++) {
        for (int j = 0; j < height; j++) {
            *p = greenIndex;
            p += width;
        }
    }

    // Green y = 0
    p = data;
    for (int k = 0; k < depth; k++) {
        for (int i = 0; i < width; i++) {
            *p = greenIndex;
            p++;
        }
        p += (frameSize - width);
    }

    // Green y = max
//    p = data + frameSize - width;
//    for (int k = 0; k < depth; k++) {
//        for (int i = 0; i < width; i++) {
//            *p = greenIndex;
//            p++;
//        }
//        p += (frameSize - width);
//    }
}

void VolumetricModifier::createAnotherVolume()
{
    m_volumeItem2 = new QCustom3DVolume;
    m_volumeItem2->setTextureFormat(QImage::Format_ARGB32);
    m_volumeItem2->setPosition(QVector3D(xMiddle + (xRange / 2.0f), yMiddle - (yRange / 2.0f), zMiddle));

    QImage logo;
    logo.load(QStringLiteral(":/logo_no_padding.png"));
    //logo.load(QStringLiteral(":/logo.png"));
    logo = logo.convertToFormat(QImage::Format_ARGB8555_Premultiplied);
    qDebug() << "second image dimensions:" << logo.width() << logo.height()
             << logo.byteCount() << (logo.width() * logo.height())
             << logo.bytesPerLine();

    logo.save("d:/qt/goobar.png");

    QVector<QImage *> imageArray(imageCount);
    for (int i = 0; i < imageCount; i++) {
        QImage *newImage = new QImage(logo);
        imageArray[i] = newImage;
    }

    m_volumeItem2->createTextureData(imageArray);

    for (int i = 0; i < imageCount; i++)
        delete imageArray[i];

    m_sliceIndexX = m_volumeItem2->textureWidth() / 2;
    m_sliceIndexY = m_volumeItem2->textureWidth() / 2;
    m_sliceIndexZ = m_volumeItem2->textureWidth() / 2;

    int width = m_volumeItem2->textureWidth();
    int height = m_volumeItem2->textureHeight();
    int depth = m_volumeItem2->textureDepth();
    qDebug() << width << height << depth << m_volumeItem2->textureData()->size();
    m_volumeItem2->setScaling(QVector3D(float(width) / float(depth) * xRange * 2.0f,
                                       float(height) / float(depth) * yRange * 2.0f,
                                       zRange * 2.0f));

    // Change one picture using subtexture replacement
    QImage flipped = logo.mirrored();
    m_volumeItem2->setSubTextureData(Qt::ZAxis, 100, flipped);
    //m_volumeItem2->setAlphaMultiplier(0.2f);
    m_volumeItem2->setPreserveOpacity(false);
}

void VolumetricModifier::createYetAnotherVolume()
{
    m_volumeItem3 = new QCustom3DVolume;
    m_volumeItem3->setTextureFormat(QImage::Format_Indexed8);
//    m_volumeItem2->setRotation(QQuaternion::fromAxisAndAngle(1.0f, 1.0f, 0.0f, 10.0f));
    m_volumeItem3->setPosition(QVector3D(xMiddle - (xRange / 2.0f), yMiddle - (yRange / 2.0f), zMiddle));

//    m_volumeItem3->setTextureDimensions(m_volumeItem->textureDataWidth(),
//                                        m_volumeItem->textureHeight(),
//                                        m_volumeItem->textureDepth());
    m_volumeItem3->setTextureDimensions(200, 200, 200);

    QVector<uchar> *tdata = new QVector<uchar>(m_volumeItem3->textureDataWidth()
                                               * m_volumeItem3->textureHeight()
                                               * m_volumeItem3->textureDepth());
    tdata->fill(0);
    m_volumeItem3->setTextureData(tdata);

    m_sliceIndexX = m_volumeItem3->textureWidth() / 2;
    m_sliceIndexY = m_volumeItem3->textureWidth() / 2;
    m_sliceIndexZ = m_volumeItem3->textureWidth() / 2;

    QVector<QRgb> colorTable = m_volumeItem->colorTable();
    colorTable[0] = qRgba(0, 0, 0, 0);
    m_volumeItem3->setColorTable(colorTable);
    int redIndex = colorTable.size() - 3;
    int greenIndex = colorTable.size() - 2;
    int blueIndex = colorTable.size() - 1;
    int width = m_volumeItem3->textureDataWidth();
    int height = m_volumeItem3->textureHeight();
    int depth = m_volumeItem3->textureDepth();
    int frameSize = width * height;
    qDebug() << width << height << depth << m_volumeItem3->textureData()->size();
    m_volumeItem3->setScaling(m_volumeItem->scaling());

    uchar *data = tdata->data();
    uchar *p = data;

    // Red first subtexture
//    for (int j = 0; j < height; j++) {
//        for (int i = 0; i < width; i++) {
//            *p = redIndex;
//            p++;
//        }
//    }

    // Red last subtexture
    p = data + frameSize * (depth - 1);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            *p = redIndex;
            p++;
        }
    }

    // green edge
    p = data + frameSize * (depth - 1);
    for (int i = 0; i < width; i++) {
        *p = greenIndex;
        p++;
    }
    for (int j = 2; j < height; j++) {
        *p = greenIndex;
        p += width - 1;
        *p = j % 7 ? greenIndex : blueIndex;
        p++;
    }
    for (int i = 0; i < width; i++) {
        *p = greenIndex;
        p++;
    }

//    // Blue second to last subtexture
//    p = data + frameSize * (depth - 2);
//    for (int j = 0; j < height; j++) {
//        for (int i = 0; i < width; i++) {
//            *p = blueIndex;
//            p++;
//        }
//    }

//    // green third to last subtexture
//    p = data + frameSize * (depth - 3);
//    for (int j = 0; j < height; j++) {
//        for (int i = 0; i < width; i++) {
//            *p = greenIndex;
//            p++;
//        }
//    }

    // Blue x = 0
    p = data;
    for (int k = 0; k < depth; k++) {
        for (int j = 0; j < height; j++) {
            *p = blueIndex;
            p += width;
        }
    }

//    // Blue x = max
//    p = data + width - 1;
//    for (int k = 0; k < depth; k++) {
//        for (int j = 0; j < height; j++) {
//            *p = blueIndex;
//            p += width;
//        }
//    }

    // Green y = 0
//    p = data;
//    for (int k = 0; k < depth; k++) {
//        for (int i = 0; i < width; i++) {
//            *p = greenIndex;
//            p++;
//        }
//        p += (frameSize - width);
//    }

//    // Green y = max
    p = data + frameSize - width;
    for (int k = 0; k < depth; k++) {
        for (int i = 0; i < width; i++) {
            *p = greenIndex;
            p++;
        }
        p += (frameSize - width);
    }


//    // Fill with alternating pixels
//    p = data;
//    for (int k = 0; k < (depth * width * height / 4); k++) {
//        *p = greenIndex;
//        p++;
//        *p = greenIndex;
//        p++;
//        *p = blueIndex;
//        p++;
//        *p = redIndex;
//        p++;
//    }


}

void VolumetricModifier::setSliceLabels(QLabel *xLabel, QLabel *yLabel, QLabel *zLabel)
{
    m_sliceLabelX = xLabel;
    m_sliceLabelY = yLabel;
    m_sliceLabelZ = zLabel;

    adjustSliceX(512);
    adjustSliceY(512);
    adjustSliceZ(512);
}
