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

#ifndef VOLUMETRICMODIFIER_H
#define VOLUMETRICMODIFIER_H

#include <QtDataVisualization/q3dscatter.h>
#include <QtDataVisualization/qcustom3dvolume.h>
#include <QtCore/QTimer>
#include <QtGui/QRgb>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QRadioButton>

using namespace QtDataVisualization;

class VolumetricModifier : public QObject
{
    Q_OBJECT
public:
    explicit VolumetricModifier(Q3DScatter *scatter);
    ~VolumetricModifier();

    void setFpsLabel(QLabel *fpsLabel);
    void setMediumDetailRB(QRadioButton *button);
    void setHighDetailRB(QRadioButton *button);
    void setSliceLabels(QLabel *xLabel, QLabel *yLabel, QLabel *zLabel);
    void setAlphaMultiplierLabel(QLabel *label);

public Q_SLOTS:
    void sliceX(int enabled);
    void sliceY(int enabled);
    void sliceZ(int enabled);
    void adjustSliceX(int value);
    void adjustSliceY(int value);
    void adjustSliceZ(int value);
    void handleFpsChange(qreal fps);
    void handleTimeout();
    void toggleLowDetail(bool enabled);
    void toggleMediumDetail(bool enabled);
    void toggleHighDetail(bool enabled);
    void setFpsMeasurement(bool enabled);
    void setSliceSliders(QSlider *sliderX, QSlider *sliderY, QSlider *sliderZ);
    void changeColorTable(int enabled);
    void setPreserveOpacity(bool enabled);
    void setTransparentGround(bool enabled);
    void setUseHighDefShader(bool enabled);
    void adjustAlphaMultiplier(int value);
    void toggleAreaAll(bool enabled);
    void toggleAreaMine(bool enabled);
    void toggleAreaMountain(bool enabled);
    void setDrawSliceFrames(int enabled);

private:

    void initHeightMap(QString fileName, QVector<uchar> &layerData);
    void initMineShaftArray();
    int createVolume(int textureSize, int startIndex, int count,
                      QVector<uchar> *textureData);
    int excavateMineShaft(int textureSize, int startIndex, int count,
                          QVector<uchar> *textureData);
    void excavateMineBlock(int textureSize, int dataIndex, int size, QVector<uchar> *textureData);
    void handleSlicingChanges();

    Q3DScatter *m_graph;
    QCustom3DVolume *m_volumeItem;
    int m_sliceIndexX;
    int m_sliceIndexY;
    int m_sliceIndexZ;
    bool m_slicingX;
    bool m_slicingY;
    bool m_slicingZ;
    QLabel *m_fpsLabel;
    QRadioButton *m_mediumDetailRB;
    QRadioButton *m_highDetailRB;
    QVector<uchar> *m_lowDetailData;
    QVector<uchar> *m_mediumDetailData;
    QVector<uchar> *m_highDetailData;
    QTimer m_timer;
    int m_mediumDetailIndex;
    int m_highDetailIndex;
    int m_mediumDetailShaftIndex;
    int m_highDetailShaftIndex;
    QSlider *m_sliceSliderX;
    QSlider *m_sliceSliderY;
    QSlider *m_sliceSliderZ;
    QVector<QRgb> m_colorTable1;
    QVector<QRgb> m_colorTable2;
    bool m_usingPrimaryTable;
    QLabel *m_sliceLabelX;
    QLabel *m_sliceLabelY;
    QLabel *m_sliceLabelZ;
    QLabel *m_alphaMultiplierLabel;
    QVector<uchar> m_magmaLayer;
    QVector<uchar> m_waterLayer;
    QVector<uchar> m_groundLayer;
    QVector<QPair<QVector3D, QVector3D> > m_mineShaftArray;
};

#endif
